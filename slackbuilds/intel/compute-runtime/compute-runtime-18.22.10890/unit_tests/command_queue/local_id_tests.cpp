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

#include "runtime/command_queue/local_id_gen.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/ptr_math.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <cstdint>

using namespace OCLRT;

TEST(LocalID, GRFsPerThread_SIMD8) {
    uint32_t simd = 8;
    EXPECT_EQ(1u, getGRFsPerThread(simd));
}

TEST(LocalID, GRFsPerThread_SIMD16) {
    uint32_t simd = 16;
    EXPECT_EQ(1u, getGRFsPerThread(simd));
}

TEST(LocalID, GRFsPerThread_SIMD32) {
    uint32_t simd = 32;
    EXPECT_EQ(2u, getGRFsPerThread(simd));
}

TEST(LocalID, ThreadsPerWorkgroup) {
    size_t lws = 33;
    uint32_t simd = 32;
    EXPECT_EQ(2u, getThreadsPerWG(simd, lws));
}

TEST(LocalID, PerThreadSizeLocalIDs_SIMD8) {
    uint32_t simd = 8;

    // 3 channels (x,y,z) * 1 GRFs per thread (@SIMD8)
    EXPECT_EQ(3 * sizeof(GRF), getPerThreadSizeLocalIDs(simd));
}

TEST(LocalID, PerThreadSizeLocalIDs_SIMD16) {
    uint32_t simd = 16;

    // 3 channels (x,y,z) * 1 GRFs per thread (@SIMD16)
    EXPECT_EQ(3 * sizeof(GRF), getPerThreadSizeLocalIDs(simd));
}

TEST(LocalID, PerThreadSizeLocalIDs_SIMD32) {
    uint32_t simd = 32;

    // 3 channels (x,y,z) * 2 GRFs per thread (@SIMD32)
    EXPECT_EQ(6 * sizeof(GRF), getPerThreadSizeLocalIDs(simd));
}

struct LocalIDFixture : public ::testing::TestWithParam<std::tuple<int, int, int, int>> {
    void SetUp() override {
        simd = std::get<0>(GetParam());
        localWorkSizeX = std::get<1>(GetParam());
        localWorkSizeY = std::get<2>(GetParam());
        localWorkSizeZ = std::get<3>(GetParam());

        localWorkSize = localWorkSizeX * localWorkSizeY * localWorkSizeZ;
        if (localWorkSize > 256) {
            localWorkSizeY = std::min(256 / localWorkSizeX, localWorkSizeY);
            localWorkSizeZ = std::min(256 / (localWorkSizeX * localWorkSizeY), localWorkSizeZ);
            localWorkSize = localWorkSizeX * localWorkSizeY * localWorkSizeZ;
        }

        const auto bufferSize = 32 * 3 * 16 * sizeof(uint16_t);
        buffer = reinterpret_cast<uint16_t *>(alignedMalloc(bufferSize, 32));
        memset(buffer, 0xff, bufferSize);
    }

    void TearDown() override {
        alignedFree(buffer);
    }

    void validateIDWithinLimits(uint32_t simd, uint32_t lwsX, uint32_t lwsY, uint32_t lwsZ) {
        auto idsPerThread = simd;

        // As per BackEnd HLD, SIMD32 has 32 localIDs per channel.  SIMD8/16 has up to 16 localIDs.
        auto skipPerThread = simd == 32 ? 32 : 16;

        auto pBufferX = buffer;
        auto pBufferY = pBufferX + skipPerThread;
        auto pBufferZ = pBufferY + skipPerThread;

        auto numWorkItems = lwsX * lwsY * lwsZ;

        size_t itemIndex = 0;
        while (numWorkItems > 0) {
            EXPECT_LT(pBufferX[itemIndex], lwsX) << simd << " " << lwsX << " " << lwsY << " " << lwsZ;
            EXPECT_LT(pBufferY[itemIndex], lwsY) << simd << " " << lwsX << " " << lwsY << " " << lwsZ;
            EXPECT_LT(pBufferZ[itemIndex], lwsZ) << simd << " " << lwsX << " " << lwsY << " " << lwsZ;
            ++itemIndex;
            if (idsPerThread == itemIndex) {
                pBufferX += skipPerThread * 3;
                pBufferY += skipPerThread * 3;
                pBufferZ += skipPerThread * 3;

                itemIndex = 0;
            }
            --numWorkItems;
        }
    }

