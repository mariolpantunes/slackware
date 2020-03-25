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

#include "runtime/mem_obj/mem_obj.h"
#include "runtime/device/device.h"
#include "runtime/helpers/properties_helper.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_deferred_deleter.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "gtest/gtest.h"

using namespace OCLRT;

TEST(MemObj, useCount) {
    char buffer[64];
    MockContext context;
    MockGraphicsAllocation *mockAllocation = new MockGraphicsAllocation(buffer, sizeof(buffer));

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_USE_HOST_PTR,
                  sizeof(buffer), buffer, buffer, mockAllocation, true, false, false);

    EXPECT_EQ(ObjectNotResident, memObj.getGraphicsAllocation()->residencyTaskCount);
    memObj.getGraphicsAllocation()->residencyTaskCount = 1;
    EXPECT_EQ(1, memObj.getGraphicsAllocation()->residencyTaskCount);
    memObj.getGraphicsAllocation()->residencyTaskCount--;
    EXPECT_EQ(0, memObj.getGraphicsAllocation()->residencyTaskCount);
}

TEST(MemObj, GivenMemObjWhenInititalizedFromHostPtrThenInitializeFields) {
    const size_t size = 64;
    char buffer[size];
    MockContext context;
    MockGraphicsAllocation *mockAllocation = new MockGraphicsAllocation(buffer, sizeof(buffer));

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_USE_HOST_PTR,
                  sizeof(buffer), buffer, buffer, mockAllocation, true, false, false);

    EXPECT_EQ(&buffer, memObj.getCpuAddress());
    EXPECT_EQ(&buffer, memObj.getHostPtr());
    EXPECT_EQ(size, memObj.getSize());
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_USE_HOST_PTR), memObj.getFlags());
}

TEST(MemObj, givenMemObjectWhenAskedForTransferToHostPtrThenDoNothing) {
    const size_t size = 64;
    uint8_t hostPtr[size] = {};
    uint8_t expectedHostPtr[size] = {};
    MockContext context;
    MockGraphicsAllocation *mockAllocation = new MockGraphicsAllocation(hostPtr, sizeof(hostPtr));
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_USE_HOST_PTR,
                  size, hostPtr, hostPtr, mockAllocation, true, false, false);

    memset(memObj.getCpuAddress(), 123, size);
    memset(hostPtr, 0, size);

    MemObjOffsetArray copyOffset = {{0, 0, 0}};
    MemObjSizeArray copySize = {{size, 0, 0}};
    EXPECT_THROW(memObj.transferDataToHostPtr(copySize, copyOffset), std::exception);

    EXPECT_TRUE(memcmp(hostPtr, expectedHostPtr, size) == 0);
}

TEST(MemObj, givenMemObjectWhenAskedForTransferFromHostPtrThenDoNothing) {
    const size_t size = 64;
    uint8_t hostPtr[size] = {};
    uint8_t expectedBufferPtr[size] = {};
    MockContext context;
    MockGraphicsAllocation *mockAllocation = new MockGraphicsAllocation(hostPtr, sizeof(hostPtr));
    MemObj memObj(&context, CL_MEM_OBJECT_PIPE, CL_MEM_USE_HOST_PTR,
                  size, hostPtr, hostPtr, mockAllocation, true, false, false);

    memset(memObj.getCpuAddress(), 123, size);
    memset(expectedBufferPtr, 123, size);

    MemObjOffsetArray copyOffset = {{0, 0, 0}};
    MemObjSizeArray copySize = {{size, 0, 0}};
    EXPECT_THROW(memObj.transferDataFromHostPtr(copySize, copyOffset), std::exception);

    EXPECT_TRUE(memcmp(memObj.getCpuAddress(), expectedBufferPtr, size) == 0);
}

TEST(MemObj, givenHostPtrAndUseHostPtrFlagWhenAskingForBaseMapPtrThenReturnHostPtr) {
    uint8_t hostPtr = 0;
    MockContext context;

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_USE_HOST_PTR,
                  1, nullptr, &hostPtr, nullptr, true, false, false);

    EXPECT_EQ(&hostPtr, memObj.getBasePtrForMap());
}

