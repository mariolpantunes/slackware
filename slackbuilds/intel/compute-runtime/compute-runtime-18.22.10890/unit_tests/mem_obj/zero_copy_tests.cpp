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

#include "runtime/mem_obj/buffer.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/memory_management.h"
#include "runtime/helpers/aligned_memory.h"
#include "unit_tests/mocks/mock_context.h"
#include "gtest/gtest.h"

using namespace OCLRT;

class ZeroCopyBufferTest : public DeviceFixture,
                           public testing::TestWithParam<std::tuple<uint64_t /*cl_mem_flags*/, size_t, size_t, int, bool, bool>> {
  public:
    ZeroCopyBufferTest() {
    }

  protected:
    void SetUp() override {
        size_t sizeToAlloc;
        size_t alignment;
        host_ptr = nullptr;
        std::tie(flags, sizeToAlloc, alignment, size, ShouldBeZeroCopy, MisalignPointer) = GetParam();
        if (sizeToAlloc > 0) {
            host_ptr = (void *)alignedMalloc(sizeToAlloc, alignment);
        }
        DeviceFixture::SetUp();
    }

    void TearDown() override {
        DeviceFixture::TearDown();
        alignedFree(host_ptr);
    }

    cl_int retVal = CL_SUCCESS;
    MockContext context;
    cl_mem_flags flags = 0;
    void *host_ptr;
    bool ShouldBeZeroCopy;
    cl_int size;
    bool MisalignPointer;
};

static const int Multiplier = 1000;
static const int CacheLinedAlignedSize = MemoryConstants::cacheLineSize * Multiplier;
static const int CacheLinedMisAlignedSize = CacheLinedAlignedSize - 1;
static const int PageAlignSize = MemoryConstants::preferredAlignment * Multiplier;

// clang-format off
//flags, size to alloc, alignment, size, ZeroCopy, MisalignPointer
std::tuple<uint64_t , size_t, size_t, int, bool, bool> Inputs[] = {std::make_tuple((cl_mem_flags)CL_MEM_USE_HOST_PTR, CacheLinedMisAlignedSize, MemoryConstants::preferredAlignment, CacheLinedMisAlignedSize, false, true),
                                                                           std::make_tuple((cl_mem_flags)CL_MEM_USE_HOST_PTR, CacheLinedAlignedSize, MemoryConstants::preferredAlignment, CacheLinedAlignedSize, false, true),
                                                                           std::make_tuple((cl_mem_flags)CL_MEM_USE_HOST_PTR, CacheLinedAlignedSize, MemoryConstants::preferredAlignment, CacheLinedAlignedSize, true, false),
                                                                           std::make_tuple((cl_mem_flags)CL_MEM_USE_HOST_PTR, CacheLinedMisAlignedSize, MemoryConstants::preferredAlignment, CacheLinedMisAlignedSize, false, false),
                                                                           std::make_tuple((cl_mem_flags)CL_MEM_USE_HOST_PTR, PageAlignSize, MemoryConstants::preferredAlignment, PageAlignSize, true, false),
                                                                           std::make_tuple((cl_mem_flags)CL_MEM_USE_HOST_PTR, CacheLinedMisAlignedSize, MemoryConstants::cacheLineSize, CacheLinedAlignedSize, true, false),
                                                                           std::make_tuple((cl_mem_flags)CL_MEM_COPY_HOST_PTR, CacheLinedMisAlignedSize, MemoryConstants::preferredAlignment, CacheLinedMisAlignedSize, true, true),
                                                                           std::make_tuple((cl_mem_flags)CL_MEM_COPY_HOST_PTR, CacheLinedMisAlignedSize, MemoryConstants::preferredAlignment, CacheLinedMisAlignedSize, true, false),
                                                                           std::make_tuple((cl_mem_flags)NULL, 0, 0, CacheLinedMisAlignedSize, true, false),
                                                                           std::make_tuple((cl_mem_flags)NULL, 0, 0, CacheLinedAlignedSize, true, true)};
// clang-format on

