// Copyright 2021 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// tests runs tint against a number of test shaders checking for expected behavior
package main

import (
	"context"
	"crypto/sha256"
	"encoding/json"
	"flag"
	"fmt"
	"io/ioutil"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"regexp"
	"runtime"
	"sort"
	"strings"
	"time"
	"unicode/utf8"

	"dawn.googlesource.com/dawn/tools/src/fileutils"
	"dawn.googlesource.com/dawn/tools/src/glob"
	"dawn.googlesource.com/dawn/tools/src/match"
	"dawn.googlesource.com/dawn/tools/src/transform"
	"github.com/fatih/color"
	"github.com/sergi/go-diff/diffmatchpatch"
	"golang.org/x/term"
)

type outputFormat string

const (
	testTimeout = 2 * time.Minute

	glsl    = outputFormat("glsl")
	hlslFXC = outputFormat("hlsl-fxc")
	hlslDXC = outputFormat("hlsl-dxc")
	msl     = outputFormat("msl")
	spvasm  = outputFormat("spvasm")
	wgsl    = outputFormat("wgsl")
)

// The root directory of the dawn project
var dawnRoot = fileutils.DawnRoot()

// The default non-flag arguments to the command
var defaultArgs = []string{"test/tint"}

// The globs automatically appended if a glob argument is a directory
var directoryGlobs = []string{
	"**.wgsl",
	"**.spvasm",
	"**.spv",
}

// Directories we don't generate expected PASS result files for.
// These directories contain large corpora of tests for which the generated code
// is uninteresting.
// These paths use unix-style slashes and do not contain the '/test/tint' prefix.
var dirsWithNoPassExpectations = []string{
	filepath.ToSlash(dawnRoot) + "/test/tint/benchmark/",
	filepath.ToSlash(dawnRoot) + "/test/tint/unittest/",
	filepath.ToSlash(dawnRoot) + "/test/tint/vk-gl-cts/",
}

func main() {
	if err := run(); err != nil {
		fmt.Println(err)
		os.Exit(1)
	}
}

func showUsage() {
	fmt.Printf(`
tests runs tint against a number of test shaders checking for expected behavior

usage:
  tests [flags...] [globs...]

  [globs]  a list of project-root relative file globs, directory or file paths
           of test cases.
           A file path will be added to the test list.
           A directory will automatically expand to the globs:
                %v
           Globs will include all test files that match the glob, but exclude
		   those that match the --ignore flag.
           If omitted, defaults to: %v

optional flags:`,
		transform.SliceNoErr(directoryGlobs, func(in string) string { return fmt.Sprintf("'<dir>/%v'", in) }),
		transform.SliceNoErr(defaultArgs, func(in string) string { return fmt.Sprintf("'%v'", in) }))
	flag.PrintDefaults()
	fmt.Println(``)
	os.Exit(1)
}