    void validateAllWorkItemsCovered(uint32_t simd, uint32_t lwsX, uint32_t lwsY, uint32_t lwsZ) {
        auto idsPerThread = simd;

        // As per BackEnd HLD, SIMD32 has 32 localIDs per channel.  SIMD8/16 has up to 16 localIDs.
        auto skipPerThread = simd == 32 ? 32 : 16;

        auto pBufferX = buffer;
        auto pBufferY = pBufferX + skipPerThread;
        auto pBufferZ = pBufferY + skipPerThread;

        auto numWorkItems = lwsX * lwsY * lwsZ;

        // Initialize local ID hit table
        uint32_t localIDHitTable[8];
        memset(localIDHitTable, 0, sizeof(localIDHitTable));

        size_t itemIndex = 0;
        while (numWorkItems > 0) {
            // Flatten out the IDs
            auto workItem = pBufferX[itemIndex] + pBufferY[itemIndex] * lwsX + pBufferZ[itemIndex] * lwsX * lwsY;
            ASSERT_LT(workItem, 256u);

            // Look up in the hit table
            auto &hitItem = localIDHitTable[workItem / 32];
            auto hitBit = 1 << (workItem % 32);

            // No double-hits
            EXPECT_EQ(0u, hitItem & hitBit);

            // Set that work item as hit
            hitItem |= hitBit;

            ++itemIndex;
            if (idsPerThread == itemIndex) {
                pBufferX += skipPerThread * 3;
                pBufferY += skipPerThread * 3;
                pBufferZ += skipPerThread * 3;

                itemIndex = 0;
            }
            --numWorkItems;
        }

        // All entries in hit table should be in form of n^2 - 1
        for (uint32_t i : localIDHitTable) {
            EXPECT_EQ(0u, i & (i + 1));
        }
    }

    void dumpBuffer(uint32_t simd, uint32_t lwsX, uint32_t lwsY, uint32_t lwsZ) {
        auto workSize = lwsX * lwsY * lwsZ;
        auto threads = (workSize + simd - 1) / simd;

        auto pBuffer = buffer;

        // As per BackEnd HLD, SIMD32 has 32 localIDs per channel.  SIMD8/16 has up to 16 localIDs.
        auto skipPerThread = simd == 32 ? 32 : 16;

        while (threads-- > 0) {
            auto lanes = std::min(workSize, simd);

            for (auto dimension = 0u; dimension < 3u; ++dimension) {
                for (auto lane = 0u; lane < lanes; ++lane) {
                    printf("%04d ", (unsigned int)pBuffer[lane]);
                }
                pBuffer += skipPerThread;
                printf("\n");
            }

            workSize -= simd;
        }
    }

    // Test parameters
    uint32_t localWorkSizeX;
    uint32_t localWorkSizeY;
    uint32_t localWorkSizeZ;
    uint32_t localWorkSize;
    uint32_t simd;

    // Provide support for a max LWS of 256
    // 32 threads @ SIMD8
    // 3 channels (x/y/z)
    // 16 lanes per thread (SIMD8 - only 8 used)
    uint16_t *buffer;
};

TEST_P(LocalIDFixture, checkIDWithinLimits) {
    generateLocalIDs(buffer, simd, localWorkSizeX, localWorkSizeY, localWorkSizeZ);
    validateIDWithinLimits(simd, localWorkSizeX, localWorkSizeY, localWorkSizeZ);
}

TEST_P(LocalIDFixture, checkAllWorkItemsCovered) {
    generateLocalIDs(buffer, simd, localWorkSizeX, localWorkSizeY, localWorkSizeZ);
    validateAllWorkItemsCovered(simd, localWorkSizeX, localWorkSizeY, localWorkSizeZ);
}

TEST_P(LocalIDFixture, sizeCalculationLocalIDs) {
    auto workItems = localWorkSizeX * localWorkSizeY * localWorkSizeZ;
    auto sizeTotalPerThreadData = getThreadsPerWG(simd, workItems) * getPerThreadSizeLocalIDs(simd);

    // Should be multiple of GRFs
    auto sizeGRF = sizeof(GRF);
    EXPECT_EQ(0u, sizeTotalPerThreadData % sizeGRF);

    auto numGRFsPerThread = (simd == 32) ? 2 : 1;
    auto numThreadsExpected = (workItems + simd - 1) / simd;
    auto numGRFsExpected = 3 * numGRFsPerThread * numThreadsExpected;
    EXPECT_EQ(numGRFsExpected * sizeGRF, sizeTotalPerThreadData);
}

#define SIMDParams ::testing::Values(8, 16, 32)
#if HEAVY_DUTY_TESTING
#define LWSXParams ::testing::Values(1, 7, 8, 9, 15, 16, 17, 31, 32, 33, 64, 128, 256)
#define LWSYParams ::testing::Values(1, 2, 3, 4, 5, 6, 7, 8)
#define LWSZParams ::testing::Values(1, 2, 3, 4)
#else
#define LWSXParams ::testing::Values(1, 7, 8, 9, 15, 16, 17, 31, 32, 33, 64, 128, 256)
#define LWSYParams ::testing::Values(1, 2, 4, 8)
#define LWSZParams ::testing::Values(1)
#endif

INSTANTIATE_TEST_CASE_P(AllCombinations, LocalIDFixture, ::testing::Combine(SIMDParams, LWSXParams, LWSYParams, LWSZParams));

// To debug a specific configuration replace the list of Values with specific values.
// NOTE: You'll need a unique test prefix
INSTANTIATE_TEST_CASE_P(SingleTest, LocalIDFixture,
                        ::testing::Combine(
                            ::testing::Values(32),  //SIMD
                            ::testing::Values(5),   //LWSX
                            ::testing::Values(6),   //LWSY
                            ::testing::Values(7))); //LWSZ
