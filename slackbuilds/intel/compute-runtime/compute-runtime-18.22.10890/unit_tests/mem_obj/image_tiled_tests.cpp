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

#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_gmm.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "runtime/mem_obj/image.h"
#include "unit_tests/mocks/mock_context.h"
#include "gtest/gtest.h"

using namespace OCLRT;

static const auto dimension = 16;
static auto channelType = CL_UNORM_INT8;
static auto channelOrder = CL_RGBA;

class CreateTiledImageTest : public DeviceFixture,
                             public testing::TestWithParam<uint32_t>,
                             public CommandQueueHwFixture {
    typedef CommandQueueHwFixture CommandQueueFixture;

  public:
    CreateTiledImageTest() {
    }

  protected:
    void SetUp() override {
        DeviceFixture::SetUp();
        CommandQueueFixture::SetUp(pDevice, 0);
        type = GetParam();

        // clang-format off
        imageFormat.image_channel_data_type = channelType;
        imageFormat.image_channel_order     = channelOrder;

        imageDesc.image_type        = type;
        imageDesc.image_width       = dimension;
        imageDesc.image_height      = dimension;
        imageDesc.image_depth       = 1;
        imageDesc.image_array_size  = 1;
        imageDesc.image_row_pitch   = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels    = 0;
        imageDesc.num_samples       = 0;
        imageDesc.mem_object = NULL;
        // clang-format on
    }

    void TearDown() override {
        CommandQueueFixture::TearDown();
        DeviceFixture::TearDown();
    }

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal = CL_SUCCESS;
    cl_mem_object_type type = 0;
};

TEST_P(CreateTiledImageTest, isTiledImageIsSetForTiledImages) {

    MockContext context;
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    auto image = Image::create(
        &context,
        flags,
        surfaceFormat,
        &imageDesc,
        nullptr,
        retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_TRUE(image->isTiledImage);

    delete image;
}

TEST_P(CreateTiledImageTest, isTiledImageIsSetForSharedImages) {
    MockContext context;
    MockGraphicsAllocation *alloc = new MockGraphicsAllocation(0, 0x1000);
    ImageInfo info = {0};
    McsSurfaceInfo msi = {};
    SurfaceFormatInfo surfaceFormat;
    surfaceFormat.GMMSurfaceFormat = GMM_FORMAT_B8G8R8A8_UNORM;
    info.surfaceFormat = &surfaceFormat;

    info.imgDesc = &imageDesc;
    info.plane = GMM_NO_PLANE;

    auto gmm = MockGmm::queryImgParams(info);

    alloc->gmm = gmm.release();

    auto image = Image::createSharedImage(
        &context,
        nullptr,
        msi,
        alloc,
        nullptr,
        CL_MEM_READ_WRITE,
        info,
        0, 0, 0);

    ASSERT_NE(nullptr, image);

    EXPECT_TRUE(image->isTiledImage);

    delete image;
}

typedef CreateTiledImageTest CreateNonTiledImageTest;

TEST_P(CreateNonTiledImageTest, isTiledImageIsNotSetForNonTiledSharedImage) {
    MockContext context;
    MockGraphicsAllocation *alloc = new MockGraphicsAllocation(0, 0x1000);
    ImageInfo info = {0};
    McsSurfaceInfo msi = {};
    SurfaceFormatInfo surfaceFormat;

    imageDesc.image_height = 1;

    surfaceFormat.GMMSurfaceFormat = GMM_FORMAT_B8G8R8A8_UNORM;
    info.surfaceFormat = &surfaceFormat;

    info.imgDesc = &imageDesc;
    info.plane = GMM_NO_PLANE;

    auto gmm = MockGmm::queryImgParams(info);

    alloc->gmm = gmm.release();

    auto image = Image::createSharedImage(
        &context,
        nullptr,
        msi,
        alloc,
        nullptr,
        CL_MEM_READ_WRITE,
        info,
        0, 0, 0);

    ASSERT_NE(nullptr, image);

    EXPECT_FALSE(image->isTiledImage);

    delete image;
}

// Parameterized test that tests image creation with tiled types
static uint32_t TiledImageTypes[] = {
    CL_MEM_OBJECT_IMAGE2D,
    CL_MEM_OBJECT_IMAGE2D_ARRAY,
    CL_MEM_OBJECT_IMAGE3D};

static uint32_t NonTiledImageTypes[] = {
    CL_MEM_OBJECT_IMAGE1D};

INSTANTIATE_TEST_CASE_P(CreateTiledImageTest, CreateTiledImageTest, testing::ValuesIn(TiledImageTypes));
INSTANTIATE_TEST_CASE_P(CreateNonTiledImageTest, CreateNonTiledImageTest, testing::ValuesIn(NonTiledImageTypes));
