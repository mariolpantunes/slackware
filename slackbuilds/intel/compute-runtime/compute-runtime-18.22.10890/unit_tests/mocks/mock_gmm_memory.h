/*
* Copyright (c) 2017 - 2018, Intel Corporation
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
#include "gmm_memory.h"
#include "runtime/os_interface/windows/windows_defs.h"
#include "gmock/gmock.h"

namespace OCLRT {

class MockGmmMemory : public GmmMemory {
  public:
    ~MockGmmMemory() = default;

    MockGmmMemory() = default;

    bool configureDeviceAddressSpace(GMM_ESCAPE_HANDLE hAdapter,
                                     GMM_ESCAPE_HANDLE hDevice,
                                     GMM_ESCAPE_FUNC_TYPE pfnEscape,
                                     GMM_GFX_SIZE_T SvmSize,
                                     BOOLEAN FaultableSvm,
                                     BOOLEAN SparseReady,
                                     BOOLEAN BDWL3Coherency,
                                     GMM_GFX_SIZE_T SizeOverride,
                                     GMM_GFX_SIZE_T SlmGfxSpaceReserve) override {
        return true;
    }

    uintptr_t getInternalGpuVaRangeLimit() override {
        return OCLRT::windowsMinAddress;
    }
};

class GmockGmmMemory : public GmmMemory {
  public:
    ~GmockGmmMemory() = default;

    GmockGmmMemory() = default;

    MOCK_METHOD9(configureDeviceAddressSpace,
                 bool(GMM_ESCAPE_HANDLE hAdapter,
                      GMM_ESCAPE_HANDLE hDevice,
                      GMM_ESCAPE_FUNC_TYPE pfnEscape,
                      GMM_GFX_SIZE_T SvmSize,
                      BOOLEAN FaultableSvm,
                      BOOLEAN SparseReady,
                      BOOLEAN BDWL3Coherency,
                      GMM_GFX_SIZE_T SizeOverride,
                      GMM_GFX_SIZE_T SlmGfxSpaceReserve));

    uintptr_t getInternalGpuVaRangeLimit() override {
        return OCLRT::windowsMinAddress;
    }
};
} // namespace OCLRT
