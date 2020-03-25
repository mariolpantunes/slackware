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
#include "runtime/device/device.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/helpers/file_io.h"
#include "runtime/helpers/options.h"
#include "unit_tests/helpers/kernel_binary_helper.h"
#include "unit_tests/helpers/test_files.h"
#include "unit_tests/mocks/mock_kernel.h"

using namespace OCLRT;

struct clGetKernelWorkGroupInfoTests : public api_fixture,
                                       public ::testing::TestWithParam<uint32_t /*cl_kernel_work_group_info*/> {
    typedef api_fixture BaseClass;

    void SetUp() override {
        BaseClass::SetUp();

        void *pSource = nullptr;
        size_t sourceSize = 0;
        std::string testFile;

        kbHelper = new KernelBinaryHelper("CopyBuffer_simd8", false);
        testFile.append(clFiles);
        testFile.append("CopyBuffer_simd8.cl");
        ASSERT_EQ(true, fileExists(testFile));

        sourceSize = loadDataFromFile(
            testFile.c_str(),
            pSource);
        ASSERT_NE(0u, sourceSize);
        ASSERT_NE(nullptr, pSource);

        pProgram = clCreateProgramWithSource(
            pContext,
            1,
            (const char **)&pSource,
            &sourceSize,
            &retVal);
        EXPECT_NE(nullptr, pProgram);
        ASSERT_EQ(CL_SUCCESS, retVal);

        deleteDataReadFromFile(pSource);

        retVal = clBuildProgram(
            pProgram,
            num_devices,
            devices,
            nullptr,
            nullptr,
            nullptr);
        ASSERT_EQ(CL_SUCCESS, retVal);

        kernel = clCreateKernel(pProgram, "CopyBuffer", &retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);

        CompilerInterface::shutdown();
    }

    void TearDown() override {
        retVal = clReleaseKernel(kernel);
        EXPECT_EQ(CL_SUCCESS, retVal);

        retVal = clReleaseProgram(pProgram);
        EXPECT_EQ(CL_SUCCESS, retVal);

        delete kbHelper;
        BaseClass::TearDown();
    }

    cl_program pProgram = nullptr;
    cl_kernel kernel = nullptr;
    KernelBinaryHelper *kbHelper;
};

namespace ULT {

TEST_P(clGetKernelWorkGroupInfoTests, success) {

    size_t paramValueSizeRet;
    retVal = clGetKernelWorkGroupInfo(
        kernel,
        devices[0],
        GetParam(),
        0,
        nullptr,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(0u, paramValueSizeRet);
}

TEST_F(clGetKernelWorkGroupInfoTests, returnSpillMemSize) {
    size_t paramValueSizeRet;
    cl_ulong param_value;
    auto pDevice = castToObject<Device>(devices[0]);

    MockKernelWithInternals mockKernel(*pDevice);
    SPatchMediaVFEState mediaVFEstate;
    mediaVFEstate.PerThreadScratchSpace = 1024; //whatever greater than 0
    mockKernel.kernelInfo.patchInfo.mediavfestate = &mediaVFEstate;

    cl_ulong scratchSpaceSize = static_cast<cl_ulong>(mockKernel.mockKernel->getScratchSize());
    EXPECT_EQ(scratchSpaceSize, 1024u);

    retVal = clGetKernelWorkGroupInfo(
        mockKernel,
        pDevice,
        CL_KERNEL_SPILL_MEM_SIZE_INTEL,
        sizeof(cl_ulong),
        &param_value,
        &paramValueSizeRet);

    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(paramValueSizeRet, sizeof(cl_ulong));
    EXPECT_EQ(param_value, scratchSpaceSize);
}

TEST_F(clGetKernelWorkGroupInfoTests, givenKernelHavingPrivateMemoryAllocationWhenAskedForPrivateAllocationSizeThenProperSizeIsReturned) {
    size_t paramValueSizeRet;
    cl_ulong param_value;
    auto pDevice = castToObject<Device>(devices[0]);

    MockKernelWithInternals mockKernel(*pDevice);
    SPatchAllocateStatelessPrivateSurface privateAllocation;
    privateAllocation.PerThreadPrivateMemorySize = 1024;
    mockKernel.kernelInfo.patchInfo.pAllocateStatelessPrivateSurface = &privateAllocation;

    retVal = clGetKernelWorkGroupInfo(
        mockKernel,
        pDevice,
        CL_KERNEL_PRIVATE_MEM_SIZE,
        sizeof(cl_ulong),
        &param_value,
        &paramValueSizeRet);

    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(paramValueSizeRet, sizeof(cl_ulong));
    EXPECT_EQ(param_value, privateAllocation.PerThreadPrivateMemorySize);
}

TEST_F(clGetKernelWorkGroupInfoTests, givenKernelNotHavingPrivateMemoryAllocationWhenAskedForPrivateAllocationSizeThenZeroIsReturned) {
    size_t paramValueSizeRet;
    cl_ulong param_value;
    auto pDevice = castToObject<Device>(devices[0]);

    MockKernelWithInternals mockKernel(*pDevice);

    retVal = clGetKernelWorkGroupInfo(
        mockKernel,
        pDevice,
        CL_KERNEL_PRIVATE_MEM_SIZE,
        sizeof(cl_ulong),
        &param_value,
        &paramValueSizeRet);

    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(paramValueSizeRet, sizeof(cl_ulong));
    EXPECT_EQ(param_value, 0u);
}

static cl_kernel_work_group_info paramNames[] = {
    CL_KERNEL_WORK_GROUP_SIZE,
    CL_KERNEL_COMPILE_WORK_GROUP_SIZE,
    CL_KERNEL_LOCAL_MEM_SIZE,
    CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE,
    CL_KERNEL_SPILL_MEM_SIZE_INTEL,
    CL_KERNEL_PRIVATE_MEM_SIZE};

INSTANTIATE_TEST_CASE_P(
    api,
    clGetKernelWorkGroupInfoTests,
    testing::ValuesIn(paramNames));
} // namespace ULT
