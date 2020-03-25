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

#include "runtime/helpers/get_info.h"
#include "gtest/gtest.h"

TEST(getInfo, valid_params_returnsSuccess) {
    float dest = 0.0f;
    float src = 1.0f;

    auto retVal = getInfo(&dest, sizeof(dest), &src, sizeof(src));
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(src, dest);
}

TEST(getInfo, null_param_and_param_size_too_small_returnsSuccess) {
    float dest = 0.0f;
    float src = 1.0f;

    auto retVal = getInfo(nullptr, 0, &src, sizeof(src));
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(src, dest);
}

TEST(getInfo, GivenNullPtrAsValueAndNonZeroSizeWhenAskedForGetInfoThenSuccessIsReturned) {
    float dest = 0.0f;
    float src = 1.0f;

    auto retVal = getInfo(nullptr, sizeof(dest), &src, sizeof(src));
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(src, dest);
}

TEST(getInfo, param_size_too_small_returnsError) {
    float dest = 0.0f;
    float src = 1.0f;

    auto retVal = getInfo(&dest, 0, &src, sizeof(src));
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_NE(src, dest);
}

TEST(getInfo, null_src_param_returnsError) {
    float dest = 0.0f;
    float src = 1.0f;

    auto retVal = getInfo(&dest, sizeof(dest), nullptr, sizeof(src));
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_NE(src, dest);
}

TEST(getInfo, zero_src_param_size_returnsError) {
    float dest = 0.0f;
    float src = 1.0f;

    auto retVal = getInfo(&dest, sizeof(dest), &src, 0);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_NE(src, dest);
}

TEST(getInfoHelper, staticSetter) {
    uint32_t *getValue = nullptr;
    uint32_t expectedValue = 1;

    GetInfoHelper::set(getValue, expectedValue);
    EXPECT_EQ(nullptr, getValue);

    getValue = new uint32_t(0);
    GetInfoHelper::set(getValue, expectedValue);
    ASSERT_NE(nullptr, getValue);
    EXPECT_EQ(*getValue, expectedValue);

    delete getValue;
}

TEST(errorCodeHelper, localVariable) {
    cl_int errCode = CL_SUCCESS;
    ErrorCodeHelper err(&errCode, CL_INVALID_VALUE);
    EXPECT_EQ(CL_INVALID_VALUE, errCode);
    EXPECT_EQ(CL_INVALID_VALUE, err.localErrcode);

    err.set(CL_INVALID_CONTEXT);
    EXPECT_EQ(CL_INVALID_CONTEXT, errCode);
    EXPECT_EQ(CL_INVALID_CONTEXT, err.localErrcode);
}
