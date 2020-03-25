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
#include "runtime/memory_manager/os_agnostic_memory_manager.h"

#include "gmock/gmock.h"

namespace OCLRT {

class MockMemoryManager : public OsAgnosticMemoryManager {
  public:
    void setDeferredDeleter(DeferredDeleter *deleter);
    void overrideAsyncDeleterFlag(bool newValue);
    GraphicsAllocation *allocateGraphicsMemoryForImage(ImageInfo &imgInfo, Gmm *gmm) override;
    int redundancyRatio = 1;
    void setCommandStreamReceiver(CommandStreamReceiver *csr);
    void setDevice(Device *device);
    bool isAllocationListEmpty();
    GraphicsAllocation *peekAllocationListHead();
};

class GMockMemoryManager : public MockMemoryManager {
  public:
    MOCK_METHOD2(cleanAllocationList, bool(uint32_t waitTaskCount, uint32_t allocationType));
    // cleanAllocationList call defined in MemoryManager.

    MOCK_METHOD1(populateOsHandles, MemoryManager::AllocationStatus(OsHandleStorage &handleStorage));
    bool MemoryManagerCleanAllocationList(uint32_t waitTaskCount, uint32_t allocationType) { return MemoryManager::cleanAllocationList(waitTaskCount, allocationType); }
    MemoryManager::AllocationStatus MemoryManagerPopulateOsHandles(OsHandleStorage &handleStorage) { return OsAgnosticMemoryManager::populateOsHandles(handleStorage); }
};

class MockAllocSysMemAgnosticMemoryManager : public OsAgnosticMemoryManager {
  public:
    MockAllocSysMemAgnosticMemoryManager() : OsAgnosticMemoryManager() {
        ptrRestrictions = nullptr;
        testRestrictions.minAddress = 0;
    }

    AlignedMallocRestrictions *getAlignedMallocRestrictions() override {
        return ptrRestrictions;
    }

    void *allocateSystemMemory(size_t size, size_t alignment) override {
        constexpr size_t minAlignment = 16;
        alignment = std::max(alignment, minAlignment);
        return alignedMalloc(size, alignment);
    }

    AlignedMallocRestrictions testRestrictions;
    AlignedMallocRestrictions *ptrRestrictions;
};

} // namespace OCLRT
