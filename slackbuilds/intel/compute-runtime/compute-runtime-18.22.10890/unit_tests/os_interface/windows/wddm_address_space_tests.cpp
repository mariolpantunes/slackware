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

#include "unit_tests/mocks/mock_wddm20.h"
#include "test.h"

using namespace OCLRT;

class WddmMockReserveAddress : public WddmMock {
  public:
    WddmMockReserveAddress() : WddmMock() {}

    void *virtualAlloc(void *inPtr, size_t size, unsigned long flags, unsigned long type) override {
        if (returnGood != 0) {
            return WddmMock::virtualAlloc(inPtr, size, flags, type);
        }

        if (returnInvalidCount != 0) {
            returnInvalidIter++;
            if (returnInvalidIter > returnInvalidCount) {
                return WddmMock::virtualAlloc(inPtr, size, flags, type);
            }
            if (returnNullCount != 0) {
                returnNullIter++;
                if (returnNullIter > returnNullCount) {
                    return nullptr;
                }
                return reinterpret_cast<void *>(0x1000);
            }
            return reinterpret_cast<void *>(0x1000);
        }

        return nullptr;
    }

    int virtualFree(void *ptr, size_t size, unsigned long flags) override {
        if ((ptr == reinterpret_cast<void *>(0x1000)) || (ptr == reinterpret_cast<void *>(0x0))) {
            return 1;
        }

        return WddmMock::virtualFree(ptr, size, flags);
    }

    uint32_t returnGood = 0;
    uint32_t returnInvalidCount = 0;
    uint32_t returnInvalidIter = 0;
    uint32_t returnNullCount = 0;
    uint32_t returnNullIter = 0;
};

using WddmReserveAddressTest = ::testing::Test;

HWTEST_F(WddmReserveAddressTest, givenWddmWhenFirstIsSuccessfulThenReturnReserveAddress) {
    std::unique_ptr<WddmMockReserveAddress> wddm(new WddmMockReserveAddress());
    size_t size = 0x1000;
    void *reserve = nullptr;

    bool ret = wddm->init<FamilyType>();
    EXPECT_TRUE(ret);

    wddm->returnGood = 1;

    ret = wddm->reserveValidAddressRange(size, reserve);
    uintptr_t expectedReserve = wddm->virtualAllocAddress;
    EXPECT_TRUE(ret);
    EXPECT_EQ(expectedReserve, reinterpret_cast<uintptr_t>(reserve));
    wddm->releaseReservedAddress(reserve);
}

HWTEST_F(WddmReserveAddressTest, givenWddmWhenFirstIsNullThenReturnNull) {
    std::unique_ptr<WddmMockReserveAddress> wddm(new WddmMockReserveAddress());
    size_t size = 0x1000;
    void *reserve = nullptr;

    bool ret = wddm->init<FamilyType>();
    EXPECT_TRUE(ret);
    uintptr_t expectedReserve = 0;
    ret = wddm->reserveValidAddressRange(size, reserve);
    EXPECT_FALSE(ret);
    EXPECT_EQ(expectedReserve, reinterpret_cast<uintptr_t>(reserve));
}

HWTEST_F(WddmReserveAddressTest, givenWddmWhenFirstIsInvalidSecondSuccessfulThenReturnSecond) {
    std::unique_ptr<WddmMockReserveAddress> wddm(new WddmMockReserveAddress());
    size_t size = 0x1000;
    void *reserve = nullptr;

    bool ret = wddm->init<FamilyType>();
    EXPECT_TRUE(ret);

    wddm->returnInvalidCount = 1;

    ret = wddm->reserveValidAddressRange(size, reserve);
    uintptr_t expectedReserve = wddm->virtualAllocAddress;
    EXPECT_TRUE(ret);
    EXPECT_EQ(expectedReserve, reinterpret_cast<uintptr_t>(reserve));
    wddm->releaseReservedAddress(reserve);
}

HWTEST_F(WddmReserveAddressTest, givenWddmWhenSecondIsInvalidThirdSuccessfulThenReturnThird) {
    std::unique_ptr<WddmMockReserveAddress> wddm(new WddmMockReserveAddress());
    size_t size = 0x1000;
    void *reserve = nullptr;

    bool ret = wddm->init<FamilyType>();
    EXPECT_TRUE(ret);

    wddm->returnInvalidCount = 2;

    ret = wddm->reserveValidAddressRange(size, reserve);
    uintptr_t expectedReserve = wddm->virtualAllocAddress;
    EXPECT_TRUE(ret);
    EXPECT_EQ(expectedReserve, reinterpret_cast<uintptr_t>(reserve));
    wddm->releaseReservedAddress(reserve);
}

HWTEST_F(WddmReserveAddressTest, givenWddmWhenFirstIsInvalidSecondNullThenReturnSecondNull) {
    std::unique_ptr<WddmMockReserveAddress> wddm(new WddmMockReserveAddress());
    size_t size = 0x1000;
    void *reserve = nullptr;

    bool ret = wddm->init<FamilyType>();
    EXPECT_TRUE(ret);

    wddm->returnInvalidCount = 2;
    wddm->returnNullCount = 1;
    uintptr_t expectedReserve = 0;

    ret = wddm->reserveValidAddressRange(size, reserve);
    EXPECT_FALSE(ret);
    EXPECT_EQ(expectedReserve, reinterpret_cast<uintptr_t>(reserve));
}
