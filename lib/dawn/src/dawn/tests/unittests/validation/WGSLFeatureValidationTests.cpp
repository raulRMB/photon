// Copyright 2023 The Dawn & Tint Authors
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
#include <vector>

#include "dawn/tests/unittests/validation/ValidationTest.h"
#include "dawn/utils/WGPUHelpers.h"

namespace dawn {
namespace {

class WGSLFeatureValidationTest : public ValidationTest {
  protected:
    void SetUp() override {
        ValidationTest::SetUp();
        DAWN_SKIP_TEST_IF(UsesWire());
    }

    struct InstanceSpec {
        bool useTestingFeatures = true;
        bool allowUnsafeAPIs = false;
    };

    wgpu::Instance CreateInstance(InstanceSpec spec) {
        wgpu::InstanceDescriptor desc;

        std::vector<const char*> enabledToggles;
        if (spec.useTestingFeatures) {
            enabledToggles.push_back("expose_wgsl_testing_features");
        }
        if (spec.allowUnsafeAPIs) {
            enabledToggles.push_back("allow_unsafe_apis");
        }

        wgpu::DawnTogglesDescriptor togglesDesc;
        togglesDesc.nextInChain = desc.nextInChain;
        desc.nextInChain = &togglesDesc;
        togglesDesc.enabledToggleCount = enabledToggles.size();
        togglesDesc.enabledToggles = enabledToggles.data();

        return wgpu::CreateInstance(&desc);
    }

