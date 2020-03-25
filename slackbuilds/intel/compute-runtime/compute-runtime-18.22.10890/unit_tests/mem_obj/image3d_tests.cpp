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

#include "hw_cmds.h"
#include "runtime/mem_obj/image.h"
#include "runtime/helpers/aligned_memory.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_gmm.h"
#include "test.h"

using namespace OCLRT;

static const unsigned int testImageDimensions = 31;

class CreateImage3DTest : public DeviceFixture,
                          public testing::TestWithParam<uint32_t /*cl_mem_object_type*/> {
  public:
    CreateImage3DTest() {}

  protected:
    void SetUp() override {
        DeviceFixture::SetUp();
        context = new MockContext(pDevice);

        // clang-format off
        imageFormat.image_channel_data_type = CL_UNORM_INT8;
        imageFormat.image_channel_order = CL_RGBA;

        imageDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
        imageDesc.image_width = testImageDimensions;
        imageDesc.image_height = testImageDimensions;
        imageDesc.image_depth = testImageDimensions;
        imageDesc.image_array_size = 1;
        imageDesc.image_row_pitch = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels = 0;
        imageDesc.num_samples = 0;
        imageDesc.mem_object = NULL;
        // clang-format on
    }

    void TearDown() override {
        delete context;
        DeviceFixture::TearDown();
    }

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal = CL_SUCCESS;
    MockContext *context;
    cl_mem_object_type types = 0;
};

HWTEST_F(CreateImage3DTest, validTypes) {
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    auto image = Image::create(context, flags, surfaceFormat, &imageDesc, nullptr, retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, image);

    auto imgDesc = image->getImageDesc();

    EXPECT_NE(0u, imgDesc.image_width);
    EXPECT_NE(0u, imgDesc.image_height);
    EXPECT_NE(0u, imgDesc.image_depth);
    EXPECT_NE(0u, imgDesc.image_slice_pitch);
    EXPECT_EQ(0u, imgDesc.image_array_size);
    EXPECT_NE(0u, imgDesc.image_row_pitch);

    EXPECT_EQ(image->getCubeFaceIndex(), static_cast<uint32_t>(__GMM_NO_CUBE_MAP));

    ASSERT_EQ(true, image->isMemObjZeroCopy());
    auto address = image->getCpuAddress();
    EXPECT_NE(nullptr, address);

    typedef typename FamilyType::RENDER_SURFACE_STATE SURFACE_STATE;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image);
    EXPECT_EQ(SURFACE_STATE::SURFACE_TYPE_SURFTYPE_3D, imageHw->surfaceType);

    delete image;
}

HWTEST_F(CreateImage3DTest, calculate3dImageQpitchTiledAndLinear) {
    bool defaultTiling = DebugManager.flags.ForceLinearImages.get();
    imageDesc.image_height = 1;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(0, &imageFormat);
    auto imgInfo = MockGmm::initImgInfo(imageDesc, 0, surfaceFormat);
    MockGmm::queryImgParams(imgInfo);

    auto image = Image::create(
        context,
        0,
        surfaceFormat,
        &imageDesc,
        nullptr,
        retVal);
    ASSERT_NE(nullptr, image);
    EXPECT_EQ(image->getSize(), imgInfo.size);
    EXPECT_EQ(image->getImageDesc().image_slice_pitch, imgInfo.slicePitch);
    EXPECT_GE(image->getImageDesc().image_slice_pitch, image->getImageDesc().image_row_pitch);
    EXPECT_EQ(image->getImageDesc().image_row_pitch, imgInfo.rowPitch);
    EXPECT_EQ(image->getQPitch(), imgInfo.qPitch);

    delete image;

    DebugManager.flags.ForceLinearImages.set(!defaultTiling);

    // query again
    surfaceFormat = Image::getSurfaceFormatFromTable(0, &imageFormat);
    MockGmm::queryImgParams(imgInfo);

    image = Image::create(
        context,
        0,
        surfaceFormat,
        &imageDesc,
        nullptr,
        retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ(image->getSize(), imgInfo.size);
    EXPECT_EQ(image->getImageDesc().image_slice_pitch, imgInfo.slicePitch);
    EXPECT_EQ(image->getImageDesc().image_row_pitch, imgInfo.rowPitch);
    EXPECT_GE(image->getImageDesc().image_slice_pitch, image->getImageDesc().image_row_pitch);
    EXPECT_EQ(image->getQPitch(), imgInfo.qPitch);

    delete image;
    DebugManager.flags.ForceLinearImages.set(defaultTiling);
}
