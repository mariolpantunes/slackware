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

#include "runtime/helpers/hash.h"
#include "runtime/helpers/string_helpers.h"
#include "gtest/gtest.h"

using OCLRT::Hash;

TEST(CreateCombinedStrings, singleString) {
    std::string dstString;
    size_t dstStringSizeInBytes = 0;
    const char srcString[] = "HelloWorld";
    const char *pSrcString = srcString;
    auto srcStrings = &pSrcString;
    size_t lengths = strlen(srcString);

    auto retVal = createCombinedString(
        dstString,
        dstStringSizeInBytes,
        1,
        srcStrings,
        &lengths);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(lengths + 1, dstStringSizeInBytes);

    EXPECT_EQ(0, strcmp(srcString, dstString.c_str()));
}

TEST(CreateCombinedStrings, SingleStringWithNullLengthNoCrash) {
    std::string dstString;
    size_t dstStringSizeInBytes = 0;
    const char srcString[] = "HelloWorld";
    const char *pSrcString = srcString;
    auto srcStrings = &pSrcString;

    auto retVal = createCombinedString(
        dstString,
        dstStringSizeInBytes,
        1,
        srcStrings,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, strcmp(srcString, dstString.c_str()));
}

TEST(CreateCombinedStrings, SingleStringWithZeroLengthNoCrash) {
    std::string dstString;
    size_t dstStringSizeInBytes = 0;
    const char srcString[] = "HelloWorld";
    const char *pSrcString = srcString;
    auto srcStrings = &pSrcString;
    size_t lengths = 0;

    auto retVal = createCombinedString(
        dstString,
        dstStringSizeInBytes,
        1,
        srcStrings,
        &lengths);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, strcmp(srcString, dstString.c_str()));
}

TEST(CreateCombinedStrings, multiString) {
    std::string dstString;
    size_t dstStringSizeInBytes = 0;
    const char *srcString[] = {"HelloWorld", "dlroWolleH"};
    std::string combined(srcString[0]);
    combined += srcString[1];

    auto srcStrings = &srcString[0];
    size_t lengths[2] = {strlen(srcString[0]), strlen(srcString[1])};

    auto retVal = createCombinedString(
        dstString,
        dstStringSizeInBytes,
        2,
        srcStrings,
        lengths);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, strcmp(combined.c_str(), dstString.c_str()));
}

TEST(CreateCombinedStrings, multiStringWithNullLengthNoCrash) {
    std::string dstString;
    size_t dstStringSizeInBytes = 0;
    const char *srcString[] = {"HelloWorld", "dlroWolleH"};
    std::string combined(srcString[0]);
    combined += srcString[1];
    auto srcStrings = &srcString[0];

    auto retVal = createCombinedString(
        dstString,
        dstStringSizeInBytes,
        2,
        srcStrings,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, strcmp(combined.c_str(), dstString.c_str()));
}

TEST(CreateCombinedStrings, multiStringWithZeroLengthNoCrash) {
    std::string dstString;
    size_t dstStringSizeInBytes = 0;
    const char *srcString[] = {"HelloWorld", "dlroWolleH"};
    std::string combined(srcString[0]);
    combined += srcString[1];
    auto srcStrings = &srcString[0];
    size_t lengths[2] = {0, strlen(srcString[1])};

    auto retVal = createCombinedString(
        dstString,
        dstStringSizeInBytes,
        2,
        srcStrings,
        lengths);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, strcmp(combined.c_str(), dstString.c_str()));
}

TEST(CreateCombinedStrings, GivenMultipleStringWhichOneContainsAndErrorAndSizeThatForcesDriverToOmitTheErorrThenProgramSourceDoesntContainErrorString) {
    std::string dstString;
    size_t dstStringSizeInBytes = 0;
    const char *srcString[] = {"HelloWorld", "dlroWolleHBABA"};
    const char *expString[] = {"HelloWorld", "dlroWolleH"};
    size_t lengths[2] = {0, strlen(expString[1])};
    std::string combined(expString[0]);
    combined += expString[1];

    auto retVal = createCombinedString(
        dstString,
        dstStringSizeInBytes,
        2,
        srcString,
        lengths);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, strcmp(combined.c_str(), dstString.c_str()));
}

TEST(CreateCombinedStrings, negativeScenarios) {
    std::string dstString;
    size_t dstStringSizeInBytes = 0;
    const char *srcString[] = {"HelloWorld", "dlroWolleH"};
    std::string combined(srcString[0]);
    combined += srcString[1];
    const char *srcStrings[2] = {srcString[0], srcString[1]};
    size_t lengths[2] = {0, strlen(srcString[1])};

    auto retVal = createCombinedString(
        dstString,
        dstStringSizeInBytes,
        0,
        srcStrings,
        lengths);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = createCombinedString(
        dstString,
        dstStringSizeInBytes,
        1,
        nullptr,
        lengths);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    srcStrings[0] = nullptr;
    retVal = createCombinedString(
        dstString,
        dstStringSizeInBytes,
        2,
        srcStrings,
        lengths);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST(CreateCombinedStrings, GivenMultipleStringThatCountIsHigherThenMaximalStackSizeSizesWhenAskedForCombinedThenProperStringIsRetruned) {
    std::string dstString;
    size_t dstStringSizeInBytes = 0;
    const char *defaultString = "hello";
    const char *srcString[maximalStackSizeSizes + 2];
    std::string combinedString;
    for (int i = 0; i < maximalStackSizeSizes + 2; i++) {
        srcString[i] = defaultString;
        combinedString += defaultString;
    }

    auto retVal = createCombinedString(
        dstString,
        dstStringSizeInBytes,
        maximalStackSizeSizes + 2,
        srcString,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, strcmp(combinedString.c_str(), dstString.c_str()));
}

TEST(CreateHash, HashBuffers) {
    char pBuffer[128];
    memset(pBuffer, 0x23, sizeof(pBuffer));

    // make sure we can get a hash and make sure we can get the same hash
    auto hash1 = Hash::hash(pBuffer, sizeof(pBuffer));
    auto hash2 = Hash::hash(pBuffer, sizeof(pBuffer));
    EXPECT_NE(0u, hash1);
    EXPECT_NE(0u, hash2);
    EXPECT_EQ(hash1, hash2);

    // make sure that we get a different hash for different length/data
    auto hash3 = Hash::hash(pBuffer, sizeof(pBuffer) - 1);
    EXPECT_NE(0u, hash3);
    EXPECT_NE(hash2, hash3);
}

TEST(CreateHash, unalignedLengths) {
    char pBuffer[] = {
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55};

    // Use unaligned lengths. Wiggle the byte after the length
    // Shouldn't affect hash.
    for (auto length = 1u; length < sizeof(pBuffer); length++) {
        auto hash1 = Hash::hash(pBuffer, length);
        pBuffer[length]++;
        EXPECT_EQ(hash1, Hash::hash(pBuffer, length));
    }
}