func run() error {
	terminalWidth, _, err := term.GetSize(int(os.Stdout.Fd()))
	if err != nil {
		terminalWidth = 0
	}

	var formatList, ignore, dxcPath, fxcPath, tintPath, xcrunPath string
	var maxTableWidth int
	numCPU := runtime.NumCPU()
	verbose, useIr, generateExpected, generateSkip := false, false, false, false
	flag.StringVar(&formatList, "format", "all", "comma separated list of formats to emit. Possible values are: all, wgsl, spvasm, msl, hlsl, hlsl-dxc, hlsl-fxc, glsl")
	flag.StringVar(&ignore, "ignore", "**.expected.*", "files to ignore in globs")
	flag.StringVar(&dxcPath, "dxc", "", "path to DXC executable for validating HLSL output")
	flag.StringVar(&fxcPath, "fxc", "", "path to FXC DLL for validating HLSL output")
	flag.StringVar(&tintPath, "tint", defaultTintPath(), "path to the tint executable")
	flag.StringVar(&xcrunPath, "xcrun", "", "path to xcrun executable for validating MSL output")
	flag.BoolVar(&verbose, "verbose", false, "print all run tests, including rows that all pass")
	flag.BoolVar(&useIr, "use-ir", false, "generate with the IR enabled")
	flag.BoolVar(&generateExpected, "generate-expected", false, "create or update all expected outputs")
	flag.BoolVar(&generateSkip, "generate-skip", false, "create or update all expected outputs that fail with SKIP")
	flag.IntVar(&numCPU, "j", numCPU, "maximum number of concurrent threads to run tests")
	flag.IntVar(&maxTableWidth, "table-width", terminalWidth, "maximum width of the results table")
	flag.Usage = showUsage
	flag.Parse()

	// Check the executable can be found and actually is executable
	if !fileutils.IsExe(tintPath) {
		fmt.Fprintln(os.Stderr, "tint executable not found, please specify with --tint")
		showUsage()
	}

	// Apply default args, if not provided
	args := flag.Args()
	if len(args) == 0 {
		args = defaultArgs
	}

	filePredicate := func(s string) bool { return true }
	if m, err := match.New(ignore); err == nil {
		filePredicate = func(s string) bool { return !m(s) }
	} else {
		return fmt.Errorf("failed to parse --ignore: %w", err)
	}

	// Transform args to globs, find the rootPath directory
	absFiles := []string{}
	rootPath := ""
	globs := []string{}
	for _, arg := range args {
		// Make absolute
		if !filepath.IsAbs(arg) {
			arg = filepath.Join(dawnRoot, arg)
		}

		switch {
		case fileutils.IsDir(arg):
			// Argument is to a directory, expand out to N globs
			for _, glob := range directoryGlobs {
				globs = append(globs, path.Join(arg, glob))
			}
		case fileutils.IsFile(arg):
			// Argument is a file, append to absFiles
			absFiles = append(absFiles, arg)
		default:
			globs = append(globs, arg)
		}

		if rootPath == "" {
			rootPath = filepath.Dir(arg)
		} else {
			rootPath = fileutils.CommonRootDir(rootPath, arg)
		}
	}

	// Glob the absFiles to test
	for _, g := range globs {
		globFiles, err := glob.Glob(g)
		if err != nil {
			return fmt.Errorf("Failed to glob files: %w", err)
		}
		filtered := transform.Filter(globFiles, filePredicate)
		normalized := transform.SliceNoErr(filtered, filepath.ToSlash)
		absFiles = append(absFiles, normalized...)
	}

	// Ensure the files are sorted (globbing should do this, but why not)
	sort.Strings(absFiles)

	// Parse --format into a list of outputFormat
	formats := []outputFormat{}
	if formatList == "all" {
		formats = []outputFormat{wgsl, spvasm, msl, hlslDXC, hlslFXC, glsl}
	} else {
		for _, f := range strings.Split(formatList, ",") {
			switch strings.TrimSpace(f) {
			case "wgsl":
				formats = append(formats, wgsl)
			case "spvasm":
				formats = append(formats, spvasm)
			case "msl":
				formats = append(formats, msl)
			case "hlsl":
				formats = append(formats, hlslDXC, hlslFXC)
			case "hlsl-dxc":
				formats = append(formats, hlslDXC)
			case "hlsl-fxc":
				formats = append(formats, hlslFXC)
			case "glsl":
				formats = append(formats, glsl)
			default:
				return fmt.Errorf("unknown format '%s'", f)
			}
		}
	}

	defaultMSLExe := "xcrun"
	if runtime.GOOS == "windows" {
		defaultMSLExe = "metal.exe"
	}

	toolchainHash := sha256.New()

	// If explicit verification compilers have been specified, check they exist.
	// Otherwise, look on PATH for them, but don't error if they cannot be found.
	for _, tool := range []struct {
		name string
		lang string
		path *string
	}{
		{"dxc", "hlsl-dxc", &dxcPath},
		{"d3dcompiler_47.dll", "hlsl-fxc", &fxcPath},
		{defaultMSLExe, "msl", &xcrunPath},
	} {
		if *tool.path == "" {
			// Look first in the directory of the tint executable
			p, err := exec.LookPath(filepath.Join(filepath.Dir(tintPath), tool.name))
			if err == nil && fileutils.IsExe(p) {
				*tool.path = p
			} else {
				// Look in PATH
				p, err := exec.LookPath(tool.name)
				if err == nil && fileutils.IsExe(p) {
					*tool.path = p
				}
			}
		} else if !fileutils.IsExe(*tool.path) {
			return fmt.Errorf("%v not found at '%v'", tool.name, *tool.path)
		}

		color.Set(color.FgCyan)
		fmt.Printf("%-8s", tool.lang)
		color.Unset()
		fmt.Printf(" validation ")
		if *tool.path != "" {
			fmt.Printf("ENABLED (" + *tool.path + ")")
		} else {
			color.Set(color.FgRed)
			fmt.Printf("DISABLED")
		}
		color.Unset()
		fmt.Println()

		toolchainHash.Write([]byte(tool.name))
		if s, err := os.Stat(*tool.path); err == nil {
			toolchainHash.Write([]byte(s.ModTime().String()))
			toolchainHash.Write([]byte(fmt.Sprint(s.Size())))
		}
	}
	fmt.Println()

	validationCache := loadValidationCache(fmt.Sprintf("%x", toolchainHash.Sum(nil)))
	defer saveValidationCache(validationCache)

	// Build the list of results.
	// These hold the chans used to report the job results.
	results := make([]map[outputFormat]chan status, len(absFiles))
	for i := range absFiles {
		fileResults := map[outputFormat]chan status{}
		for _, format := range formats {
			fileResults[format] = make(chan status, 1)
		}
		results[i] = fileResults
	}

	pendingJobs := make(chan job, 256)

	// Spawn numCPU job runners...
	runCfg := runConfig{
		tintPath:         tintPath,
		dxcPath:          dxcPath,
		fxcPath:          fxcPath,
		xcrunPath:        xcrunPath,
		useIr:            useIr,
		generateExpected: generateExpected,
		generateSkip:     generateSkip,
		validationCache:  validationCache,
	}
	for cpu := 0; cpu < numCPU; cpu++ {
		go func() {
			for job := range pendingJobs {
				job.run(runCfg)
			}
		}()
	}

	// Issue the jobs...
	go func() {
		for i, file := range absFiles { // For each test file...
			flags := parseFlags(file)
			for _, format := range formats { // For each output format...
				pendingJobs <- job{
					file:   file,
					flags:  flags,
					format: format,
					result: results[i][format],
				}
			}
		}
		close(pendingJobs)
	}()

	type failure struct {
		file   string
		format outputFormat
		err    error
	}

	type stats struct {
		numTests, numPass, numSkip, numFail int
		timeTaken                           time.Duration
	}

	// Statistics per output format
	statsByFmt := map[outputFormat]*stats{}
	for _, format := range formats {
		statsByFmt[format] = &stats{}
	}

	// Make file paths relative to rootPath, if possible
	relFiles := transform.GoSliceNoErr(absFiles, func(path string) string {
		path = filepath.ToSlash(path) // Normalize
		if rel, err := filepath.Rel(rootPath, path); err == nil {
			return rel
		}
		return path
	})

	// Print the table of file x format and gather per-format stats
	failures := []failure{}
	filenameColumnWidth := maxStringLen(relFiles)

	// Calculate the table width
	tableWidth := filenameColumnWidth + 3
	for _, format := range formats {
		tableWidth += formatWidth(format) + 3
	}

	// Reduce filename column width if too big
	if tableWidth > maxTableWidth {
		filenameColumnWidth -= tableWidth - maxTableWidth
		if filenameColumnWidth < 20 {
			filenameColumnWidth = 20
		}
	}

	red := color.New(color.FgRed)
	green := color.New(color.FgGreen)
	yellow := color.New(color.FgYellow)
	cyan := color.New(color.FgCyan)

	printFormatsHeader := func() {
		fmt.Printf(strings.Repeat(" ", filenameColumnWidth))
		fmt.Printf(" ┃ ")
		for _, format := range formats {
			cyan.Printf(alignCenter(format, formatWidth(format)))
			fmt.Printf(" │ ")
		}
		fmt.Println()
	}
	printHorizontalLine := func() {
		fmt.Printf(strings.Repeat("━", filenameColumnWidth))
		fmt.Printf("━╋━")
		for _, format := range formats {
			fmt.Printf(strings.Repeat("━", formatWidth(format)))
			fmt.Printf("━┿━")
		}
		fmt.Println()
	}

	fmt.Println()

	printFormatsHeader()
	printHorizontalLine()

	newKnownGood := knownGoodHashes{}

	for i, file := range relFiles {
		results := results[i]

		row := &strings.Builder{}
		rowAllPassed := true

		filenameLength := utf8.RuneCountInString(file)
		shortFile := file
		if filenameLength > filenameColumnWidth {
			shortFile = "..." + file[filenameLength-filenameColumnWidth+3:]
		}

		fmt.Fprintf(row, alignRight(shortFile, filenameColumnWidth))
		fmt.Fprintf(row, " ┃ ")
		for _, format := range formats {
			columnWidth := formatWidth(format)
			result := <-results[format]

			// Update the known-good hashes
			newKnownGood[fileAndFormat{file, format}] = result.passHashes

			// Update stats
			stats := statsByFmt[format]
			stats.numTests++
			stats.timeTaken += result.timeTaken
			if err := result.err; err != nil {
				failures = append(failures, failure{
					file: file, format: format, err: err,
				})
			}

			switch result.code {
			case pass:
				green.Fprintf(row, alignCenter("PASS", columnWidth))
				stats.numPass++
			case fail:
				red.Fprintf(row, alignCenter("FAIL", columnWidth))
				rowAllPassed = false
				stats.numFail++
			case skip:
				yellow.Fprintf(row, alignCenter("SKIP", columnWidth))
				rowAllPassed = false
				stats.numSkip++
			default:
				fmt.Fprintf(row, alignCenter(result.code, columnWidth))
				rowAllPassed = false
			}
			fmt.Fprintf(row, " │ ")
		}

		if verbose || !rowAllPassed {
			fmt.Fprintln(color.Output, row)
		}
	}

	// Update the validation cache known-good hashes.
	// This has to be done after all the results have been collected to avoid
	// concurrent access on the map.
	for ff, hashes := range newKnownGood {
		if len(newKnownGood) > 0 {
			validationCache.knownGood[ff] = hashes
		} else {
			delete(validationCache.knownGood, ff)
		}
	}

	printHorizontalLine()
	printFormatsHeader()
	printHorizontalLine()
	printStat := func(col *color.Color, name string, num func(*stats) int) {
		row := &strings.Builder{}
		anyNonZero := false
		for _, format := range formats {
			columnWidth := formatWidth(format)
			count := num(statsByFmt[format])
			if count > 0 {
				col.Fprintf(row, alignLeft(count, columnWidth))
				anyNonZero = true
			} else {
				fmt.Fprintf(row, alignLeft(count, columnWidth))
			}
			fmt.Fprintf(row, " │ ")
		}

		if !anyNonZero {
			return
		}
		col.Printf(alignRight(name, filenameColumnWidth))
		fmt.Printf(" ┃ ")
		fmt.Fprintln(color.Output, row)

		col.Printf(strings.Repeat(" ", filenameColumnWidth))
		fmt.Printf(" ┃ ")
		for _, format := range formats {
			columnWidth := formatWidth(format)
			stats := statsByFmt[format]
			count := num(stats)
			percent := percentage(count, stats.numTests)
			if count > 0 {
				col.Print(alignRight(percent, columnWidth))
			} else {
				fmt.Print(alignRight(percent, columnWidth))
			}
			fmt.Printf(" │ ")
		}
		fmt.Println()
	}
	printStat(green, "PASS", func(s *stats) int { return s.numPass })
	printStat(yellow, "SKIP", func(s *stats) int { return s.numSkip })
	printStat(red, "FAIL", func(s *stats) int { return s.numFail })

	cyan.Printf(alignRight("TIME", filenameColumnWidth))
	fmt.Printf(" ┃ ")
	for _, format := range formats {
		timeTaken := printDuration(statsByFmt[format].timeTaken)
		cyan.Printf(alignLeft(timeTaken, formatWidth(format)))
		fmt.Printf(" │ ")
	}
	fmt.Println()

	for _, f := range failures {
		color.Set(color.FgBlue)
		fmt.Printf("%s ", f.file)
		color.Set(color.FgCyan)
		fmt.Printf("%s ", f.format)
		color.Set(color.FgRed)
		fmt.Println("FAIL")
		color.Unset()
		fmt.Println(indent(f.err.Error(), 4))
	}
	if len(failures) > 0 {
		fmt.Println()
	}

	allStats := stats{}
	for _, format := range formats {
		stats := statsByFmt[format]
		allStats.numTests += stats.numTests
		allStats.numPass += stats.numPass
		allStats.numSkip += stats.numSkip
		allStats.numFail += stats.numFail
	}

	fmt.Printf("%d tests run", allStats.numTests)
	if allStats.numPass > 0 {
		fmt.Printf(", ")
		color.Set(color.FgGreen)
		fmt.Printf("%d tests pass", allStats.numPass)
		color.Unset()
	} else {
		fmt.Printf(", %d tests pass", allStats.numPass)
	}
	if allStats.numSkip > 0 {
		fmt.Printf(", ")
		color.Set(color.FgYellow)
		fmt.Printf("%d tests skipped", allStats.numSkip)
		color.Unset()
	} else {
		fmt.Printf(", %d tests skipped", allStats.numSkip)
	}
	if allStats.numFail > 0 {
		fmt.Printf(", ")
		color.Set(color.FgRed)
		fmt.Printf("%d tests failed", allStats.numFail)
		color.Unset()
	} else {
		fmt.Printf(", %d tests failed", allStats.numFail)
	}
	fmt.Println()
	fmt.Println()

	if allStats.numFail > 0 {
		os.Exit(1)
	}

	return nil
}

