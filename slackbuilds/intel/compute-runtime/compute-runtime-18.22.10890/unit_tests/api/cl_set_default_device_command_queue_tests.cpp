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
#include "test.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/device_queue/device_queue.h"

using namespace OCLRT;

namespace ULT {

struct clSetDefaultDeviceCommandQueueApiTest : public api_tests {
    clSetDefaultDeviceCommandQueueApiTest() {
    }

    void SetUp() override {
        api_tests::SetUp();
        cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES,
                                            CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE,
                                            0,
                                            0};
        deviceQueue = clCreateCommandQueueWithProperties(pContext, devices[0], properties, &retVal);
        ASSERT_NE(nullptr, deviceQueue);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    void TearDown() override {
        retVal = clReleaseCommandQueue(deviceQueue);
        EXPECT_EQ(CL_SUCCESS, retVal);
        api_tests::TearDown();
    }

    cl_command_queue deviceQueue = nullptr;
};

TEST_F(clSetDefaultDeviceCommandQueueApiTest, SetDefaultDeviceQueue_returnsSuccess) {
    retVal = clSetDefaultDeviceCommandQueue(pContext, devices[0], deviceQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(static_cast<_device_queue *>(deviceQueue), static_cast<_device_queue *>(pContext->getDefaultDeviceQueue()));
}

TEST_F(clSetDefaultDeviceCommandQueueApiTest, ReplaceDefaultDeviceQueue_returnsSuccess) {
    cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES,
                                        CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE,
                                        0,
                                        0};
    auto pDevice = castToObject<Device>(devices[0]);

    if (pDevice->getDeviceInfo().maxOnDeviceQueues > 1) {
        auto newDeviceQueue = clCreateCommandQueueWithProperties(pContext, devices[0], properties, &retVal);
        ASSERT_NE(nullptr, newDeviceQueue);
        ASSERT_EQ(CL_SUCCESS, retVal);

        retVal = clSetDefaultDeviceCommandQueue(pContext, devices[0], newDeviceQueue);
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_EQ(static_cast<_device_queue *>(newDeviceQueue), static_cast<_device_queue *>(pContext->getDefaultDeviceQueue()));

        clReleaseCommandQueue(newDeviceQueue);
    }
}

TEST_F(clSetDefaultDeviceCommandQueueApiTest, NullContext_returnsError) {
    retVal = clSetDefaultDeviceCommandQueue(nullptr, devices[0], deviceQueue);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(clSetDefaultDeviceCommandQueueApiTest, NullDevice_returnsError) {
    retVal = clSetDefaultDeviceCommandQueue(pContext, nullptr, deviceQueue);
    ASSERT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(clSetDefaultDeviceCommandQueueApiTest, NullDeviceQueue_returnsError) {
    retVal = clSetDefaultDeviceCommandQueue(pContext, devices[0], nullptr);
    ASSERT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(clSetDefaultDeviceCommandQueueApiTest, HostDeviceQueue_returnsError) {
    cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES, 0, 0, 0};
    cl_command_queue hostQueue = clCreateCommandQueueWithProperties(pContext, devices[0], properties, &retVal);
    ASSERT_NE(nullptr, hostQueue);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clSetDefaultDeviceCommandQueue(pContext, devices[0], hostQueue);
    ASSERT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);

    retVal = clReleaseCommandQueue(hostQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clSetDefaultDeviceCommandQueueApiTest, DeviceQueueCtx2_returnsError) {
    auto context2 = clCreateContext(nullptr, num_devices, devices, nullptr, nullptr, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES,
                                        CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE,
                                        0,
                                        0};
    cl_command_queue deviceQueueCtx2 = clCreateCommandQueueWithProperties(context2, devices[0], properties, &retVal);
    ASSERT_NE(nullptr, deviceQueueCtx2);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clSetDefaultDeviceCommandQueue(pContext, devices[0], deviceQueueCtx2);
    ASSERT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);

    retVal = clReleaseCommandQueue(deviceQueueCtx2);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context2);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT
