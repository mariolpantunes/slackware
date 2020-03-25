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

#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/uint16_sse4.h"
#include "gtest/gtest.h"

using namespace OCLRT;

TEST(Uint16_Sse4, booleanOperator) {
    EXPECT_TRUE(static_cast<bool>(uint16x8_t::mask()));
    EXPECT_FALSE(static_cast<bool>(uint16x8_t::zero()));
}

TEST(Uint16_Sse4, logicalAnd) {
    EXPECT_TRUE(uint16x8_t::mask() && uint16x8_t::mask());
    EXPECT_FALSE(uint16x8_t::mask() && uint16x8_t::zero());
    EXPECT_FALSE(uint16x8_t::zero() && uint16x8_t::mask());
    EXPECT_FALSE(uint16x8_t::zero() && uint16x8_t::zero());
}

TEST(Uint16_Sse4, constructor) {
    auto one = uint16x8_t::one();
    uint16x8_t alsoOne(one.value);
    EXPECT_EQ(0, memcmp(&alsoOne, &one, sizeof(uint16x8_t)));
}

TEST(Uint16_Sse4, replicatingConstructor) {
    uint16x8_t allSevens(7u);
    for (int i = 0; i < uint16x8_t::numChannels; ++i) {
        EXPECT_EQ(7u, allSevens.get(i));
    }
}

ALIGNAS(32)
static const uint16_t laneValues[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};

TEST(Uint16_Sse4, pointerConstructor) {
    uint16x8_t lanes(laneValues);
    for (int i = 0; i < uint16x8_t::numChannels; ++i) {
        EXPECT_EQ(static_cast<uint16_t>(i), lanes.get(i));
    }
}

TEST(Uint16_Sse4, load) {
    uint16x8_t lanes;
    lanes.load(laneValues);
    for (int i = 0; i < uint16x8_t::numChannels; ++i) {
        EXPECT_EQ(static_cast<uint16_t>(i), lanes.get(i));
    }
}

TEST(Uint16_Sse4, loadUnaligned) {
    uint16x8_t lanes;
    lanes.loadUnaligned(laneValues + 1);
    for (int i = 0; i < uint16x8_t::numChannels; ++i) {
        EXPECT_EQ(static_cast<uint16_t>(i + 1), lanes.get(i));
    }
}

TEST(Uint16_Sse4, store) {
    uint16_t *alignedMemory = reinterpret_cast<uint16_t *>(alignedMalloc(1024, 32));

    uint16x8_t lanes(laneValues);
    lanes.store(alignedMemory);
    for (int i = 0; i < uint16x8_t::numChannels; ++i) {
        EXPECT_EQ(static_cast<uint16_t>(i), alignedMemory[i]);
    }

    alignedFree(alignedMemory);
}

TEST(Uint16_Sse4, storeUnaligned) {
    uint16_t *alignedMemory = reinterpret_cast<uint16_t *>(alignedMalloc(1024, 32));

    uint16x8_t lanes(laneValues);
    lanes.storeUnaligned(alignedMemory + 1);
    for (int i = 0; i < uint16x8_t::numChannels; ++i) {
        EXPECT_EQ(static_cast<uint16_t>(i), (alignedMemory + 1)[i]);
    }

    alignedFree(alignedMemory);
}

TEST(Uint16_Sse4, decrementingAssignmentOperator) {
    uint16x8_t result(laneValues);
    result -= uint16x8_t::one();

    for (int i = 0; i < uint16x8_t::numChannels; ++i) {
        EXPECT_EQ(static_cast<uint16_t>(i - 1), result.get(i));
    }
}

TEST(Uint16_Sse4, incrementingAssignmentOperator) {
    uint16x8_t result(laneValues);
    result += uint16x8_t::one();

    for (int i = 0; i < uint16x8_t::numChannels; ++i) {
        EXPECT_EQ(static_cast<uint16_t>(i + 1), result.get(i));
    }
}

TEST(Uint16_Sse4, blend) {
    uint16x8_t a(uint16x8_t::one());
    uint16x8_t b(uint16x8_t::zero());
    uint16x8_t c;

    // c = mask ? a : b
    c = blend(a, b, uint16x8_t::mask());

    for (int i = 0; i < uint16x8_t::numChannels; ++i) {
        EXPECT_EQ(a.get(i), c.get(i));
    }

    // c = mask ? a : b
    c = blend(a, b, uint16x8_t::zero());

    for (int i = 0; i < uint16x8_t::numChannels; ++i) {
        EXPECT_EQ(b.get(i), c.get(i));
    }
}