// Structures to hold the results of the tests
type statusCode string

const (
	fail statusCode = "FAIL"
	pass statusCode = "PASS"
	skip statusCode = "SKIP"
)

type status struct {
	code       statusCode
	err        error
	timeTaken  time.Duration
	passHashes []string
}

type job struct {
	file   string
	flags  []string
	format outputFormat
	result chan status
}

type runConfig struct {
	tintPath         string
	dxcPath          string
	fxcPath          string
	xcrunPath        string
	useIr            bool
	generateExpected bool
	generateSkip     bool
	validationCache  validationCache
}

func (j job) run(cfg runConfig) {
	j.result <- func() status {
		// expectedFilePath is the path to the expected output file for the given test
		expectedFilePath := j.file + ".expected."

		if cfg.useIr {
			expectedFilePath += "ir."
		}

		switch j.format {
		case hlslDXC:
			expectedFilePath += "dxc.hlsl"
		case hlslFXC:
			expectedFilePath += "fxc.hlsl"
		default:
			expectedFilePath += string(j.format)
		}

		// Is there an expected output file? If so, load it.
		expected, expectedFileExists := "", false
		if content, err := os.ReadFile(expectedFilePath); err == nil {
			expected = string(content)
			expectedFileExists = true
		}

		skipped := false
		if strings.HasPrefix(expected, "SKIP") { // Special SKIP token
			skipped = true
		}

		expected = strings.ReplaceAll(expected, "\r\n", "\n")

		args := []string{
			j.file,
			"--format", strings.Split(string(j.format), "-")[0], // 'hlsl-fxc' -> 'hlsl', etc.
			"--print-hash",
		}

		if cfg.useIr {
			args = append(args, "--use-ir")
		}

		// Append any skip-hashes, if they're found.
		if j.format != "wgsl" { // Don't skip 'wgsl' as this 'toolchain' is ever changing.
			if skipHashes := cfg.validationCache.knownGood[fileAndFormat{j.file, j.format}]; len(skipHashes) > 0 {
				args = append(args, "--skip-hash", strings.Join(skipHashes, ","))
			}
		}

		// Can we validate?
		validate := false
		switch j.format {
		case wgsl:
			args = append(args, "--validate") // wgsl validation uses Tint, so is always available
			validate = true
		case spvasm, glsl:
			args = append(args, "--validate") // spirv-val and glslang are statically linked, always available
			validate = true
		case hlslDXC:
			if cfg.dxcPath != "" {
				args = append(args, "--dxc", cfg.dxcPath)
				validate = true
			}
		case hlslFXC:
			if cfg.fxcPath != "" {
				args = append(args, "--fxc", cfg.fxcPath)
				validate = true
			}
		case msl:
			if cfg.xcrunPath != "" {
				args = append(args, "--xcrun", cfg.xcrunPath)
				validate = true
			}
		default:
			panic("unknown format: " + j.format)
		}

		// Invoke the compiler...
		ok := false
		var out string
		args = append(args, j.flags...)

		start := time.Now()
		ok, out = invoke(cfg.tintPath, args...)
		timeTaken := time.Since(start)

		out = strings.ReplaceAll(out, "\r\n", "\n")
		out = strings.ReplaceAll(out, filepath.ToSlash(dawnRoot), "<dawn>")
		out, hashes := extractValidationHashes(out)
		matched := expected == "" || expected == out

		canEmitPassExpectationFile := true
		for _, noPass := range dirsWithNoPassExpectations {
			if strings.HasPrefix(filepath.ToSlash(j.file), noPass) {
				canEmitPassExpectationFile = false
				break
			}
		}

		saveExpectedFile := func(path string, content string) error {
			return os.WriteFile(path, []byte(content), 0666)
		}

		if ok && cfg.generateExpected && (validate || !skipped) {
			// User requested to update PASS expectations, and test passed.
			if canEmitPassExpectationFile {
				saveExpectedFile(expectedFilePath, out)
			} else if expectedFileExists {
				// Test lives in a directory where we do not want to save PASS
				// files, and there already exists an expectation file. Test has
				// likely started passing. Delete the old expectation.
				os.Remove(expectedFilePath)
			}
			matched = true // test passed and matched expectations
		}

		switch {
		case ok && matched:
			// Test passed
			return status{code: pass, timeTaken: timeTaken, passHashes: hashes}

			//       --- Below this point the test has failed ---

		case skipped:
			if cfg.generateSkip {
				saveExpectedFile(expectedFilePath, "SKIP: FAILED\n\n"+out)
			}
			return status{code: skip, timeTaken: timeTaken}

		case !ok:
			// Compiler returned non-zero exit code
			if cfg.generateSkip {
				saveExpectedFile(expectedFilePath, "SKIP: FAILED\n\n"+out)
			}
			err := fmt.Errorf("%s", out)
			return status{code: fail, err: err, timeTaken: timeTaken}

		default:
			// Compiler returned zero exit code, or output was not as expected
			if cfg.generateSkip {
				saveExpectedFile(expectedFilePath, "SKIP: FAILED\n\n"+out)
			}

			// Expected output did not match
			dmp := diffmatchpatch.New()
			diff := dmp.DiffPrettyText(dmp.DiffMain(expected, out, true))
			err := fmt.Errorf(`Output was not as expected

--------------------------------------------------------------------------------
-- Expected:                                                                  --
--------------------------------------------------------------------------------
%s

--------------------------------------------------------------------------------
-- Got:                                                                       --
--------------------------------------------------------------------------------
%s

--------------------------------------------------------------------------------
-- Diff:                                                                      --
--------------------------------------------------------------------------------
%s`,
				expected, out, diff)
			return status{code: fail, err: err, timeTaken: timeTaken}
		}
	}()
}

