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

#include "cl_api_tests.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "test.h"

using namespace OCLRT;

class KernelArgSvmFixture : public api_fixture, public DeviceFixture {
  public:
    KernelArgSvmFixture()
        : pCrossThreadData{0} {
    }

  protected:
    void SetUp() override {
        api_fixture::SetUp();
        DeviceFixture::SetUp();

        // define kernel info
        pKernelInfo = KernelInfo::create();

        // setup kernel arg offsets
        KernelArgPatchInfo kernelArgPatchInfo;

        kernelHeader.SurfaceStateHeapSize = sizeof(pSshLocal);
        pKernelInfo->heapInfo.pSsh = pSshLocal;
        pKernelInfo->heapInfo.pKernelHeader = &kernelHeader;
        pKernelInfo->usesSsh = true;
        pKernelInfo->requiresSshForBuffers = true;

        pKernelInfo->kernelArgInfo.resize(1);
        pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);

        pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset = 0x30;
        pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].size = (uint32_t)sizeof(void *);
        pKernelInfo->kernelArgInfo[0].typeStr = "char *";
        pKernelInfo->kernelArgInfo[0].addressQualifier = CL_KERNEL_ARG_ADDRESS_GLOBAL;

        pMockKernel = new MockKernel(pProgram, *pKernelInfo, *pPlatform->getDevice(0));
        ASSERT_EQ(CL_SUCCESS, pMockKernel->initialize());
        pMockKernel->setCrossThreadData(pCrossThreadData, sizeof(pCrossThreadData));
    }

    void TearDown() override {
        delete pKernelInfo;
        delete pMockKernel;

        DeviceFixture::TearDown();
        api_fixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    MockKernel *pMockKernel = nullptr;
    KernelInfo *pKernelInfo = nullptr;
    SKernelBinaryHeaderCommon kernelHeader;
    char pSshLocal[64];
    char pCrossThreadData[64];
};

typedef Test<KernelArgSvmFixture> clSetKernelArgSVMPointer_;

namespace ULT {

TEST_F(clSetKernelArgSVMPointer_, SetKernelArgSVMPointer_invalidKernel) {
    auto retVal = clSetKernelArgSVMPointer(
        nullptr, // cl_kernel kernel
        0,       // cl_uint arg_index
        nullptr  // const void *arg_value
        );
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST_F(clSetKernelArgSVMPointer_, SetKernelArgSVMPointer_invalidArgIndex) {
    auto retVal = clSetKernelArgSVMPointer(
        pMockKernel, // cl_kernel kernel
        (cl_uint)-1, // cl_uint arg_index
        nullptr      // const void *arg_value
        );
    EXPECT_EQ(CL_INVALID_ARG_INDEX, retVal);
}

TEST_F(clSetKernelArgSVMPointer_, SetKernelArgSVMPointer_invalidArgAddressQualifier) {
    pKernelInfo->kernelArgInfo[0].addressQualifier = CL_KERNEL_ARG_ADDRESS_LOCAL;

    auto retVal = clSetKernelArgSVMPointer(
        pMockKernel, // cl_kernel kernel
        0,           // cl_uint arg_index
        nullptr      // const void *arg_value
        );
    EXPECT_EQ(CL_INVALID_ARG_VALUE, retVal);
}

TEST_F(clSetKernelArgSVMPointer_, SetKernelArgSVMPointer_invalidArgValue) {
    void *ptrHost = malloc(256);
    EXPECT_NE(nullptr, ptrHost);

    auto retVal = clSetKernelArgSVMPointer(
        pMockKernel, // cl_kernel kernel
        0,           // cl_uint arg_index
        ptrHost      // const void *arg_value
        );
    EXPECT_EQ(CL_INVALID_ARG_VALUE, retVal);

    free(ptrHost);
}

TEST_F(clSetKernelArgSVMPointer_, SetKernelArgSVMPointerWithNullArgValue_success) {
    auto retVal = clSetKernelArgSVMPointer(
        pMockKernel, // cl_kernel kernel
        0,           // cl_uint arg_index
        nullptr      // const void *arg_value
        );
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clSetKernelArgSVMPointer_, SetKernelArgSVMPointer_success) {
    const DeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, ptrSvm);

        auto retVal = clSetKernelArgSVMPointer(
            pMockKernel, // cl_kernel kernel
            0,           // cl_uint arg_index
            ptrSvm       // const void *arg_value
            );
        EXPECT_EQ(CL_SUCCESS, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}

TEST_F(clSetKernelArgSVMPointer_, SetKernelArgSVMPointerConst_success) {
    const DeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        EXPECT_NE(nullptr, ptrSvm);

        pKernelInfo->kernelArgInfo[0].addressQualifier = CL_KERNEL_ARG_ADDRESS_CONSTANT;

        auto retVal = clSetKernelArgSVMPointer(
            pMockKernel, // cl_kernel kernel
            0,           // cl_uint arg_index
            ptrSvm       // const void *arg_value
            );
        EXPECT_EQ(CL_SUCCESS, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}

TEST_F(clSetKernelArgSVMPointer_, SetKernelArgSVMPointerWithOffset_success) {
    const DeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        size_t offset = 256 / 2;
        EXPECT_NE(nullptr, ptrSvm);

        auto retVal = clSetKernelArgSVMPointer(
            pMockKernel,            // cl_kernel kernel
            0,                      // cl_uint arg_index
            (char *)ptrSvm + offset // const void *arg_value
            );
        EXPECT_EQ(CL_SUCCESS, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}

TEST_F(clSetKernelArgSVMPointer_, SetKernelArgSVMPointerWithOffset_invalidArgValue) {
    const DeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        void *ptrSvm = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 256, 4);
        size_t offset = alignUp(512, MemoryConstants::pageSize) + 1;
        EXPECT_NE(nullptr, ptrSvm);

        auto retVal = clSetKernelArgSVMPointer(
            pMockKernel,            // cl_kernel kernel
            0,                      // cl_uint arg_index
            (char *)ptrSvm + offset // const void *arg_value
            );
        EXPECT_EQ(CL_INVALID_ARG_VALUE, retVal);

        clSVMFree(pContext, ptrSvm);
    }
}
} // namespace ULT
