/*
 * Copyright (c) 2018, Intel Corporation
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

#include "runtime/kernel/image_transformer.h"
#include "runtime/program/kernel_info.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "test.h"

using namespace OCLRT;

class ImageTransformerTest : public ::testing::Test {
  public:
    void SetUp() override {
        using SimpleKernelArgInfo = Kernel::SimpleKernelArgInfo;
        pKernelInfo.reset(KernelInfo::create());
        pKernelInfo->kernelArgInfo.resize(2);
        pKernelInfo->kernelArgInfo[0].isTransformable = true;
        pKernelInfo->kernelArgInfo[0].offsetHeap = firstImageOffset;
        pKernelInfo->kernelArgInfo[1].isTransformable = true;
        pKernelInfo->kernelArgInfo[1].offsetHeap = secondImageOffset;
        image1.reset(Image3dHelper<>::create(&context));
        image2.reset(Image3dHelper<>::create(&context));
        SimpleKernelArgInfo imageArg1;
        SimpleKernelArgInfo imageArg2;
        clImage1 = static_cast<cl_mem>(image2.get());
        clImage2 = static_cast<cl_mem>(image2.get());
        imageArg1.value = &clImage1;
        imageArg2.value = &clImage2;
        kernelArguments.push_back(imageArg1);
        kernelArguments.push_back(imageArg2);
    }
    const int firstImageOffset = 0x20;
    const int secondImageOffset = 0x40;
    std::unique_ptr<KernelInfo> pKernelInfo;
    ImageTransformer imageTransformer;
    MockContext context;
    std::unique_ptr<Image> image1;
    std::unique_ptr<Image> image2;
    cl_mem clImage1;
    cl_mem clImage2;
    char ssh[0x80];
    std::vector<Kernel::SimpleKernelArgInfo> kernelArguments;
};

TEST_F(ImageTransformerTest, givenImageTransformerWhenRegisterImage3dThenTransformerHasRegisteredImages3d) {
    bool retVal;
    retVal = imageTransformer.hasRegisteredImages3d();
    EXPECT_FALSE(retVal);
    imageTransformer.registerImage3d(0);
    retVal = imageTransformer.hasRegisteredImages3d();
    EXPECT_TRUE(retVal);
}

TEST_F(ImageTransformerTest, givenImageTransformerWhenTransformToImage2dArrayThenTransformerDidTransform) {
    bool retVal;
    retVal = imageTransformer.didTransform();
    EXPECT_FALSE(retVal);
    imageTransformer.transformImagesTo2dArray(*pKernelInfo, kernelArguments, nullptr);
    retVal = imageTransformer.didTransform();
    EXPECT_TRUE(retVal);
}

TEST_F(ImageTransformerTest, givenImageTransformerWhenTransformToImage3dThenTransformerDidNotTransform) {
    bool retVal;
    retVal = imageTransformer.didTransform();
    EXPECT_FALSE(retVal);
    imageTransformer.transformImagesTo2dArray(*pKernelInfo, kernelArguments, nullptr);
    imageTransformer.transformImagesTo3d(*pKernelInfo, kernelArguments, nullptr);
    retVal = imageTransformer.didTransform();
    EXPECT_FALSE(retVal);
}
HWTEST_F(ImageTransformerTest, givenImageTransformerWhenTransformToImage2dArrayThenTransformOnlyRegisteredImages) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;

    auto firstSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, firstImageOffset));
    auto secondSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, secondImageOffset));

    firstSurfaceState->setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL);
    secondSurfaceState->setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL);
    firstSurfaceState->setSurfaceArray(false);
    secondSurfaceState->setSurfaceArray(false);

    imageTransformer.registerImage3d(1);
    imageTransformer.transformImagesTo2dArray(*pKernelInfo, kernelArguments, ssh);

    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL, firstSurfaceState->getSurfaceType());
    EXPECT_FALSE(firstSurfaceState->getSurfaceArray());

    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D, secondSurfaceState->getSurfaceType());
    EXPECT_TRUE(secondSurfaceState->getSurfaceArray());
}

HWTEST_F(ImageTransformerTest, givenImageTransformerWhenTransformToImage2dArrayThenTransformOnlyTransformableImages) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    pKernelInfo->kernelArgInfo[1].isTransformable = false;

    auto firstSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, firstImageOffset));
    auto secondSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, secondImageOffset));

    firstSurfaceState->setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL);
    secondSurfaceState->setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL);
    firstSurfaceState->setSurfaceArray(false);
    secondSurfaceState->setSurfaceArray(false);

    imageTransformer.registerImage3d(0);
    imageTransformer.registerImage3d(1);
    imageTransformer.transformImagesTo2dArray(*pKernelInfo, kernelArguments, ssh);

    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D, firstSurfaceState->getSurfaceType());
    EXPECT_TRUE(firstSurfaceState->getSurfaceArray());

    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL, secondSurfaceState->getSurfaceType());
    EXPECT_FALSE(secondSurfaceState->getSurfaceArray());
}

HWTEST_F(ImageTransformerTest, givenImageTransformerWhenTransformToImage3dhenTransformAllRegisteredImages) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    pKernelInfo->kernelArgInfo[1].isTransformable = false;

    auto firstSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, firstImageOffset));
    auto secondSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, secondImageOffset));

    firstSurfaceState->setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL);
    secondSurfaceState->setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL);
    firstSurfaceState->setSurfaceArray(true);
    secondSurfaceState->setSurfaceArray(true);

    imageTransformer.registerImage3d(0);
    imageTransformer.registerImage3d(1);
    imageTransformer.transformImagesTo3d(*pKernelInfo, kernelArguments, ssh);

    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_3D, firstSurfaceState->getSurfaceType());
    EXPECT_FALSE(firstSurfaceState->getSurfaceArray());

    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_3D, secondSurfaceState->getSurfaceType());
    EXPECT_FALSE(secondSurfaceState->getSurfaceArray());
}

HWTEST_F(ImageTransformerTest, givenImageTransformerWhenTransformToImage3dhenTransformOnlyRegisteredImages) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    pKernelInfo->kernelArgInfo[1].isTransformable = false;

    auto firstSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, firstImageOffset));
    auto secondSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, secondImageOffset));

    firstSurfaceState->setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL);
    secondSurfaceState->setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL);
    firstSurfaceState->setSurfaceArray(true);
    secondSurfaceState->setSurfaceArray(true);

    imageTransformer.registerImage3d(1);
    imageTransformer.transformImagesTo3d(*pKernelInfo, kernelArguments, ssh);

    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL, firstSurfaceState->getSurfaceType());
    EXPECT_TRUE(firstSurfaceState->getSurfaceArray());

    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_3D, secondSurfaceState->getSurfaceType());
    EXPECT_FALSE(secondSurfaceState->getSurfaceArray());
}

class MockImageTransformer : public ImageTransformer {
  public:
    using ImageTransformer::argIndexes;
};
TEST(ImageTransformerRegisterImageTest, givenImageTransformerWhenRegisterTheSameImageTwiceThenAppendOnlyOne) {
    MockImageTransformer transformer;
    EXPECT_EQ(0u, transformer.argIndexes.size());
    transformer.registerImage3d(0);
    EXPECT_EQ(1u, transformer.argIndexes.size());
    transformer.registerImage3d(0);
    EXPECT_EQ(1u, transformer.argIndexes.size());
    transformer.registerImage3d(1);
    EXPECT_EQ(2u, transformer.argIndexes.size());
}
