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

#include "runtime/helpers/surface_formats.h"
#include "runtime/helpers/options.h"
#include "runtime/mem_obj/image.h"
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_device.h"
#include "gtest/gtest.h"

#include <memory>

using namespace OCLRT;

struct GetSupportedImageFormatsTest : public PlatformFixture,
                                      public ContextFixture,
                                      public ::testing::TestWithParam<std::tuple<uint64_t, uint32_t>> {

    using ContextFixture::SetUp;
    using PlatformFixture::SetUp;

    GetSupportedImageFormatsTest() {
    }

    void SetUp() override {
        PlatformFixture::SetUp(numPlatformDevices, platformDevices);
        ContextFixture::SetUp(num_devices, devices);
    }

    void TearDown() override {
        ContextFixture::TearDown();
        PlatformFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
};

TEST_P(GetSupportedImageFormatsTest, checkNumImageFormats) {
    cl_uint numImageFormats = 0;
    uint64_t imageFormatsFlags;
    uint32_t imageFormats;
    std::tie(imageFormatsFlags, imageFormats) = GetParam();
    retVal = pContext->getSupportedImageFormats(
        castToObject<Device>(devices[0]),
        imageFormatsFlags,
        imageFormats,
        0,
        nullptr,
        &numImageFormats);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(numImageFormats, 0u);
}

TEST_P(GetSupportedImageFormatsTest, retrieveImageFormats) {
    cl_uint numImageFormats = 0;
    uint64_t imageFormatsFlags;
    uint32_t imageFormats;
    std::tie(imageFormatsFlags, imageFormats) = GetParam();
    retVal = pContext->getSupportedImageFormats(
        castToObject<Device>(devices[0]),
        imageFormatsFlags,
        imageFormats,
        0,
        nullptr,
        &numImageFormats);
    EXPECT_GT(numImageFormats, 0u);

    auto imageFormatList = new cl_image_format[numImageFormats];
    memset(imageFormatList, 0, numImageFormats * sizeof(cl_image_format));

    retVal = pContext->getSupportedImageFormats(
        castToObject<Device>(devices[0]),
        imageFormatsFlags,
        imageFormats,
        numImageFormats,
        imageFormatList,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (cl_uint entry = 0; entry < numImageFormats; ++entry) {
        EXPECT_NE(0u, imageFormatList[entry].image_channel_order);
        EXPECT_NE(0u, imageFormatList[entry].image_channel_data_type);
    }

    delete[] imageFormatList;
}

TEST_P(GetSupportedImageFormatsTest, retrieveImageFormatsSRGB) {
    cl_uint numImageFormats = 0;
    uint64_t imageFormatsFlags;
    uint32_t imageFormats;
    bool sRGBAFormatFound = false;
    bool sBGRAFormatFound = false;
    bool isReadOnly = false;
    std::tie(imageFormatsFlags, imageFormats) = GetParam();
    retVal = pContext->getSupportedImageFormats(
        castToObject<Device>(devices[0]),
        imageFormatsFlags,
        imageFormats,
        0,
        nullptr,
        &numImageFormats);
    EXPECT_GT(numImageFormats, 0u);

    auto imageFormatList = new cl_image_format[numImageFormats];
    memset(imageFormatList, 0, numImageFormats * sizeof(cl_image_format));

    retVal = pContext->getSupportedImageFormats(
        castToObject<Device>(devices[0]),
        imageFormatsFlags,
        imageFormats,
        numImageFormats,
        imageFormatList,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    isReadOnly |= (imageFormatsFlags == CL_MEM_READ_ONLY);

    for (cl_uint entry = 0; entry < numImageFormats; ++entry) {
        EXPECT_NE(0u, imageFormatList[entry].image_channel_order);
        EXPECT_NE(0u, imageFormatList[entry].image_channel_data_type);

        if (imageFormatList[entry].image_channel_order == CL_sRGBA) {
            sRGBAFormatFound = true;
        }

        if (imageFormatList[entry].image_channel_order == CL_sBGRA) {
            sBGRAFormatFound = true;
        }
    }

    if (isReadOnly) {
        EXPECT_TRUE(sRGBAFormatFound & sBGRAFormatFound);
    } else {
        EXPECT_FALSE(sRGBAFormatFound | sBGRAFormatFound);
    }

    delete[] imageFormatList;
}

TEST(ImageFormats, isDepthFormat) {
    for (size_t i = 0; i < numReadOnlySurfaceFormats; i++) {
        EXPECT_FALSE(Image::isDepthFormat(readOnlySurfaceFormats[i].OCLImageFormat));
    }

    for (size_t i = 0; i < numReadOnlyDepthSurfaceFormats; i++) {
        EXPECT_TRUE(Image::isDepthFormat(readOnlyDepthSurfaceFormats[i].OCLImageFormat));
    }
}

struct PackedYuvExtensionSupportedImageFormatsTest : public ::testing::TestWithParam<std::tuple<uint64_t, uint32_t>> {

    void SetUp() override {
        device = std::unique_ptr<MockDevice>(new MockDevice(*platformDevices[0]));
        context = std::unique_ptr<MockContext>(new MockContext(device.get(), true));
    }

    void TearDown() override {
    }
    std::unique_ptr<MockDevice> device;
    std::unique_ptr<MockContext> context;
    cl_int retVal;
};

TEST_P(PackedYuvExtensionSupportedImageFormatsTest, retrieveImageFormatsPackedYUV) {
    cl_uint numImageFormats = 0;
    uint64_t imageFormatsFlags;
    uint32_t imageFormats;
    bool YUYVFormatFound = false;
    bool UYVYFormatFound = false;
    bool YVYUFormatFound = false;
    bool VYUYFormatFound = false;
    bool isReadOnly = false;
    std::tie(imageFormatsFlags, imageFormats) = GetParam();

    device->getDeviceInfoToModify()->packedYuvExtension = true;

    retVal = context->getSupportedImageFormats(
        device.get(),
        imageFormatsFlags,
        imageFormats,
        0,
        nullptr,
        &numImageFormats);
    EXPECT_GT(numImageFormats, 0u);

    auto imageFormatList = new cl_image_format[numImageFormats];
    memset(imageFormatList, 0, numImageFormats * sizeof(cl_image_format));

    retVal = context->getSupportedImageFormats(
        device.get(),
        imageFormatsFlags,
        imageFormats,
        numImageFormats,
        imageFormatList,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    isReadOnly |= (imageFormatsFlags == CL_MEM_READ_ONLY);

    for (cl_uint entry = 0; entry < numImageFormats; ++entry) {
        EXPECT_NE(0u, imageFormatList[entry].image_channel_order);
        EXPECT_NE(0u, imageFormatList[entry].image_channel_data_type);

        if (imageFormatList[entry].image_channel_order == CL_YUYV_INTEL) {
            YUYVFormatFound = true;
        }

        if (imageFormatList[entry].image_channel_order == CL_UYVY_INTEL) {
            UYVYFormatFound = true;
        }

        if (imageFormatList[entry].image_channel_order == CL_YVYU_INTEL) {
            YVYUFormatFound = true;
        }

        if (imageFormatList[entry].image_channel_order == CL_VYUY_INTEL) {
            VYUYFormatFound = true;
        }
    }

    if (isReadOnly && imageFormats == CL_MEM_OBJECT_IMAGE2D) {
        EXPECT_TRUE(YUYVFormatFound);
        EXPECT_TRUE(UYVYFormatFound);
        EXPECT_TRUE(YVYUFormatFound);
        EXPECT_TRUE(VYUYFormatFound);
    } else {
        EXPECT_FALSE(YUYVFormatFound);
        EXPECT_FALSE(UYVYFormatFound);
        EXPECT_FALSE(YVYUFormatFound);
        EXPECT_FALSE(VYUYFormatFound);
    }

    delete[] imageFormatList;
}

struct NV12ExtensionSupportedImageFormatsTest : public ::testing::TestWithParam<std::tuple<uint64_t, uint32_t>> {

    void SetUp() override {
        device = std::unique_ptr<MockDevice>(new MockDevice(*platformDevices[0]));
        context = std::unique_ptr<MockContext>(new MockContext(device.get(), true));
    }

    void TearDown() override {
    }
    std::unique_ptr<MockDevice> device;
    std::unique_ptr<MockContext> context;
    cl_int retVal;
};

typedef NV12ExtensionSupportedImageFormatsTest NV12ExtensionUnsupportedImageFormatsTest;

TEST_P(NV12ExtensionSupportedImageFormatsTest, givenNV12ExtensionWhenQueriedForImageFormatsThenNV12FormatIsReturnedOnlyFor2DImages) {
    cl_uint numImageFormats = 0;
    uint64_t imageFormatsFlags;
    uint32_t imageFormats;
    bool Nv12FormatFound = false;
    std::tie(imageFormatsFlags, imageFormats) = GetParam();

    device->getDeviceInfoToModify()->nv12Extension = true;

    retVal = context->getSupportedImageFormats(
        device.get(),
        imageFormatsFlags,
        imageFormats,
        0,
        nullptr,
        &numImageFormats);

    size_t expectedNumReadOnlyFormats = numReadOnlySurfaceFormats;
    if (Image::isImage2dOr2dArray(imageFormats) && imageFormatsFlags == CL_MEM_READ_ONLY) {
        expectedNumReadOnlyFormats += numReadOnlyDepthSurfaceFormats;
    }

    if (Image::isImage2d(imageFormats)) {
        if (imageFormatsFlags == CL_MEM_READ_ONLY) {
            EXPECT_EQ(expectedNumReadOnlyFormats + numPlanarYuvSurfaceFormats, static_cast<size_t>(numImageFormats));
        }
        if (imageFormatsFlags == CL_MEM_NO_ACCESS_INTEL) {
            EXPECT_EQ(expectedNumReadOnlyFormats + numPlanarYuvSurfaceFormats, static_cast<size_t>(numImageFormats));
        }
    } else {
        if (imageFormatsFlags == CL_MEM_READ_ONLY) {
            EXPECT_EQ(expectedNumReadOnlyFormats, static_cast<size_t>(numImageFormats));
        }
        if (imageFormatsFlags == CL_MEM_NO_ACCESS_INTEL) {
            EXPECT_EQ(expectedNumReadOnlyFormats, static_cast<size_t>(numImageFormats));
        }
    }

    auto imageFormatList = new cl_image_format[numImageFormats];
    memset(imageFormatList, 0, numImageFormats * sizeof(cl_image_format));

    retVal = context->getSupportedImageFormats(
        device.get(),
        imageFormatsFlags,
        imageFormats,
        numImageFormats,
        imageFormatList,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (cl_uint entry = 0; entry < numImageFormats; ++entry) {
        EXPECT_NE(0u, imageFormatList[entry].image_channel_order);
        EXPECT_NE(0u, imageFormatList[entry].image_channel_data_type);

        if (imageFormatList[entry].image_channel_order == CL_NV12_INTEL) {
            Nv12FormatFound = true;
        }
    }

    if (imageFormats == CL_MEM_OBJECT_IMAGE2D) {
        EXPECT_TRUE(Nv12FormatFound);
    } else {
        EXPECT_FALSE(Nv12FormatFound);
    }

    delete[] imageFormatList;
}

TEST_P(NV12ExtensionUnsupportedImageFormatsTest, givenNV12ExtensionWhenQueriedForWriteOnlyOrReadWriteImageFormatsThenNV12FormatIsNotReturned) {
    cl_uint numImageFormats = 0;
    uint64_t imageFormatsFlags;
    uint32_t imageFormats;
    bool Nv12FormatFound = false;
    std::tie(imageFormatsFlags, imageFormats) = GetParam();

    device->getDeviceInfoToModify()->nv12Extension = true;

    retVal = context->getSupportedImageFormats(
        device.get(),
        imageFormatsFlags,
        imageFormats,
        0,
        nullptr,
        &numImageFormats);

    if (imageFormatsFlags == CL_MEM_WRITE_ONLY) {
        if (!Image::isImage2dOr2dArray(imageFormats)) {
            EXPECT_EQ(numWriteOnlySurfaceFormats, static_cast<size_t>(numImageFormats));
        } else {
            EXPECT_EQ(numWriteOnlySurfaceFormats + numReadWriteDepthSurfaceFormats, static_cast<size_t>(numImageFormats));
        }
    }

    if (imageFormatsFlags == CL_MEM_READ_WRITE) {
        if (!Image::isImage2dOr2dArray(imageFormats)) {
            EXPECT_EQ(numReadWriteSurfaceFormats, static_cast<size_t>(numImageFormats));
        } else {
            EXPECT_EQ(numReadWriteSurfaceFormats + numReadWriteDepthSurfaceFormats, static_cast<size_t>(numImageFormats));
        }
    }

    auto imageFormatList = new cl_image_format[numImageFormats];
    memset(imageFormatList, 0, numImageFormats * sizeof(cl_image_format));

    retVal = context->getSupportedImageFormats(
        device.get(),
        imageFormatsFlags,
        imageFormats,
        numImageFormats,
        imageFormatList,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (cl_uint entry = 0; entry < numImageFormats; ++entry) {
        EXPECT_NE(0u, imageFormatList[entry].image_channel_order);
        EXPECT_NE(0u, imageFormatList[entry].image_channel_data_type);

        if (imageFormatList[entry].image_channel_order == CL_NV12_INTEL) {
            Nv12FormatFound = true;
        }
    }

    EXPECT_FALSE(Nv12FormatFound);

    delete[] imageFormatList;
}

TEST_P(NV12ExtensionSupportedImageFormatsTest, retrieveLessImageFormatsThanAvailable) {
    cl_uint numImageFormats = 0;
    uint64_t imageFormatsFlags;
    uint32_t imageFormats;
    std::tie(imageFormatsFlags, imageFormats) = GetParam();

    device->getDeviceInfoToModify()->nv12Extension = true;

    retVal = context->getSupportedImageFormats(
        device.get(),
        imageFormatsFlags,
        imageFormats,
        0,
        nullptr,
        &numImageFormats);
    EXPECT_GT(numImageFormats, 0u);

    if (numImageFormats > 1)
        numImageFormats--;

    auto imageFormatList = new cl_image_format[numImageFormats];
    memset(imageFormatList, 0, numImageFormats * sizeof(cl_image_format));

    retVal = context->getSupportedImageFormats(
        device.get(),
        imageFormatsFlags,
        imageFormats,
        numImageFormats,
        imageFormatList,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (cl_uint entry = 0; entry < numImageFormats; ++entry) {
        EXPECT_NE(0u, imageFormatList[entry].image_channel_order);
        EXPECT_NE(0u, imageFormatList[entry].image_channel_data_type);
    }

    delete[] imageFormatList;
}

cl_mem_flags GetSupportedImageFormatsFlags[] = {
    CL_MEM_READ_WRITE,
    CL_MEM_WRITE_ONLY,
    CL_MEM_READ_ONLY};

cl_mem_object_type GetSupportedImageFormats[] = {
    CL_MEM_OBJECT_IMAGE1D,
    CL_MEM_OBJECT_IMAGE1D_BUFFER,
    CL_MEM_OBJECT_IMAGE1D_ARRAY,
    CL_MEM_OBJECT_IMAGE2D,
    CL_MEM_OBJECT_IMAGE2D_ARRAY,
    CL_MEM_OBJECT_IMAGE3D};

INSTANTIATE_TEST_CASE_P(
    Context,
    GetSupportedImageFormatsTest,
    ::testing::Combine(
        ::testing::ValuesIn(GetSupportedImageFormatsFlags),
        ::testing::ValuesIn(GetSupportedImageFormats)));

INSTANTIATE_TEST_CASE_P(
    Context,
    PackedYuvExtensionSupportedImageFormatsTest,
    ::testing::Combine(
        ::testing::ValuesIn(GetSupportedImageFormatsFlags),
        ::testing::ValuesIn(GetSupportedImageFormats)));

cl_mem_flags NV12ExtensionSupportedImageFormatsFlags[] = {
    CL_MEM_NO_ACCESS_INTEL,
    CL_MEM_READ_ONLY};

cl_mem_flags NV12ExtensionUnsupportedImageFormatsFlags[] = {
    CL_MEM_READ_WRITE,
    CL_MEM_WRITE_ONLY};

cl_mem_object_type NV12ExtensionSupportedImageFormats[] = {
    CL_MEM_OBJECT_IMAGE1D,
    CL_MEM_OBJECT_IMAGE2D};

INSTANTIATE_TEST_CASE_P(
    Context,
    NV12ExtensionSupportedImageFormatsTest,
    ::testing::Combine(
        ::testing::ValuesIn(NV12ExtensionSupportedImageFormatsFlags),
        ::testing::ValuesIn(NV12ExtensionSupportedImageFormats)));

INSTANTIATE_TEST_CASE_P(
    Context,
    NV12ExtensionUnsupportedImageFormatsTest,
    ::testing::Combine(
        ::testing::ValuesIn(NV12ExtensionUnsupportedImageFormatsFlags),
        ::testing::ValuesIn(NV12ExtensionSupportedImageFormats)));
