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

#ifndef SRC_DAWN_NATIVE_COMPUTEPIPELINE_H_
#define SRC_DAWN_NATIVE_COMPUTEPIPELINE_H_

#include "dawn/common/ContentLessObjectCacheable.h"
#include "dawn/common/NonCopyable.h"
#include "dawn/native/Forward.h"
#include "dawn/native/Pipeline.h"

namespace dawn::native {

class DeviceBase;
struct EntryPointMetadata;

MaybeError ValidateComputePipelineDescriptor(DeviceBase* device,
                                             const ComputePipelineDescriptor* descriptor);

class ComputePipelineBase : public PipelineBase,
                            public ContentLessObjectCacheable<ComputePipelineBase> {
  public:
    ComputePipelineBase(DeviceBase* device, const ComputePipelineDescriptor* descriptor);
    ~ComputePipelineBase() override;

    static ComputePipelineBase* MakeError(DeviceBase* device, const char* label);

    ObjectType GetType() const override;

    // Functors necessary for the unordered_set<ComputePipelineBase*>-based cache.
    struct EqualityFunc {
        bool operator()(const ComputePipelineBase* a, const ComputePipelineBase* b) const;
    };

    bool IsFullSubgroupsRequired() const;

  protected:
    void DestroyImpl() override;

  private:
    ComputePipelineBase(DeviceBase* device, ObjectBase::ErrorTag tag, const char* label);

    bool mRequiresFullSubgroups;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_COMPUTEPIPELINE_H_
