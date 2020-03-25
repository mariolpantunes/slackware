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

#include "CL/cl.h"
#include "runtime/kernel/kernel.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "test.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"
#include "gtest/gtest.h"

using namespace OCLRT;

template <typename T>
class KernelArgImmediateTest : public Test<DeviceFixture> {
  public:
    KernelArgImmediateTest() {
    }

  protected:
    void SetUp() override {
        DeviceFixture::SetUp();
        memset(pCrossThreadData, 0xfe, sizeof(pCrossThreadData));

        // define kernel info
        pKernelInfo = KernelInfo::create();

        // setup kernel arg offsets
        KernelArgPatchInfo kernelArgPatchInfo;

        pKernelInfo->kernelArgInfo.resize(4);
        pKernelInfo->kernelArgInfo[3].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
        pKernelInfo->kernelArgInfo[3].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
        pKernelInfo->kernelArgInfo[2].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
        pKernelInfo->kernelArgInfo[1].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
        pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);

        pKernelInfo->kernelArgInfo[3].kernelArgPatchInfoVector[1].crossthreadOffset = 0x28;
        pKernelInfo->kernelArgInfo[3].kernelArgPatchInfoVector[0].crossthreadOffset = 0x20;
        pKernelInfo->kernelArgInfo[2].kernelArgPatchInfoVector[0].crossthreadOffset = 0x30;
        pKernelInfo->kernelArgInfo[1].kernelArgPatchInfoVector[0].crossthreadOffset = 0x40;
        pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset = 0x50;
        pKernelInfo->kernelArgInfo[3].kernelArgPatchInfoVector[1].size = sizeof(T);
        pKernelInfo->kernelArgInfo[3].kernelArgPatchInfoVector[0].size = sizeof(T);
        pKernelInfo->kernelArgInfo[2].kernelArgPatchInfoVector[0].size = sizeof(T);
        pKernelInfo->kernelArgInfo[1].kernelArgPatchInfoVector[0].size = sizeof(T);
        pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].size = sizeof(T);

        pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);
        ASSERT_EQ(CL_SUCCESS, pKernel->initialize());
        pKernel->setCrossThreadData(pCrossThreadData, sizeof(pCrossThreadData));

        pKernel->setKernelArgHandler(0, &Kernel::setArgImmediate);
        pKernel->setKernelArgHandler(1, &Kernel::setArgImmediate);
        pKernel->setKernelArgHandler(2, &Kernel::setArgImmediate);
        pKernel->setKernelArgHandler(3, &Kernel::setArgImmediate);
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
    char pCrossThreadData[0x60];
};

typedef ::testing::Types<
    char,
    float,
    int,
    short,
    long,
    unsigned char,
    unsigned int,
    unsigned short,
    unsigned long>
    KernelArgImmediateTypes;

TYPED_TEST_CASE(KernelArgImmediateTest, KernelArgImmediateTypes);

TYPED_TEST(KernelArgImmediateTest, SetKernelArg) {
    auto val = (TypeParam)0xaaaaaaaaULL;
    auto pVal = &val;
    this->pKernel->setArg(0, sizeof(TypeParam), pVal);

    auto pKernelArg = (TypeParam *)(this->pKernel->getCrossThreadData() +
                                    this->pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);

    EXPECT_EQ(val, *pKernelArg);
}

TYPED_TEST(KernelArgImmediateTest, SetKernelArgWithInvalidIndex) {
    auto val = (TypeParam)0U;
    auto pVal = &val;
    auto ret = this->pKernel->setArg((uint32_t)-1, sizeof(TypeParam), pVal);

    EXPECT_EQ(ret, CL_INVALID_ARG_INDEX);
}

TYPED_TEST(KernelArgImmediateTest, setKernelArgMultipleArguments) {
    auto val = (TypeParam)0xaaaaaaaaULL;
    auto pVal = &val;
    this->pKernel->setArg(0, sizeof(TypeParam), pVal);

    auto pKernelArg = (TypeParam *)(this->pKernel->getCrossThreadData() +
                                    this->pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);

    EXPECT_EQ(val, *pKernelArg);

    val = (TypeParam)0xbbbbbbbbULL;
    this->pKernel->setArg(1, sizeof(TypeParam), &val);

    pKernelArg = (TypeParam *)(this->pKernel->getCrossThreadData() +
                               this->pKernelInfo->kernelArgInfo[1].kernelArgPatchInfoVector[0].crossthreadOffset);

    EXPECT_EQ(val, *pKernelArg);

    val = (TypeParam)0xccccccccULL;
    this->pKernel->setArg(2, sizeof(TypeParam), &val);

    pKernelArg = (TypeParam *)(this->pKernel->getCrossThreadData() +
                               this->pKernelInfo->kernelArgInfo[2].kernelArgPatchInfoVector[0].crossthreadOffset);

    EXPECT_EQ(val, *pKernelArg);
}

TYPED_TEST(KernelArgImmediateTest, setKernelArgOverwritesCrossThreadData) {
    TypeParam val = (TypeParam)0xaaaaaaaaULL;
    TypeParam *pVal = &val;
    this->pKernel->setArg(0, sizeof(TypeParam), pVal);

    TypeParam *pKernelArg = (TypeParam *)(this->pKernel->getCrossThreadData() +
                                          this->pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);

    EXPECT_EQ(val, *pKernelArg);

    val = (TypeParam)0xbbbbbbbbULL;
    this->pKernel->setArg(1, sizeof(TypeParam), &val);

    pKernelArg = (TypeParam *)(this->pKernel->getCrossThreadData() +
                               this->pKernelInfo->kernelArgInfo[1].kernelArgPatchInfoVector[0].crossthreadOffset);

    EXPECT_EQ(val, *pKernelArg);

    val = (TypeParam)0xccccccccULL;
    this->pKernel->setArg(0, sizeof(TypeParam), &val);

    pKernelArg = (TypeParam *)(this->pKernel->getCrossThreadData() +
                               this->pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);

    EXPECT_EQ(val, *pKernelArg);
}

TYPED_TEST(KernelArgImmediateTest, setSingleKernelArgMultipleStructElements) {
    struct ImmediateStruct {
        TypeParam a;
        unsigned char unused[3]; // want to force a gap, ideally unpadded
        TypeParam b;
    } immediateStruct;

    immediateStruct.a = (TypeParam)0xaaaaaaaaULL;
    immediateStruct.b = (TypeParam)0xbbbbbbbbULL;
    immediateStruct.unused[0] = 0xfe;
    immediateStruct.unused[1] = 0xfe;
    immediateStruct.unused[2] = 0xfe;

    this->pKernelInfo->kernelArgInfo[3].kernelArgPatchInfoVector[0].sourceOffset = offsetof(struct ImmediateStruct, a);
    this->pKernelInfo->kernelArgInfo[3].kernelArgPatchInfoVector[1].sourceOffset = offsetof(struct ImmediateStruct, b);

    this->pKernel->setArg(3, sizeof(immediateStruct), &immediateStruct);

    auto pCrossthreadA = (TypeParam *)(this->pKernel->getCrossThreadData() +
                                       this->pKernelInfo->kernelArgInfo[3].kernelArgPatchInfoVector[0].crossthreadOffset);
    EXPECT_EQ(immediateStruct.a, *pCrossthreadA);

    auto pCrossthreadB = (TypeParam *)(this->pKernel->getCrossThreadData() +
                                       this->pKernelInfo->kernelArgInfo[3].kernelArgPatchInfoVector[1].crossthreadOffset);
    EXPECT_EQ(immediateStruct.b, *pCrossthreadB);
}
