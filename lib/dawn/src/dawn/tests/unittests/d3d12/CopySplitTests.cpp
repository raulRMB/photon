// Copyright 2017 The Dawn & Tint Authors
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

#include <algorithm>

#include "dawn/common/Assert.h"
#include "dawn/common/Constants.h"
#include "dawn/common/Math.h"
#include "dawn/native/Format.h"
#include "dawn/native/d3d12/TextureCopySplitter.h"
#include "dawn/native/d3d12/d3d12_platform.h"
#include "dawn/utils/TestUtils.h"
#include "dawn/webgpu_cpp_print.h"
#include "gtest/gtest.h"

namespace dawn::native::d3d12 {
namespace {

struct TextureSpec {
    uint32_t x;
    uint32_t y;
    uint32_t z;
    uint32_t width;
    uint32_t height;
    uint32_t depthOrArrayLayers;
    uint32_t texelBlockSizeInBytes;
    uint32_t blockWidth = 1;
    uint32_t blockHeight = 1;
};

struct BufferSpec {
    uint64_t offset;
    uint32_t bytesPerRow;
    uint32_t rowsPerImage;
};

// Check that each copy region fits inside the buffer footprint
void ValidateFootprints(const TextureSpec& textureSpec,
                        const BufferSpec& bufferSpec,
                        const TextureCopySubresource& copySplit,
                        wgpu::TextureDimension dimension) {
    for (uint32_t i = 0; i < copySplit.count; ++i) {
        const auto& copy = copySplit.copies[i];
        ASSERT_LE(copy.bufferOffset.x + copy.copySize.width, copy.bufferSize.width);
        ASSERT_LE(copy.bufferOffset.y + copy.copySize.height, copy.bufferSize.height);
        ASSERT_LE(copy.bufferOffset.z + copy.copySize.depthOrArrayLayers,
                  copy.bufferSize.depthOrArrayLayers);

        // If there are multiple layers, 2D texture splitter actually splits each layer
        // independently. See the details in Compute2DTextureCopySplits(). As a result,
        // if we simply expand a copy region generated by 2D texture splitter to all
        // layers, the copy region might be OOB. But that is not the approach that the
        // current 2D texture splitter is doing, although Compute2DTextureCopySubresource
        // forwards "copySize.depthOrArrayLayers" to the copy region it generated. So skip
        // the test below for 2D textures with multiple layers.
        if (textureSpec.depthOrArrayLayers <= 1 || dimension == wgpu::TextureDimension::e3D) {
            uint32_t widthInBlocks = textureSpec.width / textureSpec.blockWidth;
            uint32_t heightInBlocks = textureSpec.height / textureSpec.blockHeight;
            uint64_t minimumRequiredBufferSize =
                bufferSpec.offset +
                utils::RequiredBytesInCopy(
                    bufferSpec.bytesPerRow, bufferSpec.rowsPerImage, widthInBlocks, heightInBlocks,
                    textureSpec.depthOrArrayLayers, textureSpec.texelBlockSizeInBytes);

            // The last pixel (buffer footprint) of each copy region depends on its
            // bufferOffset and copySize. It is not the last pixel where the bufferSize
            // ends.
            ASSERT_EQ(copy.bufferOffset.x % textureSpec.blockWidth, 0u);
            ASSERT_EQ(copy.copySize.width % textureSpec.blockWidth, 0u);
            uint32_t footprintWidth = copy.bufferOffset.x + copy.copySize.width;
            ASSERT_EQ(footprintWidth % textureSpec.blockWidth, 0u);
            uint32_t footprintWidthInBlocks = footprintWidth / textureSpec.blockWidth;

            ASSERT_EQ(copy.bufferOffset.y % textureSpec.blockHeight, 0u);
            ASSERT_EQ(copy.copySize.height % textureSpec.blockHeight, 0u);
            uint32_t footprintHeight = copy.bufferOffset.y + copy.copySize.height;
            ASSERT_EQ(footprintHeight % textureSpec.blockHeight, 0u);
            uint32_t footprintHeightInBlocks = footprintHeight / textureSpec.blockHeight;

            uint64_t bufferSizeForFootprint =
                copy.alignedOffset +
                utils::RequiredBytesInCopy(bufferSpec.bytesPerRow, copy.bufferSize.height,
                                           footprintWidthInBlocks, footprintHeightInBlocks,
                                           copy.bufferSize.depthOrArrayLayers,
                                           textureSpec.texelBlockSizeInBytes);

            // The buffer footprint of each copy region should not exceed the minimum
            // required buffer size. Otherwise, pixels accessed by copy may be OOB.
            ASSERT_LE(bufferSizeForFootprint, minimumRequiredBufferSize);
        }
    }
}

// Check that the offset is aligned
void ValidateOffset(const TextureCopySubresource& copySplit) {
    for (uint32_t i = 0; i < copySplit.count; ++i) {
        ASSERT_TRUE(
            Align(copySplit.copies[i].alignedOffset, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT) ==
            copySplit.copies[i].alignedOffset);
    }
}

bool InclusiveRangesOverlap(uint32_t minA, uint32_t maxA, uint32_t minB, uint32_t maxB) {
    return (minA <= minB && minB <= maxA) || (minB <= minA && minA <= maxB);
}

// Check that no pair of copy regions intersect each other
void ValidateDisjoint(const TextureCopySubresource& copySplit) {
    for (uint32_t i = 0; i < copySplit.count; ++i) {
        const auto& a = copySplit.copies[i];
        for (uint32_t j = i + 1; j < copySplit.count; ++j) {
            const auto& b = copySplit.copies[j];
            // If textureOffset.x is 0, and copySize.width is 2, we are copying pixel 0 and
            // 1. We never touch pixel 2 on x-axis. So the copied range on x-axis should be
            // [textureOffset.x, textureOffset.x + copySize.width - 1] and both ends are
            // included.
            bool overlapX =
                InclusiveRangesOverlap(a.textureOffset.x, a.textureOffset.x + a.copySize.width - 1,
                                       b.textureOffset.x, b.textureOffset.x + b.copySize.width - 1);
            bool overlapY = InclusiveRangesOverlap(
                a.textureOffset.y, a.textureOffset.y + a.copySize.height - 1, b.textureOffset.y,
                b.textureOffset.y + b.copySize.height - 1);
            bool overlapZ = InclusiveRangesOverlap(
                a.textureOffset.z, a.textureOffset.z + a.copySize.depthOrArrayLayers - 1,
                b.textureOffset.z, b.textureOffset.z + b.copySize.depthOrArrayLayers - 1);
            ASSERT_TRUE(!overlapX || !overlapY || !overlapZ);
        }
    }
}

// Check that the union of the copy regions exactly covers the texture region
void ValidateTextureBounds(const TextureSpec& textureSpec,
                           const TextureCopySubresource& copySplit) {
    ASSERT_GT(copySplit.count, 0u);

    uint32_t minX = copySplit.copies[0].textureOffset.x;
    uint32_t minY = copySplit.copies[0].textureOffset.y;
    uint32_t minZ = copySplit.copies[0].textureOffset.z;
    uint32_t maxX = copySplit.copies[0].textureOffset.x + copySplit.copies[0].copySize.width;
    uint32_t maxY = copySplit.copies[0].textureOffset.y + copySplit.copies[0].copySize.height;
    uint32_t maxZ =
        copySplit.copies[0].textureOffset.z + copySplit.copies[0].copySize.depthOrArrayLayers;

    for (uint32_t i = 1; i < copySplit.count; ++i) {
        const auto& copy = copySplit.copies[i];
        minX = std::min(minX, copy.textureOffset.x);
        minY = std::min(minY, copy.textureOffset.y);
        minZ = std::min(minZ, copy.textureOffset.z);
        maxX = std::max(maxX, copy.textureOffset.x + copy.copySize.width);
        maxY = std::max(maxY, copy.textureOffset.y + copy.copySize.height);
        maxZ = std::max(maxZ, copy.textureOffset.z + copy.copySize.depthOrArrayLayers);
    }

    ASSERT_EQ(minX, textureSpec.x);
    ASSERT_EQ(minY, textureSpec.y);
    ASSERT_EQ(minZ, textureSpec.z);
    ASSERT_EQ(maxX, textureSpec.x + textureSpec.width);
    ASSERT_EQ(maxY, textureSpec.y + textureSpec.height);
    ASSERT_EQ(maxZ, textureSpec.z + textureSpec.depthOrArrayLayers);
}

// Validate that the number of pixels copied is exactly equal to the number of pixels in the
// texture region
void ValidatePixelCount(const TextureSpec& textureSpec, const TextureCopySubresource& copySplit) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < copySplit.count; ++i) {
        const auto& copy = copySplit.copies[i];
        uint32_t copiedPixels =
            copy.copySize.width * copy.copySize.height * copy.copySize.depthOrArrayLayers;
        ASSERT_GT(copiedPixels, 0u);
        count += copiedPixels;
    }
    ASSERT_EQ(count, textureSpec.width * textureSpec.height * textureSpec.depthOrArrayLayers);
}

