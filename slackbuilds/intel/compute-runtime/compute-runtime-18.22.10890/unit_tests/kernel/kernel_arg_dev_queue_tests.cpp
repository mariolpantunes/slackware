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

#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/device_host_queue_fixture.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/mocks/mock_buffer.h"

using namespace OCLRT;
using namespace DeviceHostQueue;

struct KernelArgDevQueueTest : public DeviceFixture,
                               public DeviceHostQueueFixture<DeviceQueue> {
    KernelArgDevQueueTest() : program(), kernelArgPatchInfo() {}

  protected:
    void SetUp() override {
        DeviceFixture::SetUp();
        DeviceHostQueueFixture<DeviceQueue>::SetUp();

        pDeviceQueue = createQueueObject();

        pKernelInfo = KernelInfo::create();
        pKernelInfo->kernelArgInfo.resize(1);
        pKernelInfo->kernelArgInfo[0].isDeviceQueue = true;

        kernelArgPatchInfo.crossthreadOffset = 0x4;
        kernelArgPatchInfo.size = 0x4;
        kernelArgPatchInfo.sourceOffset = 0;

        pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);

        pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);
        ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

        uint8_t pCrossThreadData[crossThreadDataSize];
        memset(pCrossThreadData, crossThreadDataInit, sizeof(pCrossThreadData));
        pKernel->setCrossThreadData(pCrossThreadData, sizeof(pCrossThreadData));
    }

    void TearDown() override {
        delete pKernelInfo;
        delete pKernel;
        delete pDeviceQueue;

        DeviceHostQueueFixture<DeviceQueue>::TearDown();
        DeviceFixture::TearDown();
    }

    bool crossThreadDataUnchanged() {
        for (uint32_t i = 0; i < crossThreadDataSize; i++) {
            if (pKernel->mockCrossThreadData[i] != crossThreadDataInit) {
                return false;
            }
        }

        return true;
    }

    static const uint32_t crossThreadDataSize = 0x10;
    static const char crossThreadDataInit = 0x7e;

    MockProgram program;
    DeviceQueue *pDeviceQueue = nullptr;
    MockKernel *pKernel = nullptr;
    KernelInfo *pKernelInfo = nullptr;
    KernelArgPatchInfo kernelArgPatchInfo;
};

TEST_F(KernelArgDevQueueTest, GIVENkernelWithDevQueueArgWHENsetArgHandleTHENsetsProperHandle) {
    EXPECT_EQ(pKernel->kernelArgHandlers[0], &Kernel::setArgDevQueue);
}

TEST_F(KernelArgDevQueueTest, GIVENdevQueueArgHandlerWHENpassDevQueueTHENacceptObjAndPatch) {
    auto clDeviceQueue = static_cast<cl_command_queue>(pDeviceQueue);

    auto ret = pKernel->setArgDevQueue(0, sizeof(cl_command_queue), &clDeviceQueue);
    EXPECT_EQ(ret, CL_SUCCESS);

    auto gpuAddress = static_cast<uint32_t>(pDeviceQueue->getQueueBuffer()->getGpuAddressToPatch());
    auto patchLocation = ptrOffset(pKernel->mockCrossThreadData.data(), kernelArgPatchInfo.crossthreadOffset);
    EXPECT_EQ(*(reinterpret_cast<uint32_t *>(patchLocation)), gpuAddress);
}

TEST_F(KernelArgDevQueueTest, GIVENdevQueueArgHandlerWHENpassNormalQueueTHENrejectObjAndReturnError) {
    auto clCmdQueue = static_cast<cl_command_queue>(pCommandQueue);

    auto ret = pKernel->setArgDevQueue(0, sizeof(cl_command_queue), &clCmdQueue);
    EXPECT_EQ(ret, CL_INVALID_DEVICE_QUEUE);
    EXPECT_EQ(crossThreadDataUnchanged(), true);
}

TEST_F(KernelArgDevQueueTest, GIVENdevQueueArgHandlerWHENpassNonQueueObjTHENrejectObjAndReturnError) {
    Buffer *buffer = new MockBuffer();
    auto clBuffer = static_cast<cl_mem>(buffer);

    auto ret = pKernel->setArgDevQueue(0, sizeof(cl_command_queue), &clBuffer);
    EXPECT_EQ(ret, CL_INVALID_DEVICE_QUEUE);
    EXPECT_EQ(crossThreadDataUnchanged(), true);

    delete buffer;
}

TEST_F(KernelArgDevQueueTest, GIVENdevQueueArgHandlerWHENpassFakeQueueTHENrejectObjAndReturnError) {
    char *pFakeDeviceQueue = new char[sizeof(DeviceQueue)];
    auto clFakeDeviceQueue = reinterpret_cast<cl_command_queue *>(pFakeDeviceQueue);

    auto ret = pKernel->setArgDevQueue(0, sizeof(cl_command_queue), &clFakeDeviceQueue);
    EXPECT_EQ(ret, CL_INVALID_DEVICE_QUEUE);
    EXPECT_EQ(crossThreadDataUnchanged(), true);

    delete[] pFakeDeviceQueue;
}

TEST_F(KernelArgDevQueueTest, GIVENdevQueueArgHandlerWHENpassNullptrTHENrejectObjAndReturnError) {
    auto ret = pKernel->setArgDevQueue(0, sizeof(cl_command_queue), nullptr);
    EXPECT_EQ(ret, CL_INVALID_ARG_VALUE);
    EXPECT_EQ(crossThreadDataUnchanged(), true);
}

TEST_F(KernelArgDevQueueTest, GIVENdevQueueArgHandlerWHENpassWrongSizeTHENrejectObjAndReturnError) {
    auto clDeviceQueue = static_cast<cl_command_queue>(pDeviceQueue);

    auto ret = pKernel->setArgDevQueue(0, sizeof(cl_command_queue) - 1, &clDeviceQueue);
    EXPECT_EQ(ret, CL_INVALID_ARG_SIZE);
    EXPECT_EQ(crossThreadDataUnchanged(), true);
}
