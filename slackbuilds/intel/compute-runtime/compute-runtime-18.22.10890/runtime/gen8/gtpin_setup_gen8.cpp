/*
 * Copyright (c) 2018, Intel Corporation
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

#include "gtpin_ocl_interface.h"
#include "runtime/gtpin/gtpin_hw_helper.h"
#include "runtime/gtpin/gtpin_hw_helper.inl"

namespace OCLRT {

extern GTPinHwHelper *gtpinHwHelperFactory[IGFX_MAX_CORE];

typedef BDWFamily Family;
static const auto gfxFamily = IGFX_GEN8_CORE;

template <>
uint32_t GTPinHwHelperHw<Family>::getGenVersion() {
    return gtpin::GTPIN_GEN_8;
}

template class GTPinHwHelperHw<Family>;

struct GTPinEnableGen8 {
    GTPinEnableGen8() {
        gtpinHwHelperFactory[gfxFamily] = &GTPinHwHelperHw<Family>::get();
    }
};

static GTPinEnableGen8 gtpinEnable;

} // namespace OCLRT