// Check that every buffer offset is at the correct pixel location
void ValidateBufferOffset(const TextureSpec& textureSpec,
                          const BufferSpec& bufferSpec,
                          const TextureCopySubresource& copySplit,
                          wgpu::TextureDimension dimension) {
    ASSERT_GT(copySplit.count, 0u);

    uint32_t texelsPerBlock = textureSpec.blockWidth * textureSpec.blockHeight;
    for (uint32_t i = 0; i < copySplit.count; ++i) {
        const auto& copy = copySplit.copies[i];

        uint32_t bytesPerRowInTexels =
            bufferSpec.bytesPerRow / textureSpec.texelBlockSizeInBytes * texelsPerBlock;
        uint32_t slicePitchInTexels =
            bytesPerRowInTexels * (bufferSpec.rowsPerImage / textureSpec.blockHeight);
        uint32_t absoluteTexelOffset =
            copy.alignedOffset / textureSpec.texelBlockSizeInBytes * texelsPerBlock +
            copy.bufferOffset.x / textureSpec.blockWidth * texelsPerBlock +
            copy.bufferOffset.y / textureSpec.blockHeight * bytesPerRowInTexels;

        // There is one empty row at most in a 2D copy region. However, it is not true for
        // a 3D texture copy region when we are copying the last row of each slice. We may
        // need to offset a lot rows and copy.bufferOffset.y may be big.
        if (dimension == wgpu::TextureDimension::e2D) {
            ASSERT_LE(copy.bufferOffset.y, textureSpec.blockHeight);
        }
        ASSERT_EQ(copy.bufferOffset.z, 0u);

        ASSERT_GE(absoluteTexelOffset,
                  bufferSpec.offset / textureSpec.texelBlockSizeInBytes * texelsPerBlock);
        uint32_t relativeTexelOffset = absoluteTexelOffset - bufferSpec.offset /
                                                                 textureSpec.texelBlockSizeInBytes *
                                                                 texelsPerBlock;

        uint32_t z = relativeTexelOffset / slicePitchInTexels;
        uint32_t y = (relativeTexelOffset % slicePitchInTexels) / bytesPerRowInTexels;
        uint32_t x = relativeTexelOffset % bytesPerRowInTexels;

        ASSERT_EQ(copy.textureOffset.x - textureSpec.x, x);
        ASSERT_EQ(copy.textureOffset.y - textureSpec.y, y);
        ASSERT_EQ(copy.textureOffset.z - textureSpec.z, z);
    }
}