var reValidationHash = regexp.MustCompile(`<<HASH: ([^>]*)>>\n`)

// Parses and returns the validation hashes emitted by tint, or an empty string
// if the hash wasn't found, along with the input string with the validation
// hashes removed.
func extractValidationHashes(in string) (out string, hashes []string) {
	matches := reValidationHash.FindAllStringSubmatch(in, -1)
	if matches == nil {
		return in, nil
	}
	out = in
	for _, match := range matches {
		out = strings.ReplaceAll(out, match[0], "")
		hashes = append(hashes, match[1])
	}
	return out, hashes
}

// indent returns the string 's' indented with 'n' whitespace characters
func indent(s string, n int) string {
	tab := strings.Repeat(" ", n)
	return tab + strings.ReplaceAll(s, "\n", "\n"+tab)
}

// alignLeft returns the string of 'val' padded so that it is aligned left in
// a column of the given width
func alignLeft(val interface{}, width int) string {
	s := fmt.Sprint(val)
	padding := width - utf8.RuneCountInString(s)
	if padding < 0 {
		return s
	}
	return s + strings.Repeat(" ", padding)
}

// alignCenter returns the string of 'val' padded so that it is centered in a
// column of the given width.
func alignCenter(val interface{}, width int) string {
	s := fmt.Sprint(val)
	padding := width - utf8.RuneCountInString(s)
	if padding < 0 {
		return s
	}
	return strings.Repeat(" ", padding/2) + s + strings.Repeat(" ", (padding+1)/2)
}

