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

#pragma once
#include "runtime/gen_common/hw_cmds.h"

namespace OCLRT {
class Kernel;

class GTPinHwHelper {
  public:
    static GTPinHwHelper &get(GFXCORE_FAMILY gfxCore);
    virtual uint32_t getGenVersion() = 0;
    virtual bool addSurfaceState(Kernel *pKernel) = 0;
    virtual void *getSurfaceState(Kernel *pKernel, size_t bti) = 0;

  protected:
    GTPinHwHelper(){};
};

template <typename GfxFamily>
class GTPinHwHelperHw : public GTPinHwHelper {
  public:
    static GTPinHwHelper &get() {
        static GTPinHwHelperHw<GfxFamily> gtpinHwHelper;
        return gtpinHwHelper;
    }
    uint32_t getGenVersion() override;
    bool addSurfaceState(Kernel *pKernel) override;
    void *getSurfaceState(Kernel *pKernel, size_t bti) override;

  private:
    GTPinHwHelperHw(){};
};
} // namespace OCLRT