void ValidateCopySplit(const TextureSpec& textureSpec,
                       const BufferSpec& bufferSpec,
                       const TextureCopySubresource& copySplit,
                       wgpu::TextureDimension dimension) {
    ValidateFootprints(textureSpec, bufferSpec, copySplit, dimension);
    ValidateOffset(copySplit);
    ValidateDisjoint(copySplit);
    ValidateTextureBounds(textureSpec, copySplit);
    ValidatePixelCount(textureSpec, copySplit);
    ValidateBufferOffset(textureSpec, bufferSpec, copySplit, dimension);
}

std::ostream& operator<<(std::ostream& os, const TextureSpec& textureSpec) {
    os << "TextureSpec("
       << "[(" << textureSpec.x << ", " << textureSpec.y << ", " << textureSpec.z << "), ("
       << textureSpec.width << ", " << textureSpec.height << ", " << textureSpec.depthOrArrayLayers
       << ")], " << textureSpec.texelBlockSizeInBytes << ")";
    return os;
}

std::ostream& operator<<(std::ostream& os, const BufferSpec& bufferSpec) {
    os << "BufferSpec(" << bufferSpec.offset << ", " << bufferSpec.bytesPerRow << ", "
       << bufferSpec.rowsPerImage << ")";
    return os;
}