// alignRight returns the string of 'val' padded so that it is aligned right in
// a column of the given width
func alignRight(val interface{}, width int) string {
	s := fmt.Sprint(val)
	padding := width - utf8.RuneCountInString(s)
	if padding < 0 {
		return s
	}
	return strings.Repeat(" ", padding) + s
}

// maxStringLen returns the maximum number of runes found in all the strings in
// 'l'
func maxStringLen(l []string) int {
	max := 0
	for _, s := range l {
		if c := utf8.RuneCountInString(s); c > max {
			max = c
		}
	}
	return max
}

// formatWidth returns the width in runes for the outputFormat column 'b'
func formatWidth(b outputFormat) int {
	const min = 6
	c := utf8.RuneCountInString(string(b))
	if c < min {
		return min
	}
	return c
}

// percentage returns the percentage of n out of total as a string
func percentage(n, total int) string {
	if total == 0 {
		return "-"
	}
	f := float64(n) / float64(total)
	return fmt.Sprintf("%.1f%c", f*100.0, '%')
}

// invoke runs the executable 'exe' with the provided arguments.
func invoke(exe string, args ...string) (ok bool, output string) {
	ctx, cancel := context.WithTimeout(context.Background(), testTimeout)
	defer cancel()

	cmd := exec.CommandContext(ctx, exe, args...)
	out, err := cmd.CombinedOutput()
	str := string(out)
	if err != nil {
		if ctx.Err() == context.DeadlineExceeded {
			return false, fmt.Sprintf("test timed out after %v", testTimeout)
		}
		if str != "" {
			return false, str
		}
		return false, err.Error()
	}
	return true, str
}