TEST(MemObj, givenHostPtrWithoutUseHostPtrFlagWhenAskingForBaseMapPtrThenReturnAllocatedPtr) {
    uint8_t hostPtr = 0;
    MockContext context;

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  1, nullptr, &hostPtr, nullptr, true, false, false);

    EXPECT_NE(&hostPtr, memObj.getBasePtrForMap());
    EXPECT_EQ(memObj.getAllocatedMapPtr(), memObj.getBasePtrForMap());
}

TEST(MemObj, givenMemObjWhenReleaseAllocatedPtrIsCalledTwiceThenItDoesntCrash) {
    void *allocatedPtr = alignedMalloc(MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);

    MockContext context;

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_USE_HOST_PTR,
                  1, nullptr, nullptr, nullptr, true, false, false);

    memObj.setAllocatedMapPtr(allocatedPtr);
    memObj.releaseAllocatedMapPtr();
    EXPECT_EQ(nullptr, memObj.getAllocatedMapPtr());
    memObj.releaseAllocatedMapPtr();
    EXPECT_EQ(nullptr, memObj.getAllocatedMapPtr());
}

TEST(MemObj, givenNotReadyGraphicsAllocationWhenMemObjDestroysAllocationAsyncThenAllocationIsAddedToMemoryManagerAllocationList) {
    MockMemoryManager memoryManager;
    MockContext context;

    context.setMemoryManager(&memoryManager);
    memoryManager.setDevice(context.getDevice(0));

    auto allocation = memoryManager.allocateGraphicsMemory(MemoryConstants::pageSize, MemoryConstants::pageSize);
    allocation->taskCount = 2;
    *memoryManager.device->getTagAddress() = 1;
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  MemoryConstants::pageSize, nullptr, nullptr, nullptr, true, false, false);

    EXPECT_TRUE(memoryManager.isAllocationListEmpty());
    memObj.destroyGraphicsAllocation(allocation, true);

    EXPECT_FALSE(memoryManager.isAllocationListEmpty());
}

TEST(MemObj, givenReadyGraphicsAllocationWhenMemObjDestroysAllocationAsyncThenAllocationIsNotAddedToMemoryManagerAllocationList) {
    MockMemoryManager memoryManager;
    MockContext context;

    context.setMemoryManager(&memoryManager);
    memoryManager.setDevice(context.getDevice(0));

    auto allocation = memoryManager.allocateGraphicsMemory(MemoryConstants::pageSize, MemoryConstants::pageSize);
    allocation->taskCount = 1;
    *memoryManager.device->getTagAddress() = 1;
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  MemoryConstants::pageSize, nullptr, nullptr, nullptr, true, false, false);

    EXPECT_TRUE(memoryManager.isAllocationListEmpty());
    memObj.destroyGraphicsAllocation(allocation, true);

    EXPECT_TRUE(memoryManager.isAllocationListEmpty());
}

TEST(MemObj, givenNotUsedGraphicsAllocationWhenMemObjDestroysAllocationAsyncThenAllocationIsNotAddedToMemoryManagerAllocationList) {
    MockMemoryManager memoryManager;
    MockContext context;

    context.setMemoryManager(&memoryManager);
    memoryManager.setDevice(context.getDevice(0));

    auto allocation = memoryManager.allocateGraphicsMemory(MemoryConstants::pageSize, MemoryConstants::pageSize);
    allocation->taskCount = ObjectNotUsed;
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  MemoryConstants::pageSize, nullptr, nullptr, nullptr, true, false, false);

    EXPECT_TRUE(memoryManager.isAllocationListEmpty());
    memObj.destroyGraphicsAllocation(allocation, true);

    EXPECT_TRUE(memoryManager.isAllocationListEmpty());
}

