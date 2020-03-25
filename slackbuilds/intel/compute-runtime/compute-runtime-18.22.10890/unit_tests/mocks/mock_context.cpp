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

#include "runtime/sharings/sharing.h"
#include "runtime/memory_manager/deferred_deleter.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/memory_manager/svm_memory_manager.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/built_ins/built_ins.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "d3d_sharing_functions.h"

namespace OCLRT {

MockContext::MockContext(Device *device, bool noSpecialQueue) {
    memoryManager = device->getMemoryManager();
    devices.push_back(device);
    svmAllocsManager = new SVMAllocsManager(memoryManager);
    cl_int retVal;
    if (!specialQueue && !noSpecialQueue) {
        auto commandQueue = CommandQueue::create(this, device, nullptr, retVal);
        assert(retVal == CL_SUCCESS);
        overrideSpecialQueueAndDecrementRefCount(commandQueue);
    }
}

MockContext::MockContext(
    void(CL_CALLBACK *funcNotify)(const char *, const void *, size_t, void *),
    void *data) {
    properties = nullptr;
    numProperties = 0;
    contextCallback = funcNotify;
    userData = data;
    memoryManager = nullptr;
    specialQueue = nullptr;
    defaultDeviceQueue = nullptr;
    driverDiagnostics = nullptr;
}

MockContext::~MockContext() {
    if (specialQueue) {
        delete specialQueue;
        specialQueue = nullptr;
    }
    if (memoryManager->isAsyncDeleterEnabled()) {
        memoryManager->getDeferredDeleter()->removeClient();
    }
    memoryManager = nullptr;
}

MockContext::MockContext() {
    device = std::unique_ptr<Device>(DeviceHelper<>::create());
    devices.push_back(device.get());
    memoryManager = device->getMemoryManager();
    svmAllocsManager = new SVMAllocsManager(memoryManager);
    cl_int retVal;
    if (!specialQueue) {
        auto commandQueue = CommandQueue::create(this, device.get(), nullptr, retVal);
        assert(retVal == CL_SUCCESS);
        overrideSpecialQueueAndDecrementRefCount(commandQueue);
    }
}

void MockContext::setSharingFunctions(SharingFunctions *sharingFunctions) {
    this->sharingFunctions[sharingFunctions->getId()].reset(sharingFunctions);
}

void MockContext::releaseSharingFunctions(SharingType sharing) {
    this->sharingFunctions[sharing].release();
}

void MockContext::clearSharingFunctions() {
    std::vector<decltype(this->sharingFunctions)::value_type> v;
    this->sharingFunctions.swap(v);
}
} // namespace OCLRT
