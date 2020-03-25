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

#include "runtime/device/device_info_map.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "gtest/gtest.h"
#include <memory>

using namespace OCLRT;

TEST(GetDeviceInfo, InvalidFlags_returnsError) {
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create());

    auto retVal = device->getDeviceInfo(
        0,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST(GetDeviceInfo, devicePlanarYuvMaxWidthHeightReturnsErrorWhenPlanarYuvExtensionDisabled) {
    auto device = std::unique_ptr<MockDevice>(DeviceHelper<>::create());

    device->getDeviceInfoToModify()->nv12Extension = false;
    uint32_t value;

    auto retVal = device->getDeviceInfo(
        CL_DEVICE_PLANAR_YUV_MAX_WIDTH_INTEL,
        4,
        &value,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = device->getDeviceInfo(
        CL_DEVICE_PLANAR_YUV_MAX_HEIGHT_INTEL,
        4,
        &value,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST(GetDeviceInfo, devicePlanarYuvMaxWidthHeightReturnsCorrectValuesWhenPlanarYuvExtensionEnabled) {
    auto device = std::unique_ptr<MockDevice>(DeviceHelper<>::create());

    device->getDeviceInfoToModify()->nv12Extension = true;
    size_t value = 0;

    auto retVal = device->getDeviceInfo(
        CL_DEVICE_PLANAR_YUV_MAX_WIDTH_INTEL,
        sizeof(size_t),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(16384u, value);

    retVal = device->getDeviceInfo(
        CL_DEVICE_PLANAR_YUV_MAX_HEIGHT_INTEL,
        sizeof(size_t),
        &value,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(16380u, value);
}

TEST(GetDeviceInfo, numSimultaneousInterops) {
    auto device = std::unique_ptr<MockDevice>(DeviceHelper<>::create());
    device->simultaneousInterops = {0};

    cl_uint value = 0;
    size_t size = 0;

    auto retVal = device->getDeviceInfo(CL_DEVICE_NUM_SIMULTANEOUS_INTEROPS_INTEL, sizeof(cl_uint), &value, &size);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(0u, size);
    EXPECT_EQ(0u, value);

    device->simultaneousInterops = {1, 2, 3, 0};
    retVal = device->getDeviceInfo(CL_DEVICE_NUM_SIMULTANEOUS_INTEROPS_INTEL, sizeof(cl_uint), &value, &size);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_uint), size);
    EXPECT_EQ(1u, value);
}

TEST(GetDeviceInfo, simultaneousInterops) {
    auto device = std::unique_ptr<MockDevice>(DeviceHelper<>::create());
    device->simultaneousInterops = {0};

    cl_uint value[4] = {};
    size_t size = 0;

    auto retVal = device->getDeviceInfo(CL_DEVICE_SIMULTANEOUS_INTEROPS_INTEL, sizeof(cl_uint), &value, &size);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    device->simultaneousInterops = {1, 2, 3, 0};
    retVal = device->getDeviceInfo(CL_DEVICE_SIMULTANEOUS_INTEROPS_INTEL, sizeof(cl_uint) * 4u, &value, &size);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_uint) * 4u, size);
    EXPECT_TRUE(memcmp(value, &device->simultaneousInterops[0], 4u * sizeof(cl_uint)) == 0);
}

TEST(GetDeviceInfo, prefferedInteropUserSync) {
    auto device = std::unique_ptr<MockDevice>(DeviceHelper<>::create());

    cl_bool value = 0;
    size_t size = 0;

    auto retVal = device->getDeviceInfo(CL_DEVICE_PREFERRED_INTEROP_USER_SYNC, sizeof(cl_bool), &value, &size);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_bool), size);
    EXPECT_TRUE(value == 1u);
}

struct GetDeviceInfo : public ::testing::TestWithParam<uint32_t /*cl_device_info*/> {
    void SetUp() override {
        param = GetParam();
    }

    cl_device_info param;
};

TEST_P(GetDeviceInfo, valid_returnsSuccess) {
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create());

    size_t sizeReturned = 0;
    auto retVal = device->getDeviceInfo(
        param,
        0,
        nullptr,
        &sizeReturned);
    if (CL_SUCCESS != retVal) {
        ASSERT_EQ(CL_SUCCESS, retVal) << " param = " << param;
    }
    ASSERT_NE(0u, sizeReturned);

    auto *object = new char[sizeReturned];
    retVal = device->getDeviceInfo(
        param,
        sizeReturned,
        object,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete[] object;
}