TEST_P(ZeroCopyBufferTest, CheckCacheAlignedPointerResultsInZeroCopy) {

    char *PassedPtr = (char *)host_ptr;
    //misalign the pointer
    if (MisalignPointer && PassedPtr) {
        PassedPtr += 1;
    }

    auto buffer = Buffer::create(
        &context,
        flags,
        size,
        PassedPtr,
        retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(ShouldBeZeroCopy, buffer->isMemObjZeroCopy()) << "Zero Copy not handled properly";
    if (!ShouldBeZeroCopy && flags & CL_MEM_USE_HOST_PTR) {
        EXPECT_NE(buffer->getCpuAddress(), host_ptr);
    }

    EXPECT_NE(nullptr, buffer->getCpuAddress());

    //check if buffer always have properly aligned storage ( PAGE )
    EXPECT_EQ(alignUp(buffer->getCpuAddress(), MemoryConstants::cacheLineSize), buffer->getCpuAddress());

    delete buffer;
}

INSTANTIATE_TEST_CASE_P(
    ZeroCopyBufferTests,
    ZeroCopyBufferTest,
    testing::ValuesIn(Inputs));

TEST(ZeroCopyBufferTestWithSharedContext, GivenContextThatIsSharedWhenAskedForBufferCreationThenAlwaysResultsInZeroCopy) {

    MockContext context;
    auto host_ptr = reinterpret_cast<void *>(0x1001);
    auto size = 64;
    auto retVal = CL_SUCCESS;

    context.isSharedContext = true;
    std::unique_ptr<Buffer> buffer(Buffer::create(&context, CL_MEM_USE_HOST_PTR, size, host_ptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(buffer->isMemObjZeroCopy()) << "Zero Copy not handled properly";

    if (buffer->getGraphicsAllocation()->is32BitAllocation == false) {
        EXPECT_EQ(host_ptr, buffer->getGraphicsAllocation()->getUnderlyingBuffer());
    }
}

TEST(ZeroCopyBufferTestWithSharedContext, GivenContextThatIsSharedAndDisableZeroCopyFlagWhenAskedForBufferCreationThenAlwaysResultsInZeroCopy) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.DisableZeroCopyForUseHostPtr.set(true);

    MockContext context;
    auto host_ptr = reinterpret_cast<void *>(0x1001);
    auto size = 64;
    auto retVal = CL_SUCCESS;

    context.isSharedContext = true;
    std::unique_ptr<Buffer> buffer(Buffer::create(&context, CL_MEM_USE_HOST_PTR, size, host_ptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(buffer->isMemObjZeroCopy());
}

TEST(ZeroCopyWithDebugFlag, GivenInputsThatWouldResultInZeroCopyAndDisableZeroCopyFlagWhenBufferIsCreatedThenNonZeroCopyBufferIsReturned) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.DisableZeroCopyForUseHostPtr.set(true);
    MockContext context;
    auto host_ptr = reinterpret_cast<void *>(alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize));
    auto size = MemoryConstants::pageSize;
    auto retVal = CL_SUCCESS;

    std::unique_ptr<Buffer> buffer(Buffer::create(&context, CL_MEM_USE_HOST_PTR, size, host_ptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(buffer->isMemObjZeroCopy());
    alignedFree(host_ptr);
}

TEST(ZeroCopyBufferWith32BitAddressing, GivenDeviceSupporting32BitAddressingWhenAskedForBufferCreationFromHostPtrThenNonZeroCopyBufferIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.Force32bitAddressing.set(true);
    MockContext context;
    auto host_ptr = (void *)alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize);
    auto size = MemoryConstants::pageSize;
    auto retVal = CL_SUCCESS;

    std::unique_ptr<Buffer> buffer(Buffer::create(&context, CL_MEM_USE_HOST_PTR, size, host_ptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(buffer->isMemObjZeroCopy());
    if (is64bit) {
        EXPECT_TRUE(buffer->getGraphicsAllocation()->is32BitAllocation);
    }
    alignedFree(host_ptr);
}
