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

#include "runtime/memory_manager/deferred_deleter.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/surface_formats.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include <cstring>

namespace OCLRT {

void MockMemoryManager::setDeferredDeleter(DeferredDeleter *deleter) {
    deferredDeleter.reset(deleter);
}

void MockMemoryManager::overrideAsyncDeleterFlag(bool newValue) {
    asyncDeleterEnabled = newValue;
    if (asyncDeleterEnabled && deferredDeleter == nullptr) {
        deferredDeleter = createDeferredDeleter();
    }
}
GraphicsAllocation *MockMemoryManager::allocateGraphicsMemoryForImage(ImageInfo &imgInfo, Gmm *gmm) {
    imgInfo.size *= redundancyRatio;
    auto *allocation = OsAgnosticMemoryManager::allocateGraphicsMemoryForImage(imgInfo, gmm);
    imgInfo.size /= redundancyRatio;
    if (redundancyRatio != 1) {
        memset((unsigned char *)allocation->getUnderlyingBuffer(), 0, imgInfo.size * redundancyRatio);
    }
    return allocation;
}

void MockMemoryManager::setCommandStreamReceiver(CommandStreamReceiver *csr) {
    this->csr = csr;
}

void MockMemoryManager::setDevice(Device *device) {
    this->device = device;
}

bool MockMemoryManager::isAllocationListEmpty() {
    return graphicsAllocations.peekIsEmpty();
}

GraphicsAllocation *MockMemoryManager::peekAllocationListHead() {
    return graphicsAllocations.peekHead();
}

} // namespace OCLRT
