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

package common

import (
	"fmt"
	"regexp"
	"strconv"
	"strings"
	"time"
)

var re = regexp.MustCompile(`• Copyright (\d+) The`)

const header = `• Copyright %v The Dawn & Tint Authors
•
• Redistribution and use in source and binary forms, with or without
• modification, are permitted provided that the following conditions are met:
•
• 1. Redistributions of source code must retain the above copyright notice, this
•    list of conditions and the following disclaimer.
•
• 2. Redistributions in binary form must reproduce the above copyright notice,
•    this list of conditions and the following disclaimer in the documentation
•    and/or other materials provided with the distribution.
•
• 3. Neither the name of the copyright holder nor the names of its
•    contributors may be used to endorse or promote products derived from
•    this software without specific prior written permission.
•
• THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
• AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
• IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
• DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
• FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
• DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
• SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
• CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
• OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
• OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

‣
• File generated by 'tools/src/cmd/gen' using the template:
•   %v
•
• To regenerate run: './tools/run gen'
•
•                       Do not modify this file directly
‣
`

func Header(existing, templatePath, comment string) string {
	copyrightYear := time.Now().Year()

	// Replace comment characters with '•'
	existing = strings.ReplaceAll(existing, comment, "•")

	// Look for the existing copyright year
	if match := re.FindStringSubmatch(string(existing)); len(match) == 2 {
		if year, err := strconv.Atoi(match[1]); err == nil {
			copyrightYear = year
		}
	}

	// Replace '•' with comment characters, '‣' with a line of comment characters
	out := strings.ReplaceAll(header, "•", comment)
	out = strings.ReplaceAll(out, "‣", strings.Repeat(comment, 80/len(comment)))

	return fmt.Sprintf(out, copyrightYear, templatePath)
}
