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

#include "runtime/helpers/basic_math.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/kernel/kernel.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "test.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"
#include "gtest/gtest.h"

using namespace OCLRT;

class KernelSlmArgTest : public Test<DeviceFixture> {
  public:
    KernelSlmArgTest() {
    }

  protected:
    void SetUp() override {
        DeviceFixture::SetUp();
        pKernelInfo = KernelInfo::create();
        KernelArgPatchInfo kernelArgPatchInfo;

        pKernelInfo->kernelArgInfo.resize(3);
        pKernelInfo->kernelArgInfo[2].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
        pKernelInfo->kernelArgInfo[1].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
        pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);

        pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset = 0x10;
        pKernelInfo->kernelArgInfo[0].slmAlignment = 0x1;
        pKernelInfo->kernelArgInfo[1].kernelArgPatchInfoVector[0].crossthreadOffset = 0x20;
        pKernelInfo->kernelArgInfo[1].kernelArgPatchInfoVector[0].size = sizeof(void *);
        pKernelInfo->kernelArgInfo[2].kernelArgPatchInfoVector[0].crossthreadOffset = 0x30;
        pKernelInfo->kernelArgInfo[2].slmAlignment = 0x400;
        pKernelInfo->workloadInfo.slmStaticSize = 3 * KB;

        pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);
        ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

        pKernel->setKernelArgHandler(0, &Kernel::setArgLocal);
        pKernel->setKernelArgHandler(1, &Kernel::setArgImmediate);
        pKernel->setKernelArgHandler(2, &Kernel::setArgLocal);

        uint32_t crossThreadData[0x40] = {};
        crossThreadData[0x20 / sizeof(uint32_t)] = 0x12344321;
        pKernel->setCrossThreadData(crossThreadData, sizeof(crossThreadData));
    }

    void TearDown() override {
        delete pKernelInfo;
        delete pKernel;
        DeviceFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    MockProgram program;
    MockKernel *pKernel = nullptr;
    KernelInfo *pKernelInfo;

    static const size_t slmSize0 = 0x200;
    static const size_t slmSize2 = 0x30;
};

TEST_F(KernelSlmArgTest, settingSizeUpdatesAlignmentOfHigherSlmArgs) {
    pKernel->setArg(0, slmSize0, nullptr);
    pKernel->setArg(2, slmSize2, nullptr);

    auto crossThreadData = reinterpret_cast<uint32_t *>(pKernel->getCrossThreadData());
    auto slmOffset = ptrOffset(crossThreadData, 0x10);
    EXPECT_EQ(0u, *slmOffset);

    slmOffset = ptrOffset(crossThreadData, 0x20);
    EXPECT_EQ(0x12344321u, *slmOffset);

    slmOffset = ptrOffset(crossThreadData, 0x30);
    EXPECT_EQ(0x400u, *slmOffset);

    EXPECT_EQ(5 * KB, pKernel->slmTotalSize);
}

TEST_F(KernelSlmArgTest, settingSizeUpdatesAlignmentOfHigherSlmArgsReverseOrder) {
    pKernel->setArg(2, slmSize2, nullptr);
    pKernel->setArg(0, slmSize0, nullptr);

    auto crossThreadData = reinterpret_cast<uint32_t *>(pKernel->getCrossThreadData());
    auto slmOffset = ptrOffset(crossThreadData, 0x10);
    EXPECT_EQ(0u, *slmOffset);

    slmOffset = ptrOffset(crossThreadData, 0x20);
    EXPECT_EQ(0x12344321u, *slmOffset);

    slmOffset = ptrOffset(crossThreadData, 0x30);
    EXPECT_EQ(0x400u, *slmOffset);

    EXPECT_EQ(5 * KB, pKernel->slmTotalSize);
}
