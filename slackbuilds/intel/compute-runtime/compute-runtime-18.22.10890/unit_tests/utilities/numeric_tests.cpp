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

#include "test.h"
#include "runtime/utilities/numeric.h"

#include <type_traits>

using namespace OCLRT;

TEST(StorageTypeTest, whenGivenNumberOfBitsThenPicksIntegerTypeThatIsLargeEnough) {
    static_assert(std::is_same<uint8_t, StorageTypeT<0>>::value, "");
    static_assert(std::is_same<uint8_t, StorageTypeT<1>>::value, "");
    static_assert(std::is_same<uint8_t, StorageTypeT<8>>::value, "");

    static_assert(std::is_same<uint16_t, StorageTypeT<9>>::value, "");
    static_assert(std::is_same<uint16_t, StorageTypeT<16>>::value, "");

    static_assert(std::is_same<uint32_t, StorageTypeT<17>>::value, "");
    static_assert(std::is_same<uint32_t, StorageTypeT<32>>::value, "");

    static_assert(std::is_same<uint64_t, StorageTypeT<33>>::value, "");
    static_assert(std::is_same<uint64_t, StorageTypeT<60>>::value, "");
    static_assert(std::is_same<uint64_t, StorageTypeT<64>>::value, "");
}

TEST(FixedU4D8, whenGetMaxRepresentableFloatingPointValueIsCalledThenProperValueIsReturned) {
    constexpr float maxU4D8 = 15.99609375f;
    static_assert(maxU4D8 == FixedU4D8::getMaxRepresentableFloat(), "");
}

TEST(FixedU4D8, whenCreatingFromTooBigFloatThenValueIsClamped) {
    constexpr float maxU4D8 = FixedU4D8::getMaxRepresentableFloat();
    FixedU4D8 u4d8Max{maxU4D8};
    FixedU4D8 u4d8MaxPlus1{maxU4D8 + 1};
    EXPECT_EQ(u4d8Max.getRawAccess(), u4d8MaxPlus1.getRawAccess());
    EXPECT_EQ((1U << (4 + 8)) - 1, u4d8Max.getRawAccess()); // all 12 bits should be set
    EXPECT_EQ(maxU4D8, u4d8Max.asFloat());
}

TEST(FixedU4D8, whenCreatingFromNegativeFloatThenValueIsClamped) {
    FixedU4D8 u4d8Zero{0.0f};
    FixedU4D8 u4d8Minus1{-1.0f};
    EXPECT_EQ(u4d8Zero.getRawAccess(), u4d8Minus1.getRawAccess());
    EXPECT_EQ(0U, u4d8Zero.getRawAccess()); // all bits should be cleaned
    EXPECT_EQ(0.0f, u4d8Zero.asFloat());
}

TEST(FixedU4D8, whenCreatingFromRepresentableFloatThenCorrectValueIsPreserved) {
    constexpr float someValue = 3.5f;
    FixedU4D8 u4d8{someValue};
    EXPECT_EQ(someValue, u4d8.asFloat());
}
