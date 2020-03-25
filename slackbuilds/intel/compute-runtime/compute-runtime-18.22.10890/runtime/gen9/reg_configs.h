/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "runtime/helpers/preamble.h"
#include "runtime/command_stream/thread_arbitration_policy.h"

namespace OCLRT {
struct SKLFamily;
template <>
struct L3CNTLREGConfig<IGFX_SKYLAKE> {
    static const uint32_t valueForSLM = 0x60000121u;
    static const uint32_t valueForNoSLM = 0x80000140u;
};

template <>
struct L3CNTLRegisterOffset<SKLFamily> {
    static const uint32_t registerOffset = 0x7034;
};

template <>
struct L3CNTLREGConfig<IGFX_BROXTON> {
    static const uint32_t valueForSLM = 0x60000121u;
    static const uint32_t valueForNoSLM = 0x80000140u;
};

namespace DebugControlReg2 {
constexpr uint32_t address = 0xE404;
constexpr uint32_t getRegData(const uint32_t &policy) {
    return policy == ThreadArbitrationPolicy::RoundRobin ? 0x100 : 0x0;
};
} // namespace DebugControlReg2

} // namespace OCLRT