var reFlags = regexp.MustCompile(`^ *(?:\/\/|;) *flags:(.*) *\n`)

// parseFlags looks for a `// flags:` header at the start of the file with the
// given path, returning each of the space delimited tokens that follow for the
// line
func parseFlags(path string) []string {
	content, err := ioutil.ReadFile(path)
	if err != nil {
		return nil
	}
	header := strings.SplitN(string(content), "\n", 1)[0]
	m := reFlags.FindStringSubmatch(header)
	if len(m) != 2 {
		return nil
	}
	return strings.Split(m[1], " ")
}

func printDuration(d time.Duration) string {
	sec := int(d.Seconds())
	min := int(sec) / 60
	hour := min / 60
	min -= hour * 60
	sec -= min * 60
	sb := &strings.Builder{}
	if hour > 0 {
		fmt.Fprintf(sb, "%dh", hour)
	}
	if min > 0 {
		fmt.Fprintf(sb, "%dm", min)
	}
	if sec > 0 {
		fmt.Fprintf(sb, "%ds", sec)
	}
	return sb.String()
}

// fileAndFormat is a pair of test file path and output format.
type fileAndFormat struct {
	file   string
	format outputFormat
}

// Used to optimize end-to-end testing of tint
type validationCache struct {
	// A hash of all the validation toolchains in use.
	toolchainHash string
	// A map of fileAndFormat to known-good (validated) output hashes.
	knownGood knownGoodHashes
}

