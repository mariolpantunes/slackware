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

#include "runtime/accelerators/intel_accelerator.h"
#include "runtime/accelerators/intel_motion_estimation.h"
#include "runtime/context/context.h"
#include "unit_tests/api/cl_api_tests.h"

using namespace OCLRT;

namespace ULT {

struct IntelAcceleratorTest : public api_tests {
  public:
    IntelAcceleratorTest() {}

    void SetUp() override {
        api_tests::SetUp();
    }

    void TearDown() override {
        api_tests::TearDown();
    }

  protected:
    cl_accelerator_intel accelerator = nullptr;
    cl_motion_estimation_desc_intel desc;
    cl_int retVal = 0xEEEEEEEEu;
    cl_int result = -1;
};

struct IntelAcceleratorTestWithValidDescriptor : IntelAcceleratorTest {

    IntelAcceleratorTestWithValidDescriptor() {}

    void SetUp() override {
        IntelAcceleratorTest::SetUp();

        desc.mb_block_type = CL_ME_MB_TYPE_16x16_INTEL;
        desc.subpixel_mode = CL_ME_SUBPIXEL_MODE_QPEL_INTEL;
        desc.sad_adjust_mode = CL_ME_SAD_ADJUST_MODE_HAAR_INTEL;
        desc.search_path_type = CL_ME_SEARCH_PATH_RADIUS_2_2_INTEL;
    }

    void TearDown() override {
        IntelAcceleratorTest::TearDown();
    }
};

TEST_F(IntelAcceleratorTestWithValidDescriptor, GivenInvalidAcceleratorExpectFail) {
    auto INVALID_ACCELERATOR_TYPE = static_cast<cl_accelerator_type_intel>(0xEEEEEEEE);

    accelerator = clCreateAcceleratorINTEL(
        pContext,
        INVALID_ACCELERATOR_TYPE,
        sizeof(cl_motion_estimation_desc_intel),
        &desc,
        &retVal);

    EXPECT_EQ(static_cast<cl_accelerator_intel>(nullptr), accelerator);
    EXPECT_EQ(CL_INVALID_ACCELERATOR_TYPE_INTEL, retVal);
}

TEST_F(IntelAcceleratorTestWithValidDescriptor, GivenInvalidContextThenFailApiCreate) {
    accelerator = clCreateAcceleratorINTEL(
        nullptr,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
        sizeof(cl_motion_estimation_desc_intel),
        &desc,
        &retVal);
    EXPECT_EQ(static_cast<cl_accelerator_intel>(nullptr), accelerator);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(IntelAcceleratorTest, GivenNullAcceleratorThenFailApiRelease) {
    result = clReleaseAcceleratorINTEL(nullptr);
    EXPECT_EQ(CL_INVALID_ACCELERATOR_INTEL, result);
}

TEST_F(IntelAcceleratorTest, GivenNullAcceleratorThenFailApiRetain) {
    result = clRetainAcceleratorINTEL(nullptr);
    EXPECT_EQ(CL_INVALID_ACCELERATOR_INTEL, result);
}

struct IntelAcceleratorGetInfoTest : IntelAcceleratorTestWithValidDescriptor {

    IntelAcceleratorGetInfoTest() {}

    void SetUp() override {
        IntelAcceleratorTestWithValidDescriptor::SetUp();

        accelerator = clCreateAcceleratorINTEL(
            pContext,
            CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
            sizeof(cl_motion_estimation_desc_intel),
            &desc,
            &retVal);

        ASSERT_NE(nullptr, accelerator);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    void TearDown() override {
        result = clReleaseAcceleratorINTEL(accelerator);
        ASSERT_EQ(CL_SUCCESS, result);

        IntelAcceleratorTestWithValidDescriptor::TearDown();
    }

  protected:
    size_t param_value_size_ret = 0;
};

TEST_F(IntelAcceleratorTest, GivenNullAcceleratorThenFailApiGetInfo) {
    result = clGetAcceleratorInfoINTEL(
        nullptr, 0,
        0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_ACCELERATOR_INTEL, result);
}

TEST_F(IntelAcceleratorTest, GivenNullAcceleratorWithReturnsThenFailApiGetInfo) {
    cl_uint paramValue = 0xEEEEEEE1u;
    size_t paramSize = 0xEEEEEEE3u;

    result = clGetAcceleratorInfoINTEL(
        nullptr, 0,
        sizeof(paramValue), &paramValue, &paramSize);

    EXPECT_EQ(CL_INVALID_ACCELERATOR_INTEL, result);

    // No changes to inputs
    EXPECT_EQ(static_cast<cl_uint>(0xEEEEEEE1u), paramValue);
    EXPECT_EQ(0xEEEEEEE3u, paramSize);
}

TEST_F(IntelAcceleratorGetInfoTest, GetInfoWithBogusParam) {
    result = clGetAcceleratorInfoINTEL(
        accelerator,
        0xEEEEEEEE,
        sizeof(cl_uint),
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, result);
}

TEST_F(IntelAcceleratorGetInfoTest, GetInfoReferenceCountWithNullReturnExpectPass) {
    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_REFERENCE_COUNT_INTEL,
        sizeof(cl_uint),
        nullptr,
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(cl_uint), param_value_size_ret);
}

TEST_F(IntelAcceleratorGetInfoTest, GetInfoReferenceCount1ExpectPass) {
    cl_uint parm_value = static_cast<cl_uint>(-1);

    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_REFERENCE_COUNT_INTEL,
        sizeof(cl_uint),
        reinterpret_cast<void *>(&parm_value),
        nullptr);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(1u, parm_value);
}

TEST_F(IntelAcceleratorGetInfoTest, GetInfoReferenceCountWithLongReturnExpectPass) {
    cl_uint parm_value = static_cast<cl_uint>(-1);

    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_REFERENCE_COUNT_INTEL,
        sizeof(cl_uint) + 1,
        reinterpret_cast<void *>(&parm_value),
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(cl_uint), param_value_size_ret);
    EXPECT_EQ(1u, parm_value);
}