std::ostream& operator<<(std::ostream& os, const TextureCopySubresource& copySplit) {
    os << "CopySplit" << std::endl;
    for (uint32_t i = 0; i < copySplit.count; ++i) {
        const auto& copy = copySplit.copies[i];
        os << "  " << i << ": Texture at (" << copy.textureOffset.x << ", " << copy.textureOffset.y
           << ", " << copy.textureOffset.z << "), size (" << copy.copySize.width << ", "
           << copy.copySize.height << ", " << copy.copySize.depthOrArrayLayers << ")" << std::endl;
        os << "  " << i << ": Buffer at (" << copy.bufferOffset.x << ", " << copy.bufferOffset.y
           << ", " << copy.bufferOffset.z << "), footprint (" << copy.bufferSize.width << ", "
           << copy.bufferSize.height << ", " << copy.bufferSize.depthOrArrayLayers << ")"
           << std::endl;
    }
    return os;
}

// Define base texture sizes and offsets to test with: some aligned, some unaligned
constexpr TextureSpec kBaseTextureSpecs[] = {
    {0, 0, 0, 1, 1, 1, 4},
    {0, 0, 0, 64, 1, 1, 4},
    {0, 0, 0, 128, 1, 1, 4},
    {0, 0, 0, 192, 1, 1, 4},
    {31, 16, 0, 1, 1, 1, 4},
    {64, 16, 0, 1, 1, 1, 4},
    {64, 16, 8, 1, 1, 1, 4},

    {0, 0, 0, 64, 2, 1, 4},
    {0, 0, 0, 64, 1, 2, 4},
    {0, 0, 0, 64, 2, 2, 4},
    {0, 0, 0, 128, 2, 1, 4},
    {0, 0, 0, 128, 1, 2, 4},
    {0, 0, 0, 128, 2, 2, 4},
    {0, 0, 0, 192, 2, 1, 4},
    {0, 0, 0, 192, 1, 2, 4},
    {0, 0, 0, 192, 2, 2, 4},

    {0, 0, 0, 1024, 1024, 1, 4},
    {256, 512, 0, 1024, 1024, 1, 4},
    {64, 48, 0, 1024, 1024, 1, 4},
    {64, 48, 16, 1024, 1024, 1024, 4},

    {0, 0, 0, 257, 31, 1, 4},
    {0, 0, 0, 17, 93, 1, 4},
    {59, 13, 0, 257, 31, 1, 4},
    {17, 73, 0, 17, 93, 1, 4},
    {17, 73, 59, 17, 93, 99, 4},

    {0, 0, 0, 4, 4, 1, 8, 4, 4},
    {64, 16, 0, 4, 4, 1, 8, 4, 4},
    {64, 16, 8, 4, 4, 1, 8, 4, 4},
    {0, 0, 0, 4, 4, 1, 16, 4, 4},
    {64, 16, 0, 4, 4, 1, 16, 4, 4},
    {64, 16, 8, 4, 4, 1, 16, 4, 4},

    {0, 0, 0, 1024, 1024, 1, 8, 4, 4},
    {256, 512, 0, 1024, 1024, 1, 8, 4, 4},
    {64, 48, 0, 1024, 1024, 1, 8, 4, 4},
    {64, 48, 16, 1024, 1024, 1, 8, 4, 4},
    {0, 0, 0, 1024, 1024, 1, 16, 4, 4},
    {256, 512, 0, 1024, 1024, 1, 16, 4, 4},
    {64, 48, 0, 1024, 1024, 1, 4, 16, 4},
    {64, 48, 16, 1024, 1024, 1, 16, 4, 4},
};

