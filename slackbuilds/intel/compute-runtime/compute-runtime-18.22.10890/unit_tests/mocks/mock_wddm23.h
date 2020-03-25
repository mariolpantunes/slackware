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

#include "runtime/os_interface/windows/wddm/wddm23.h"

namespace OCLRT {
class WddmMock23 : public Wddm23 {
  public:
    using Wddm23::context;
    using Wddm23::createHwQueue;
    using Wddm23::createMonitoredFence;
    using Wddm23::currentPagingFenceValue;
    using Wddm23::destroyHwQueue;
    using Wddm23::gdi;
    using Wddm23::hwQueueHandle;
    using Wddm23::hwQueuesSupported;
    using Wddm23::pagingFenceAddress;
    using Wddm23::submit;

    WddmMock23() : Wddm23(){};

    bool waitOnGPU() override {
        waitOnGPUCalled++;
        return true;
    }

    bool createHwQueue() override {
        createHwQueueCalled++;
        createHwQueueResult = forceCreateHwQueueFail ? false : Wddm23::createHwQueue();
        return createHwQueueResult;
    }

    uint32_t waitOnGPUCalled = 0;
    uint32_t createHwQueueCalled = 0;
    bool forceCreateHwQueueFail = false;
    bool createHwQueueResult = false;
};
} // namespace OCLRT