TEST(MemObj, givenMemoryManagerWithoutDeviceWhenMemObjDestroysAllocationAsyncThenAllocationIsNotAddedToMemoryManagerAllocationList) {
    MockMemoryManager memoryManager;
    MockContext context;

    context.setMemoryManager(&memoryManager);

    auto allocation = memoryManager.allocateGraphicsMemory(MemoryConstants::pageSize, MemoryConstants::pageSize);

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  MemoryConstants::pageSize, nullptr, nullptr, nullptr, true, false, false);

    EXPECT_TRUE(memoryManager.isAllocationListEmpty());
    memObj.destroyGraphicsAllocation(allocation, true);

    EXPECT_TRUE(memoryManager.isAllocationListEmpty());
}

TEST(MemObj, givenMemObjWhenItDoesntHaveGraphicsAllocationThenWaitForCsrCompletionDoesntCrash) {
    MockMemoryManager memoryManager;
    MockContext context;

    context.setMemoryManager(&memoryManager);

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  MemoryConstants::pageSize, nullptr, nullptr, nullptr, true, false, false);

    EXPECT_EQ(nullptr, memoryManager.device);
    EXPECT_EQ(nullptr, memObj.getGraphicsAllocation());
    memObj.waitForCsrCompletion();

    memoryManager.setDevice(context.getDevice(0));

    EXPECT_NE(nullptr, memoryManager.device);
    EXPECT_EQ(nullptr, memObj.getGraphicsAllocation());
    memObj.waitForCsrCompletion();
}
TEST(MemObj, givenMemObjAndPointerToObjStorageWithProperCommandWhenCheckIfMemTransferRequiredThenReturnFalse) {
    MockMemoryManager memoryManager;
    MockContext context;

    context.setMemoryManager(&memoryManager);

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  MemoryConstants::pageSize, nullptr, nullptr, nullptr, true, false, false);
    void *ptr = memObj.getCpuAddressForMemoryTransfer();
    bool isMemTransferNeeded = memObj.checkIfMemoryTransferIsRequired(0, 0, ptr, CL_COMMAND_WRITE_BUFFER);
    EXPECT_FALSE(isMemTransferNeeded);

    isMemTransferNeeded = memObj.checkIfMemoryTransferIsRequired(0, 0, ptr, CL_COMMAND_READ_BUFFER);
    EXPECT_FALSE(isMemTransferNeeded);

    isMemTransferNeeded = memObj.checkIfMemoryTransferIsRequired(0, 0, ptr, CL_COMMAND_WRITE_BUFFER_RECT);
    EXPECT_FALSE(isMemTransferNeeded);

    isMemTransferNeeded = memObj.checkIfMemoryTransferIsRequired(0, 0, ptr, CL_COMMAND_READ_BUFFER_RECT);
    EXPECT_FALSE(isMemTransferNeeded);

    isMemTransferNeeded = memObj.checkIfMemoryTransferIsRequired(0, 0, ptr, CL_COMMAND_WRITE_IMAGE);
    EXPECT_FALSE(isMemTransferNeeded);

    isMemTransferNeeded = memObj.checkIfMemoryTransferIsRequired(0, 0, ptr, CL_COMMAND_READ_IMAGE);
    EXPECT_FALSE(isMemTransferNeeded);
}
TEST(MemObj, givenMemObjAndPointerToObjStorageBadCommandWhenCheckIfMemTransferRequiredThenReturnTrue) {
    MockMemoryManager memoryManager;
    MockContext context;

    context.setMemoryManager(&memoryManager);

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  MemoryConstants::pageSize, nullptr, nullptr, nullptr, true, false, false);
    void *ptr = memObj.getCpuAddressForMemoryTransfer();
    bool isMemTransferNeeded = memObj.checkIfMemoryTransferIsRequired(0, 0, ptr, CL_COMMAND_FILL_BUFFER);
    EXPECT_TRUE(isMemTransferNeeded);
}
TEST(MemObj, givenMemObjAndPointerToDiffrentStorageAndProperCommandWhenCheckIfMemTransferRequiredThenReturnTrue) {
    MockMemoryManager memoryManager;
    MockContext context;

    context.setMemoryManager(&memoryManager);

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  MemoryConstants::pageSize, nullptr, nullptr, nullptr, true, false, false);
    void *ptr = (void *)0x1234;
    bool isMemTransferNeeded = memObj.checkIfMemoryTransferIsRequired(0, 0, ptr, CL_COMMAND_WRITE_BUFFER);
    EXPECT_TRUE(isMemTransferNeeded);
}