// Define base buffer sizes to work with: some offsets aligned, some unaligned. bytesPerRow
// is the minimum required
std::array<BufferSpec, 15> BaseBufferSpecs(const TextureSpec& textureSpec) {
    uint32_t bytesPerRow =
        Align(textureSpec.texelBlockSizeInBytes * textureSpec.width, kTextureBytesPerRowAlignment);

    auto alignNonPow2 = [](uint32_t value, uint32_t size) -> uint32_t {
        return value == 0 ? 0 : ((value - 1) / size + 1) * size;
    };

    return {
        BufferSpec{alignNonPow2(0, textureSpec.texelBlockSizeInBytes), bytesPerRow,
                   textureSpec.height},
        BufferSpec{alignNonPow2(256, textureSpec.texelBlockSizeInBytes), bytesPerRow,
                   textureSpec.height},
        BufferSpec{alignNonPow2(512, textureSpec.texelBlockSizeInBytes), bytesPerRow,
                   textureSpec.height},
        BufferSpec{alignNonPow2(1024, textureSpec.texelBlockSizeInBytes), bytesPerRow,
                   textureSpec.height},
        BufferSpec{alignNonPow2(1024, textureSpec.texelBlockSizeInBytes), bytesPerRow,
                   textureSpec.height * 2},

        BufferSpec{alignNonPow2(32, textureSpec.texelBlockSizeInBytes), bytesPerRow,
                   textureSpec.height},
        BufferSpec{alignNonPow2(64, textureSpec.texelBlockSizeInBytes), bytesPerRow,
                   textureSpec.height},
        BufferSpec{alignNonPow2(64, textureSpec.texelBlockSizeInBytes), bytesPerRow,
                   textureSpec.height * 2},

        BufferSpec{alignNonPow2(31, textureSpec.texelBlockSizeInBytes), bytesPerRow,
                   textureSpec.height},
        BufferSpec{alignNonPow2(257, textureSpec.texelBlockSizeInBytes), bytesPerRow,
                   textureSpec.height},
        BufferSpec{alignNonPow2(384, textureSpec.texelBlockSizeInBytes), bytesPerRow,
                   textureSpec.height},
        BufferSpec{alignNonPow2(511, textureSpec.texelBlockSizeInBytes), bytesPerRow,
                   textureSpec.height},
        BufferSpec{alignNonPow2(513, textureSpec.texelBlockSizeInBytes), bytesPerRow,
                   textureSpec.height},
        BufferSpec{alignNonPow2(1023, textureSpec.texelBlockSizeInBytes), bytesPerRow,
                   textureSpec.height},
        BufferSpec{alignNonPow2(1023, textureSpec.texelBlockSizeInBytes), bytesPerRow,
                   textureSpec.height * 2},
    };
}

// Define a list of values to set properties in the spec structs
constexpr uint32_t kCheckValues[] = {1,  2,  3,  4,   5,   6,   7,    8,     // small values
                                     16, 32, 64, 128, 256, 512, 1024, 2048,  // powers of 2
                                     15, 31, 63, 127, 257, 511, 1023, 2047,  // misalignments
                                     17, 33, 65, 129, 257, 513, 1025, 2049};

class CopySplitTest : public testing::TestWithParam<wgpu::TextureDimension> {
  protected:
    void DoTest(const TextureSpec& textureSpec, const BufferSpec& bufferSpec) {
        DAWN_ASSERT(textureSpec.width % textureSpec.blockWidth == 0 &&
                    textureSpec.height % textureSpec.blockHeight == 0);

        wgpu::TextureDimension dimension = GetParam();
        TextureCopySubresource copySplit;
        switch (dimension) {
            case wgpu::TextureDimension::e2D: {
                copySplit = Compute2DTextureCopySubresource(
                    {textureSpec.x, textureSpec.y, textureSpec.z},
                    {textureSpec.width, textureSpec.height, textureSpec.depthOrArrayLayers},
                    {textureSpec.texelBlockSizeInBytes, textureSpec.blockWidth,
                     textureSpec.blockHeight},
                    bufferSpec.offset, bufferSpec.bytesPerRow);
                break;
            }
            case wgpu::TextureDimension::e3D: {
                copySplit = Compute3DTextureCopySplits(
                    {textureSpec.x, textureSpec.y, textureSpec.z},
                    {textureSpec.width, textureSpec.height, textureSpec.depthOrArrayLayers},
                    {textureSpec.texelBlockSizeInBytes, textureSpec.blockWidth,
                     textureSpec.blockHeight},
                    bufferSpec.offset, bufferSpec.bytesPerRow, bufferSpec.rowsPerImage);
                break;
            }
            default:
                DAWN_UNREACHABLE();
                break;
        }

        ValidateCopySplit(textureSpec, bufferSpec, copySplit, dimension);

        if (HasFatalFailure()) {
            std::ostringstream message;
            message << "Failed generating splits: " << textureSpec << ", " << bufferSpec
                    << std::endl
                    << dimension << " " << copySplit << std::endl;
            FAIL() << message.str();
        }
    }
};

