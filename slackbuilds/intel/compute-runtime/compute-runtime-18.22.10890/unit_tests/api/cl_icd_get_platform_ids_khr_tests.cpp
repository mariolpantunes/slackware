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

#if defined(_WIN32)
#include "runtime/os_interface/windows/windows_wrapper.h"
#endif

#include "unit_tests/api/cl_api_tests.h"
#include "runtime/platform/platform.h"
#include "runtime/device/device.h"

#include <algorithm>

using namespace OCLRT;

typedef api_tests clIcdGetPlatformIDsKHRTests;

namespace ULT {

TEST_F(clIcdGetPlatformIDsKHRTests, checkDispatchLocation) {
    cl_platform_id platform = pPlatform;
    EXPECT_EQ((void *)platform, (void *)(&platform->dispatch));
}

TEST_F(clIcdGetPlatformIDsKHRTests, getCount) {
    cl_int retVal = CL_SUCCESS;
    cl_uint numPlatforms = 0;

    retVal = clIcdGetPlatformIDsKHR(0, nullptr, &numPlatforms);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(numPlatforms, (cl_uint)0);
}

TEST_F(clIcdGetPlatformIDsKHRTests, checkExtensionFunctionAvailability) {
    void *funPtr = clGetExtensionFunctionAddress("clIcdGetPlatformIDsKHR");
    decltype(&clIcdGetPlatformIDsKHR) expected = clIcdGetPlatformIDsKHR;
    EXPECT_NE(nullptr, funPtr);
    EXPECT_EQ(expected, reinterpret_cast<decltype(&clIcdGetPlatformIDsKHR)>(funPtr));
}

TEST_F(clIcdGetPlatformIDsKHRTests, checkDeviceId) {
    cl_uint numPlatforms = 0;
    cl_uint numPlatformsIcd = 0;

    retVal = clGetPlatformIDs(0, nullptr, &numPlatforms);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = clIcdGetPlatformIDsKHR(0, nullptr, &numPlatformsIcd);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(numPlatforms, numPlatformsIcd);

    std::unique_ptr<cl_platform_id, decltype(free) *> platforms(reinterpret_cast<cl_platform_id *>(malloc(sizeof(cl_platform_id) * numPlatforms)), free);
    ASSERT_NE(nullptr, platforms);

    std::unique_ptr<cl_platform_id, decltype(free) *> platformsIcd(reinterpret_cast<cl_platform_id *>(malloc(sizeof(cl_platform_id) * numPlatforms)), free);
    ASSERT_NE(nullptr, platforms);

    retVal = clGetPlatformIDs(numPlatforms, platforms.get(), nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = clIcdGetPlatformIDsKHR(numPlatformsIcd, platformsIcd.get(), nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    for (cl_uint i = 0; i < std::min(numPlatforms, numPlatformsIcd); i++) {
        EXPECT_EQ(platforms.get()[i], platformsIcd.get()[i]);
    }
}

TEST_F(clIcdGetPlatformIDsKHRTests, checkExtensionString) {
    const DeviceInfo &caps = pPlatform->getDevice(0)->getDeviceInfo();
    EXPECT_NE(std::string::npos, std::string(caps.deviceExtensions).find("cl_khr_icd"));
}
} // namespace ULT
