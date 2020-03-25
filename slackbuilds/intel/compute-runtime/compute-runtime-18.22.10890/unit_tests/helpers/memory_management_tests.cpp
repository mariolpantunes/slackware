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

#include "unit_tests/helpers/memory_management.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "gtest/gtest.h"

using MemoryManagement::failingAllocation;
using MemoryManagement::numAllocations;
using MemoryManagement::indexAllocation;
using MemoryManagement::indexDeallocation;
using MemoryManagement::AllocationEvent;
using MemoryManagement::eventsAllocated;
using MemoryManagement::eventsDeallocated;

TEST(allocation, nothrow_defaultShouldPass) {
    ASSERT_EQ(failingAllocation, static_cast<size_t>(-1));
    auto ptr = new (std::nothrow) char;
    EXPECT_NE(nullptr, ptr);
    delete ptr;
}

TEST(allocation, nothrow_injectingAFailure) {
    ASSERT_EQ(static_cast<size_t>(-1), failingAllocation);
    auto previousAllocations = numAllocations.load();
    MemoryManagement::indexAllocation = 0;

    failingAllocation = 1;
    auto ptr1 = new (std::nothrow) char;
    auto ptr2 = new (std::nothrow) char;
    delete ptr1;
    delete ptr2;
    auto currentAllocations = numAllocations.load();
    failingAllocation = -1;

    EXPECT_NE(nullptr, ptr1);
    EXPECT_EQ(nullptr, ptr2);
    EXPECT_EQ(previousAllocations, currentAllocations);
}

struct MemoryManagementTest : public MemoryManagementFixture,
                              public ::testing::Test {
    void SetUp() override {
        MemoryManagementFixture::SetUp();
    }

    void TearDown() override {
        MemoryManagementFixture::TearDown();
    }
};

TEST_F(MemoryManagementTest, nothrow_injectingAFailure) {
    setFailingAllocation(1);
    auto ptr1 = new (std::nothrow) char;
    auto ptr2 = new (std::nothrow) char;
    delete ptr1;
    delete ptr2;
    clearFailingAllocation();

    EXPECT_NE(nullptr, ptr1);
    EXPECT_EQ(nullptr, ptr2);
}

TEST_F(MemoryManagementTest, NoLeaks) {
    auto indexAllocationTop = indexAllocation.load();
    auto indexDellocationTop = indexDeallocation.load();
    EXPECT_EQ(static_cast<size_t>(-1), enumerateLeak(indexAllocationTop, indexDellocationTop));
}

TEST_F(MemoryManagementTest, OneLeak) {
    size_t sizeBuffer = 10;
    auto ptr = new (std::nothrow) char[sizeBuffer];
    auto indexAllocationTop = indexAllocation.load();
    auto indexDeallocationTop = indexDeallocation.load();
    auto leakIndex = enumerateLeak(indexAllocationTop, indexDeallocationTop);
    ASSERT_NE(static_cast<size_t>(-1), leakIndex);
    EXPECT_EQ(ptr, eventsAllocated[leakIndex].address);
    EXPECT_EQ(sizeBuffer, eventsAllocated[leakIndex].size);

    // Not expecting any more failures
    EXPECT_EQ(static_cast<size_t>(-1), enumerateLeak(indexAllocationTop, indexDeallocationTop));

    delete[] ptr;
}

TEST_F(MemoryManagementTest, OneLeakBetweenFourEvents) {
    size_t sizeBuffer = 10;
    delete new (std::nothrow) char;
    auto ptr = new (std::nothrow) char[sizeBuffer];
    delete new (std::nothrow) char;
    auto indexAllocationTop = indexAllocation.load();
    auto indexDeallocationTop = indexDeallocation.load();
    auto leakIndex = enumerateLeak(indexAllocationTop, indexDeallocationTop);
    ASSERT_NE(static_cast<size_t>(-1), leakIndex);
    EXPECT_EQ(ptr, eventsAllocated[leakIndex].address);
    EXPECT_EQ(sizeBuffer, eventsAllocated[leakIndex].size);

    // Not expecting any more failures
    EXPECT_EQ(static_cast<size_t>(-1), enumerateLeak(indexAllocationTop, indexDeallocationTop));

    delete[] ptr;
}

TEST_F(MemoryManagementTest, TwoLeaks) {
    size_t sizeBuffer = 10;
    auto ptr1 = new (std::nothrow) char[sizeBuffer];
    auto ptr2 = new (std::nothrow) char[sizeBuffer];
    auto indexAllocationTop = indexAllocation.load();
    auto indexDeallocationTop = indexDeallocation.load();
    auto leakIndex1 = enumerateLeak(indexAllocationTop, indexDeallocationTop);
    auto leakIndex2 = enumerateLeak(indexAllocationTop, indexDeallocationTop);
    ASSERT_NE(static_cast<size_t>(-1), leakIndex1);
    EXPECT_EQ(ptr1, eventsAllocated[leakIndex1].address);
    EXPECT_EQ(sizeBuffer, eventsAllocated[leakIndex1].size);

    ASSERT_NE(static_cast<size_t>(-1), leakIndex2);
    EXPECT_EQ(ptr2, eventsAllocated[leakIndex2].address);
    EXPECT_EQ(sizeBuffer, eventsAllocated[leakIndex2].size);

    // Not expecting any more failures
    EXPECT_EQ(static_cast<size_t>(-1), enumerateLeak(indexAllocationTop, indexDeallocationTop));

    delete[] ptr1;
    delete[] ptr2;
}

TEST_F(MemoryManagementTest, delete_nullptr_shouldntReportLeak) {
    char *ptr = nullptr;
    delete ptr;
}

TEST_F(MemoryManagementTest, shouldBeAbleToViewAllocation) {
    size_t sizeBuffer = 10;
    auto index = MemoryManagement::indexAllocation.load();
    auto ptr = new (std::nothrow) char[sizeBuffer];
    EXPECT_EQ(ptr, eventsAllocated[index].address);
    EXPECT_EQ(sizeBuffer, eventsAllocated[index].size);

    index = MemoryManagement::indexDeallocation;
    auto ptrCopy = ptr;
    delete[] ptr;
    EXPECT_EQ(ptrCopy, eventsDeallocated[index].address);
}

#if ENABLE_ME_FOR_LEAK_TESTING
TEST_F(MemoryManagementTest, EnableForLeakTesting) {
    // Useful reference : MemoryManagement::onAllocationEvent
    MemoryManagement::breakOnAllocationEvent = 1;
    MemoryManagement::breakOnDeallocationEvent = 0;
    delete new char;
    new char;

    MemoryManagement::breakOnAllocationEvent = -1;
    MemoryManagement::breakOnDeallocationEvent = -1;
}
#endif