// A map of fileAndFormat to known-good (validated) output hashes.
type knownGoodHashes map[fileAndFormat][]string

// ValidationCacheFile is the serialized form of a known-good validation.cache file
type ValidationCacheFile struct {
	ToolchainHash string
	KnownGood     []ValidationCacheFileKnownGood
}

// ValidationCacheFileKnownGood holds a single record for a known-to-pass test output, given the
// target format and tool hashes.
type ValidationCacheFileKnownGood struct {
	File   string
	Format outputFormat
	Hashes []string
}

func validationCachePath() string {
	return filepath.Join(fileutils.DawnRoot(), "test", "tint", "validation.cache")
}

// loadValidationCache attempts to load the validation cache.
// Returns an empty cache if the file could not be loaded, or if toolchains have changed.
func loadValidationCache(toolchainHash string) validationCache {
	out := validationCache{
		toolchainHash: toolchainHash,
		knownGood:     knownGoodHashes{},
	}

	file, err := os.Open(validationCachePath())
	if err != nil {
		return out
	}
	defer file.Close()

	content := ValidationCacheFile{}
	if err := json.NewDecoder(file).Decode(&content); err != nil {
		return out
	}

	if content.ToolchainHash != toolchainHash {
		color.Set(color.FgYellow)
		fmt.Println("Toolchains have changed - clearing validation cache")
		color.Unset()
		return out
	}

	for _, knownGood := range content.KnownGood {
		out.knownGood[fileAndFormat{knownGood.File, knownGood.Format}] = knownGood.Hashes
	}

	return out
}