    wgpu::Device CreateDeviceOnInstance(wgpu::Instance instance) {
        // Get the adapter
        wgpu::Adapter adapter;
        instance.RequestAdapter(
            nullptr,
            [](WGPURequestAdapterStatus status, WGPUAdapter a, const char* message,
               void* userdata) {
                ASSERT_EQ(status, WGPURequestAdapterStatus_Success);
                ASSERT_NE(a, nullptr);
                *reinterpret_cast<wgpu::Adapter*>(userdata) = wgpu::Adapter::Acquire(a);
            },
            &adapter);

        while (!adapter) {
            FlushWire();
        }
        EXPECT_NE(nullptr, adapter.Get());

        // Get the device
        wgpu::Device device;
        adapter.RequestDevice(
            nullptr,
            [](WGPURequestDeviceStatus status, WGPUDevice d, const char* message, void* userdata) {
                ASSERT_EQ(status, WGPURequestDeviceStatus_Success);
                ASSERT_NE(d, nullptr);
                *reinterpret_cast<wgpu::Device*>(userdata) = wgpu::Device::Acquire(d);
            },
            &device);

        while (!device) {
            FlushWire();
        }
        EXPECT_NE(nullptr, device.Get());

        device.SetUncapturedErrorCallback(ValidationTest::OnDeviceError, this);
        return device;
    }
};

wgpu::WGSLFeatureName kNonExistentFeature = static_cast<wgpu::WGSLFeatureName>(0xFFFF'FFFF);

// Check HasFeature for an Instance that doesn't have unsafe APIs.
TEST_F(WGSLFeatureValidationTest, HasFeatureDefaultInstance) {
    wgpu::Instance instance = CreateInstance({});

    // Shipped features are present.
    ASSERT_TRUE(instance.HasWGSLLanguageFeature(wgpu::WGSLFeatureName::ChromiumTestingShipped));
    ASSERT_TRUE(instance.HasWGSLLanguageFeature(
        wgpu::WGSLFeatureName::ChromiumTestingShippedWithKillswitch));

    // Experimental and unimplemented features are not present.
    ASSERT_FALSE(
        instance.HasWGSLLanguageFeature(wgpu::WGSLFeatureName::ChromiumTestingExperimental));
    ASSERT_FALSE(
        instance.HasWGSLLanguageFeature(wgpu::WGSLFeatureName::ChromiumTestingUnsafeExperimental));
    ASSERT_FALSE(
        instance.HasWGSLLanguageFeature(wgpu::WGSLFeatureName::ChromiumTestingUnimplemented));

    // Non-existent features are not present.
    ASSERT_FALSE(instance.HasWGSLLanguageFeature(kNonExistentFeature));
}

// Check HasFeature for an Instance that has unsafe APIs.
TEST_F(WGSLFeatureValidationTest, HasFeatureAllowUnsafeInstance) {
    wgpu::Instance instance = CreateInstance({.allowUnsafeAPIs = true});

    // Shipped and experimental features are present.
    ASSERT_TRUE(instance.HasWGSLLanguageFeature(wgpu::WGSLFeatureName::ChromiumTestingShipped));
    ASSERT_TRUE(instance.HasWGSLLanguageFeature(
        wgpu::WGSLFeatureName::ChromiumTestingShippedWithKillswitch));
    ASSERT_TRUE(
        instance.HasWGSLLanguageFeature(wgpu::WGSLFeatureName::ChromiumTestingExperimental));
    ASSERT_TRUE(
        instance.HasWGSLLanguageFeature(wgpu::WGSLFeatureName::ChromiumTestingUnsafeExperimental));

    // Experimental and unimplemented features are not present.
    ASSERT_FALSE(
        instance.HasWGSLLanguageFeature(wgpu::WGSLFeatureName::ChromiumTestingUnimplemented));

    // Non-existent features are not present.
    ASSERT_FALSE(instance.HasWGSLLanguageFeature(kNonExistentFeature));
}

// Check HasFeature for an Instance that doesn't have the expose_wgsl_testing_features toggle.
TEST_F(WGSLFeatureValidationTest, HasFeatureWithoutExposeWGSLTestingFeatures) {
    wgpu::Instance instance = CreateInstance({.useTestingFeatures = false});

    // None of the testing features are present.
    ASSERT_FALSE(instance.HasWGSLLanguageFeature(wgpu::WGSLFeatureName::ChromiumTestingShipped));
    ASSERT_FALSE(instance.HasWGSLLanguageFeature(
        wgpu::WGSLFeatureName::ChromiumTestingShippedWithKillswitch));
    ASSERT_FALSE(
        instance.HasWGSLLanguageFeature(wgpu::WGSLFeatureName::ChromiumTestingExperimental));
    ASSERT_FALSE(
        instance.HasWGSLLanguageFeature(wgpu::WGSLFeatureName::ChromiumTestingUnsafeExperimental));
    ASSERT_FALSE(
        instance.HasWGSLLanguageFeature(wgpu::WGSLFeatureName::ChromiumTestingUnimplemented));
}

// Tests for the behavior of WGSL feature enumeration.
TEST_F(WGSLFeatureValidationTest, EnumerateFeatures) {
    wgpu::Instance instance = CreateInstance({});

    size_t featureCount = instance.EnumerateWGSLLanguageFeatures(nullptr);

    std::vector<wgpu::WGSLFeatureName> features(featureCount + 1, kNonExistentFeature);
    size_t secondFeatureCount = instance.EnumerateWGSLLanguageFeatures(features.data());

    // Exactly featureCount features should be written, and all return true in HasWGSLFeature.
    ASSERT_EQ(secondFeatureCount, featureCount);
    for (size_t i = 0; i < featureCount; i++) {
        ASSERT_TRUE(instance.HasWGSLLanguageFeature(features[i]));
        ASSERT_NE(kNonExistentFeature, features[i]);
    }
    ASSERT_EQ(kNonExistentFeature, features[featureCount]);

    // Test the presence / absence of some known testing features.
    ASSERT_NE(
        std::find(features.begin(), features.end(), wgpu::WGSLFeatureName::ChromiumTestingShipped),
        features.end());
    ASSERT_NE(std::find(features.begin(), features.end(),
                        wgpu::WGSLFeatureName::ChromiumTestingShippedWithKillswitch),
              features.end());

    ASSERT_EQ(std::find(features.begin(), features.end(),
                        wgpu::WGSLFeatureName::ChromiumTestingUnimplemented),
              features.end());
    ASSERT_EQ(std::find(features.begin(), features.end(),
                        wgpu::WGSLFeatureName::ChromiumTestingUnsafeExperimental),
              features.end());
    ASSERT_EQ(std::find(features.begin(), features.end(),
                        wgpu::WGSLFeatureName::ChromiumTestingExperimental),
              features.end());
}

// Check that the enabled / disabled features are used to validate the WGSL shaders.
TEST_F(WGSLFeatureValidationTest, UsingFeatureInShaderModule) {
    wgpu::Instance instance = CreateInstance({});
    wgpu::Device device = CreateDeviceOnInstance(instance);

    utils::CreateShaderModule(device, R"(
        requires chromium_testing_shipped;
    )");
    utils::CreateShaderModule(device, R"(
        requires chromium_testing_shipped_with_killswitch;
    )");

    ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, R"(
        requires chromium_testing_unimplemented;
    )"));
    ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, R"(
        requires chromium_testing_unsafe_experimental;
    )"));
    ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, R"(
        requires chromium_testing_experimental;
    )"));
}

}  // anonymous namespace
}  // namespace dawn