TEST_P(CopySplitTest, General) {
    for (TextureSpec textureSpec : kBaseTextureSpecs) {
        for (BufferSpec bufferSpec : BaseBufferSpecs(textureSpec)) {
            DoTest(textureSpec, bufferSpec);
        }
    }
}

TEST_P(CopySplitTest, TextureWidth) {
    for (TextureSpec textureSpec : kBaseTextureSpecs) {
        for (uint32_t val : kCheckValues) {
            if (val % textureSpec.blockWidth != 0) {
                continue;
            }
            textureSpec.width = val;
            for (BufferSpec bufferSpec : BaseBufferSpecs(textureSpec)) {
                DoTest(textureSpec, bufferSpec);
            }
        }
    }
}

TEST_P(CopySplitTest, TextureHeight) {
    for (TextureSpec textureSpec : kBaseTextureSpecs) {
        for (uint32_t val : kCheckValues) {
            if (val % textureSpec.blockHeight != 0) {
                continue;
            }
            textureSpec.height = val;
            for (BufferSpec bufferSpec : BaseBufferSpecs(textureSpec)) {
                DoTest(textureSpec, bufferSpec);
            }
        }
    }
}

TEST_P(CopySplitTest, TextureX) {
    for (TextureSpec textureSpec : kBaseTextureSpecs) {
        for (uint32_t val : kCheckValues) {
            textureSpec.x = val;
            for (BufferSpec bufferSpec : BaseBufferSpecs(textureSpec)) {
                DoTest(textureSpec, bufferSpec);
            }
        }
    }
}

TEST_P(CopySplitTest, TextureY) {
    for (TextureSpec textureSpec : kBaseTextureSpecs) {
        for (uint32_t val : kCheckValues) {
            textureSpec.y = val;
            for (BufferSpec bufferSpec : BaseBufferSpecs(textureSpec)) {
                DoTest(textureSpec, bufferSpec);
            }
        }
    }
}

TEST_P(CopySplitTest, TexelSize) {
    for (TextureSpec textureSpec : kBaseTextureSpecs) {
        for (uint32_t texelSize : {4, 8, 16, 32, 64}) {
            textureSpec.texelBlockSizeInBytes = texelSize;
            for (BufferSpec bufferSpec : BaseBufferSpecs(textureSpec)) {
                DoTest(textureSpec, bufferSpec);
            }
        }
    }
}

TEST_P(CopySplitTest, BufferOffset) {
    for (TextureSpec textureSpec : kBaseTextureSpecs) {
        for (BufferSpec bufferSpec : BaseBufferSpecs(textureSpec)) {
            for (uint32_t val : kCheckValues) {
                bufferSpec.offset = textureSpec.texelBlockSizeInBytes * val;

                DoTest(textureSpec, bufferSpec);
            }
        }
    }
}

TEST_P(CopySplitTest, RowPitch) {
    for (TextureSpec textureSpec : kBaseTextureSpecs) {
        for (BufferSpec bufferSpec : BaseBufferSpecs(textureSpec)) {
            uint32_t baseRowPitch = bufferSpec.bytesPerRow;
            for (uint32_t i = 0; i < 5; ++i) {
                bufferSpec.bytesPerRow = baseRowPitch + i * 256;

                DoTest(textureSpec, bufferSpec);
            }
        }
    }
}

TEST_P(CopySplitTest, ImageHeight) {
    for (TextureSpec textureSpec : kBaseTextureSpecs) {
        for (BufferSpec bufferSpec : BaseBufferSpecs(textureSpec)) {
            uint32_t baseImageHeight = bufferSpec.rowsPerImage;
            for (uint32_t i = 0; i < 5; ++i) {
                bufferSpec.rowsPerImage = baseImageHeight + i * 256;

                DoTest(textureSpec, bufferSpec);
            }
        }
    }
}

INSTANTIATE_TEST_SUITE_P(,
                         CopySplitTest,
                         testing::Values(wgpu::TextureDimension::e2D, wgpu::TextureDimension::e3D));

}  // anonymous namespace
}  // namespace dawn::native::d3d12