// Define new command types to run the parameterized tests
cl_device_info deviceInfoParams[] = {
    CL_DEVICE_ADDRESS_BITS,
    CL_DEVICE_AVAILABLE,
    CL_DEVICE_AVC_ME_SUPPORTS_PREEMPTION_INTEL,
    CL_DEVICE_AVC_ME_SUPPORTS_TEXTURE_SAMPLER_USE_INTEL,
    CL_DEVICE_AVC_ME_VERSION_INTEL,
    CL_DEVICE_BUILT_IN_KERNELS,
    CL_DEVICE_COMPILER_AVAILABLE,
    CL_DEVICE_IL_VERSION,
    //    NOT_SUPPORTED
    //    CL_DEVICE_TERMINATE_CAPABILITY_KHR,
    CL_DEVICE_DOUBLE_FP_CONFIG,
    CL_DEVICE_ENDIAN_LITTLE,
    CL_DEVICE_ERROR_CORRECTION_SUPPORT,
    CL_DEVICE_EXECUTION_CAPABILITIES,
    CL_DEVICE_EXTENSIONS,
    CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE,
    CL_DEVICE_GLOBAL_MEM_CACHE_SIZE,
    CL_DEVICE_GLOBAL_MEM_CACHE_TYPE,
    CL_DEVICE_GLOBAL_MEM_SIZE,
    CL_DEVICE_GLOBAL_VARIABLE_PREFERRED_TOTAL_SIZE,
    CL_DEVICE_HALF_FP_CONFIG,
    CL_DEVICE_HOST_UNIFIED_MEMORY,
    CL_DEVICE_IMAGE2D_MAX_HEIGHT,
    CL_DEVICE_IMAGE2D_MAX_WIDTH,
    CL_DEVICE_IMAGE3D_MAX_DEPTH,
    CL_DEVICE_IMAGE3D_MAX_HEIGHT,
    CL_DEVICE_IMAGE3D_MAX_WIDTH,
    CL_DEVICE_IMAGE_BASE_ADDRESS_ALIGNMENT,
    CL_DEVICE_IMAGE_MAX_ARRAY_SIZE,
    CL_DEVICE_IMAGE_MAX_BUFFER_SIZE,
    CL_DEVICE_IMAGE_PITCH_ALIGNMENT,
    CL_DEVICE_IMAGE_SUPPORT,
    CL_DEVICE_LINKER_AVAILABLE,
    CL_DEVICE_LOCAL_MEM_SIZE,
    CL_DEVICE_LOCAL_MEM_TYPE,
    CL_DEVICE_MAX_CLOCK_FREQUENCY,
    CL_DEVICE_MAX_COMPUTE_UNITS,
    CL_DEVICE_MAX_CONSTANT_ARGS,
    CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE,
    CL_DEVICE_MAX_GLOBAL_VARIABLE_SIZE,
    CL_DEVICE_MAX_MEM_ALLOC_SIZE,
    CL_DEVICE_MAX_NUM_SUB_GROUPS,
    CL_DEVICE_MAX_ON_DEVICE_EVENTS,
    CL_DEVICE_MAX_ON_DEVICE_QUEUES,
    CL_DEVICE_MAX_PARAMETER_SIZE,
    CL_DEVICE_MAX_PIPE_ARGS,
    CL_DEVICE_MAX_READ_IMAGE_ARGS,
    CL_DEVICE_MAX_READ_WRITE_IMAGE_ARGS,
    CL_DEVICE_MAX_SAMPLERS,
    CL_DEVICE_MAX_WORK_GROUP_SIZE,
    CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS,
    CL_DEVICE_MAX_WORK_ITEM_SIZES,
    CL_DEVICE_MAX_WRITE_IMAGE_ARGS,
    CL_DEVICE_MEM_BASE_ADDR_ALIGN,
    CL_DEVICE_ME_VERSION_INTEL,
    CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE,
    CL_DEVICE_NAME,
    CL_DEVICE_NATIVE_VECTOR_WIDTH_CHAR,
    CL_DEVICE_NATIVE_VECTOR_WIDTH_DOUBLE,
    CL_DEVICE_NATIVE_VECTOR_WIDTH_FLOAT,
    CL_DEVICE_NATIVE_VECTOR_WIDTH_HALF,
    CL_DEVICE_NATIVE_VECTOR_WIDTH_INT,
    CL_DEVICE_NATIVE_VECTOR_WIDTH_LONG,
    CL_DEVICE_NATIVE_VECTOR_WIDTH_SHORT,
    CL_DEVICE_OPENCL_C_VERSION,
    CL_DEVICE_PARENT_DEVICE,
    CL_DEVICE_PARTITION_AFFINITY_DOMAIN,
    CL_DEVICE_PARTITION_MAX_SUB_DEVICES,
    CL_DEVICE_PARTITION_PROPERTIES,
    CL_DEVICE_PARTITION_TYPE,
    CL_DEVICE_PIPE_MAX_ACTIVE_RESERVATIONS,
    CL_DEVICE_PIPE_MAX_PACKET_SIZE,
    CL_DEVICE_PLATFORM,
    CL_DEVICE_PREFERRED_GLOBAL_ATOMIC_ALIGNMENT,
    CL_DEVICE_PREFERRED_INTEROP_USER_SYNC,
    CL_DEVICE_PREFERRED_LOCAL_ATOMIC_ALIGNMENT,
    CL_DEVICE_PREFERRED_PLATFORM_ATOMIC_ALIGNMENT,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_HALF,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT,
    CL_DEVICE_PRINTF_BUFFER_SIZE,
    CL_DEVICE_PROFILE,
    CL_DEVICE_PROFILING_TIMER_RESOLUTION,
    CL_DEVICE_QUEUE_ON_DEVICE_MAX_SIZE,
    CL_DEVICE_QUEUE_ON_DEVICE_PREFERRED_SIZE,
    CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES,
    CL_DEVICE_QUEUE_ON_HOST_PROPERTIES,
    CL_DEVICE_REFERENCE_COUNT,
    CL_DEVICE_SINGLE_FP_CONFIG,
    CL_DEVICE_SPIR_VERSIONS,
    CL_DEVICE_SUB_GROUP_INDEPENDENT_FORWARD_PROGRESS,
    CL_DEVICE_SUB_GROUP_SIZES_INTEL,
    CL_DEVICE_SVM_CAPABILITIES,
    CL_DEVICE_TYPE,
    CL_DEVICE_VENDOR,
    CL_DEVICE_VENDOR_ID,
    CL_DEVICE_VERSION,
    CL_DRIVER_VERSION,
};

INSTANTIATE_TEST_CASE_P(
    Device_,
    GetDeviceInfo,
    testing::ValuesIn(deviceInfoParams));