TEST(MemObj, givenSharingHandlerWhenAskedForCpuMappingThenReturnFalse) {
    MemObj memObj(nullptr, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  MemoryConstants::pageSize, nullptr, nullptr, nullptr, true, false, false);
    memObj.setSharingHandler(new SharingHandler());
    EXPECT_FALSE(memObj.mappingOnCpuAllowed());
}

TEST(MemObj, givenTiledObjectWhenAskedForCpuMappingThenReturnFalse) {
    struct MyMemObj : public MemObj {
        using MemObj::MemObj;
        bool allowTiling() const override { return true; }
    };
    MyMemObj memObj(nullptr, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                    MemoryConstants::pageSize, nullptr, nullptr, nullptr, true, false, false);

    EXPECT_FALSE(memObj.mappingOnCpuAllowed());
}

TEST(MemObj, givenDefaultWhenAskedForCpuMappingThenReturnTrue) {
    char mem[64];
    MemObj memObj(nullptr, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  MemoryConstants::pageSize, mem, nullptr, nullptr, true, false, false);

    EXPECT_FALSE(memObj.allowTiling());
    EXPECT_FALSE(memObj.peekSharingHandler());
    EXPECT_TRUE(memObj.mappingOnCpuAllowed());
}

TEST(MemObj, givenMultipleMemObjectsWithReusedGraphicsAllocationWhenDestroyedThenFreeAllocationOnce) {
    // Each SharingHandler should have own implementation of reuseCount management
    struct MySharingHandler : public SharingHandler {
        MySharingHandler(GraphicsAllocation *allocation) : allocation(allocation) {
            allocation->incReuseCount();
        }
        void releaseReusedGraphicsAllocation() override {
            allocation->decReuseCount();
        }

        GraphicsAllocation *allocation = nullptr;
    };

    MockMemoryManager memoryManager;
    MockContext context;
    context.setMemoryManager(&memoryManager);

    auto allocation = memoryManager.allocateGraphicsMemory(1);

    std::unique_ptr<MemObj> memObj1(new MemObj(&context, CL_MEM_OBJECT_BUFFER, 0, 1, nullptr, nullptr, allocation, true, false, false));
    memObj1->setSharingHandler(new MySharingHandler(allocation));

    std::unique_ptr<MemObj> memObj2(new MemObj(&context, CL_MEM_OBJECT_BUFFER, 0, 1, nullptr, nullptr, allocation, true, false, false));
    memObj2->setSharingHandler(new MySharingHandler(allocation));

    std::unique_ptr<MemObj> memObj3(new MemObj(&context, CL_MEM_OBJECT_BUFFER, 0, 1, nullptr, nullptr, allocation, true, false, false));
    memObj3->setSharingHandler(new MySharingHandler(allocation));

    EXPECT_EQ(3u, allocation->peekReuseCount());

    memObj3.reset(nullptr);
    EXPECT_EQ(2u, allocation->peekReuseCount());
    memObj1.reset(nullptr);
    EXPECT_EQ(1u, allocation->peekReuseCount());

    memObj2.reset(nullptr);
}

TEST(MemObj, givenMemObjectWhenContextIsNotNullThenContextOutlivesMemobjects) {
    MockContext context;
    EXPECT_EQ(1, context.getRefInternalCount());
    {
        MemObj memObj(&context, 0, 0, 0, nullptr, nullptr, nullptr, false, false, false);
        EXPECT_EQ(2, context.getRefInternalCount());
    }
    EXPECT_EQ(1, context.getRefInternalCount());
}