// saveValidationCache saves the validation cache file.
func saveValidationCache(vc validationCache) error {
	out := ValidationCacheFile{
		ToolchainHash: vc.toolchainHash,
		KnownGood:     make([]ValidationCacheFileKnownGood, 0, len(vc.knownGood)),
	}

	for ff, hashes := range vc.knownGood {
		out.KnownGood = append(out.KnownGood, ValidationCacheFileKnownGood{
			File:   ff.file,
			Format: ff.format,
			Hashes: hashes,
		})
	}

	sort.Slice(out.KnownGood, func(i, j int) bool {
		switch {
		case out.KnownGood[i].File < out.KnownGood[j].File:
			return true
		case out.KnownGood[i].File > out.KnownGood[j].File:
			return false
		case out.KnownGood[i].Format < out.KnownGood[j].Format:
			return true
		case out.KnownGood[i].Format > out.KnownGood[j].Format:
			return false
		}
		return false
	})

	file, err := os.Create(validationCachePath())
	if err != nil {
		return fmt.Errorf("failed to save the validation cache file: %w", err)
	}
	defer file.Close()

	enc := json.NewEncoder(file)
	enc.SetIndent("", "  ")
	return enc.Encode(&out)
}

// defaultRootPath returns the default path to the root of the test tree
func defaultRootPath() string {
	return filepath.Join(fileutils.DawnRoot(), "test/tint")
}

// defaultTintPath returns the default path to the tint executable
func defaultTintPath() string {
	return filepath.Join(fileutils.DawnRoot(), "out/active/tint")
}
