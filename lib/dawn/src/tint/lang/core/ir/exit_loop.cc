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

#include "src/tint/lang/core/ir/exit_loop.h"

#include <utility>

#include "src/tint/lang/core/ir/block.h"
#include "src/tint/lang/core/ir/clone_context.h"
#include "src/tint/lang/core/ir/loop.h"
#include "src/tint/lang/core/ir/module.h"
#include "src/tint/lang/core/ir/multi_in_block.h"

TINT_INSTANTIATE_TYPEINFO(tint::core::ir::ExitLoop);

namespace tint::core::ir {

ExitLoop::ExitLoop(ir::Loop* loop, VectorRef<Value*> args /* = tint::Empty */) {
    SetLoop(loop);
    AddOperands(ExitLoop::kArgsOperandOffset, std::move(args));
}

ExitLoop::~ExitLoop() = default;

ExitLoop* ExitLoop::Clone(CloneContext& ctx) {
    auto* loop = ctx.Remap(Loop());
    auto args = ctx.Remap<ExitLoop::kDefaultNumOperands>(Args());
    return ctx.ir.instructions.Create<ExitLoop>(loop, args);
}

void ExitLoop::SetLoop(ir::Loop* l) {
    SetControlInstruction(l);
}

ir::Loop* ExitLoop::Loop() {
    return static_cast<ir::Loop*>(ControlInstruction());
}

const ir::Loop* ExitLoop::Loop() const {
    return static_cast<const ir::Loop*>(ControlInstruction());
}

}  // namespace tint::core::ir