TEST_F(IntelAcceleratorGetInfoTest, GetInfoReferenceCountWithShortReturnExpectFail) {
    cl_uint parm_value = static_cast<cl_uint>(-1);

    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_REFERENCE_COUNT_INTEL,
        sizeof(cl_uint) - 1,
        reinterpret_cast<void *>(&parm_value),
        &param_value_size_ret);

    EXPECT_EQ(CL_INVALID_VALUE, result);
}

TEST_F(IntelAcceleratorGetInfoTest, GetInfoReferenceCountSizeQueryExpectPass) {
    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_REFERENCE_COUNT_INTEL,
        0,
        nullptr,
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(cl_uint), param_value_size_ret);
}

TEST_F(IntelAcceleratorGetInfoTest, GetInfoReferenceCount2ExpectPass) {
    cl_uint parm_value = static_cast<cl_uint>(-1);

    result = clRetainAcceleratorINTEL(accelerator);
    ASSERT_EQ(CL_SUCCESS, result);

    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_REFERENCE_COUNT_INTEL,
        sizeof(cl_uint),
        reinterpret_cast<void *>(&parm_value),
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(2u, parm_value);

    result = clReleaseAcceleratorINTEL(accelerator);

    EXPECT_EQ(CL_SUCCESS, result);

    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_REFERENCE_COUNT_INTEL,
        sizeof(cl_uint),
        reinterpret_cast<void *>(&parm_value),
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(1u, parm_value);
}

TEST_F(IntelAcceleratorGetInfoTest, GetInfoContextNullReturnExpectPass) {
    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_CONTEXT_INTEL,
        sizeof(cl_context),
        nullptr,
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(cl_context), param_value_size_ret);
}

TEST_F(IntelAcceleratorGetInfoTest, GetInfoContextLongExpectPass) {
    cl_context parm_value = reinterpret_cast<cl_context>(-1);

    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_CONTEXT_INTEL,
        sizeof(cl_context) + 1,
        reinterpret_cast<void *>(&parm_value),
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(cl_context), param_value_size_ret);
}

TEST_F(IntelAcceleratorGetInfoTest, GetInfoContextLongExpectRightContext) {
    cl_context parm_value = reinterpret_cast<cl_context>(-1);

    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_CONTEXT_INTEL,
        sizeof(cl_context),
        reinterpret_cast<void *>(&parm_value),
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(cl_context), param_value_size_ret);

    cl_context referenceContext = static_cast<cl_context>(pContext);
    EXPECT_EQ(referenceContext, parm_value);
}

TEST_F(IntelAcceleratorGetInfoTest, GetInfoContextShortExpectPass) {
    cl_context parm_value = reinterpret_cast<cl_context>(-1);

    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_CONTEXT_INTEL,
        sizeof(cl_context) - 1,
        reinterpret_cast<void *>(&parm_value),
        &param_value_size_ret);

    EXPECT_EQ(CL_INVALID_VALUE, result);
    EXPECT_EQ(sizeof(cl_context), param_value_size_ret);
}

TEST_F(IntelAcceleratorGetInfoTest, GetInfoContextSizeQueryExpectPass) {
    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_CONTEXT_INTEL,
        0,
        nullptr,
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(cl_context), param_value_size_ret);
}
} // namespace ULT
