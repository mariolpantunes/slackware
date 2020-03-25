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

#include "runtime/helpers/ptr_math.h"
#include "runtime/kernel/kernel.h"
#include "unit_tests/fixtures/kernel_arg_fixture.h"
#include "unit_tests/gen_common/test.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/mocks/mock_image.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"

#include "gtest/gtest.h"

using namespace OCLRT;

TEST_F(KernelImageArgTest, GIVENkernelWithImageArgsWHENcheckDifferentScenariosTHENproperBehaviour) {
    size_t imageWidth = image->getImageDesc().image_width;
    size_t imageHeight = image->getImageDesc().image_height;
    size_t imageDepth = image->getImageDesc().image_depth;
    uint32_t objectId = pKernelInfo->kernelArgInfo[4].offsetHeap;

    cl_mem memObj = image.get();

    pKernel->setArg(0, sizeof(memObj), &memObj);
    pKernel->setArg(1, sizeof(memObj), &memObj);
    pKernel->setArg(3, sizeof(memObj), &memObj);
    pKernel->setArg(4, sizeof(memObj), &memObj);

    auto crossThreadData = reinterpret_cast<uint32_t *>(pKernel->getCrossThreadData());
    auto imgWidthOffset = ptrOffset(crossThreadData, 0x4);
    EXPECT_EQ(imageWidth, *imgWidthOffset);

    auto imgHeightOffset = ptrOffset(crossThreadData, 0xc);
    EXPECT_EQ(imageHeight, *imgHeightOffset);

    auto dummyOffset = ptrOffset(crossThreadData, 0x20);
    EXPECT_EQ(0x12344321u, *dummyOffset);

    auto imgDepthOffset = ptrOffset(crossThreadData, 0x30);
    EXPECT_EQ(imageDepth, *imgDepthOffset);

    EXPECT_EQ(objectId, *crossThreadData);
}

TEST_F(KernelImageArgTest, givenKernelWithValidOffsetNumMipLevelsWhenImageArgIsSetThenCrossthreadDataIsProperlyPatched) {
    MockImageBase image;
    image.imageDesc.num_mip_levels = 7U;
    cl_mem imageObj = &image;

    pKernel->setArg(0, sizeof(imageObj), &imageObj);
    auto crossThreadData = reinterpret_cast<uint32_t *>(pKernel->getCrossThreadData());
    auto patchedNumMipLevels = ptrOffset(crossThreadData, offsetNumMipLevelsImage0);
    EXPECT_EQ(7U, *patchedNumMipLevels);
}

TEST_F(KernelImageArgTest, givenImageWithNumSamplesWhenSetArgIsCalledThenPatchNumSamplesInfo) {
    cl_image_format imgFormat = {CL_RGBA, CL_UNORM_INT8};
    cl_image_desc imgDesc = {};
    imgDesc.num_samples = 16;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imgDesc.image_width = 5;
    imgDesc.image_height = 5;

    auto surfaceFormat = Image::getSurfaceFormatFromTable(0, &imgFormat);
    auto sampleImg = Image::create(context.get(), 0, surfaceFormat, &imgDesc, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_mem memObj = sampleImg;

    pKernel->setArg(0, sizeof(memObj), &memObj);

    auto crossThreadData = reinterpret_cast<uint32_t *>(pKernel->getCrossThreadData());
    auto patchedNumSamples = ptrOffset(crossThreadData, 0x3c);
    EXPECT_EQ(16u, *patchedNumSamples);

    sampleImg->release();
}

TEST_F(KernelImageArgTest, givenImageWithWriteOnlyAccessAndReadOnlyArgWhenCheckCorrectImageAccessQualifierIsCalledThenRetValNotValid) {
    cl_image_format imgFormat = {CL_RGBA, CL_UNORM_INT8};
    cl_image_desc imgDesc = {};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    imgDesc.image_width = 5;
    imgDesc.image_height = 5;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(0, &imgFormat);
    std::unique_ptr<Image> img(Image::create(context.get(), flags, surfaceFormat, &imgDesc, nullptr, retVal));
    pKernelInfo->kernelArgInfo[0].accessQualifier = CL_KERNEL_ARG_ACCESS_READ_ONLY;
    cl_mem memObj = img.get();
    retVal = pKernel->checkCorrectImageAccessQualifier(0, sizeof(memObj), &memObj);

    EXPECT_EQ(retVal, CL_INVALID_ARG_VALUE);
    retVal = clSetKernelArg(
        pKernel.get(),
        0,
        sizeof(memObj),
        &memObj);

    EXPECT_EQ(retVal, CL_INVALID_ARG_VALUE);

    retVal = clSetKernelArg(
        pKernel.get(),
        0,
        sizeof(memObj),
        &memObj);

    EXPECT_EQ(retVal, CL_INVALID_ARG_VALUE);

    retVal = clSetKernelArg(
        pKernel.get(),
        1000,
        sizeof(memObj),
        &memObj);

    EXPECT_EQ(retVal, CL_INVALID_ARG_INDEX);
}

TEST_F(KernelImageArgTest, givenImageWithReadOnlyAccessAndWriteOnlyArgWhenCheckCorrectImageAccessQualifierIsCalledThenReturnsInvalidArgValue) {
    cl_image_format imgFormat = {CL_RGBA, CL_UNORM_INT8};
    cl_image_desc imgDesc = {};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    imgDesc.image_width = 5;
    imgDesc.image_height = 5;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(0, &imgFormat);
    std::unique_ptr<Image> img(Image::create(context.get(), flags, surfaceFormat, &imgDesc, nullptr, retVal));
    pKernelInfo->kernelArgInfo[0].accessQualifier = CL_KERNEL_ARG_ACCESS_WRITE_ONLY;
    cl_mem memObj = img.get();
    retVal = pKernel->checkCorrectImageAccessQualifier(0, sizeof(memObj), &memObj);

    EXPECT_EQ(retVal, CL_INVALID_ARG_VALUE);
    Image *image = NULL;
    memObj = image;
    retVal = pKernel->checkCorrectImageAccessQualifier(0, sizeof(memObj), &memObj);
    EXPECT_EQ(retVal, CL_INVALID_ARG_VALUE);
}

TEST_F(KernelImageArgTest, givenImageWithReadOnlyAccessAndReadOnlyArgWhenCheckCorrectImageAccessQualifierIsCalledThenRetValNotValid) {
    cl_image_format imgFormat = {CL_RGBA, CL_UNORM_INT8};
    cl_image_desc imgDesc = {};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    imgDesc.image_width = 5;
    imgDesc.image_height = 5;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(0, &imgFormat);
    std::unique_ptr<Image> img(Image::create(context.get(), flags, surfaceFormat, &imgDesc, nullptr, retVal));
    pKernelInfo->kernelArgInfo[0].accessQualifier = CL_KERNEL_ARG_ACCESS_READ_ONLY;
    cl_mem memObj = img.get();
    retVal = pKernel->checkCorrectImageAccessQualifier(0, sizeof(memObj), &memObj);

    EXPECT_EQ(retVal, CL_SUCCESS);
}

TEST_F(KernelImageArgTest, givenImageWithWriteOnlyAccessAndWriteOnlyArgWhenCheckCorrectImageAccessQualifierIsCalledThenRetValNotValid) {
    cl_image_format imgFormat = {CL_RGBA, CL_UNORM_INT8};
    cl_image_desc imgDesc = {};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    imgDesc.image_width = 5;
    imgDesc.image_height = 5;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(0, &imgFormat);
    std::unique_ptr<Image> img(Image::create(context.get(), flags, surfaceFormat, &imgDesc, nullptr, retVal));
    pKernelInfo->kernelArgInfo[0].accessQualifier = CL_KERNEL_ARG_ACCESS_WRITE_ONLY;
    cl_mem memObj = img.get();
    retVal = pKernel->checkCorrectImageAccessQualifier(0, sizeof(memObj), &memObj);

    EXPECT_EQ(retVal, CL_SUCCESS);
}

HWTEST_F(KernelImageArgTest, givenImgWithMcsAllocWhenMakeResidentThenMakeMcsAllocationResident) {
    int32_t execStamp = 0;
    cl_image_format imgFormat = {CL_RGBA, CL_UNORM_INT8};
    cl_image_desc imgDesc = {};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imgDesc.image_width = 5;
    imgDesc.image_height = 5;

    auto surfaceFormat = Image::getSurfaceFormatFromTable(0, &imgFormat);
    auto img = Image::create(context.get(), 0, surfaceFormat, &imgDesc, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto mcsAlloc = context->getMemoryManager()->allocateGraphicsMemory(4096);
    img->setMcsAllocation(mcsAlloc);
    cl_mem memObj = img;
    pKernel->setArg(0, sizeof(memObj), &memObj);

    std::unique_ptr<OsAgnosticMemoryManager> memoryManager(new OsAgnosticMemoryManager());
    std::unique_ptr<MockCsr<FamilyType>> csr(new MockCsr<FamilyType>(execStamp));
    csr->setMemoryManager(memoryManager.get());

    pKernel->makeResident(*csr.get());
    EXPECT_TRUE(csr->isMadeResident(mcsAlloc));

    csr->makeSurfacePackNonResident(nullptr);

    EXPECT_TRUE(csr->isMadeNonResident(mcsAlloc));

    delete img;
}

TEST_F(KernelImageArgTest, givenKernelWithSettedArgWhenUnSetCalledThenArgIsUnsetAndArgCountIsDecreased) {
    cl_image_format imgFormat = {CL_RGBA, CL_UNORM_INT8};
    cl_image_desc imgDesc = {};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    imgDesc.image_width = 5;
    imgDesc.image_height = 5;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(0, &imgFormat);
    std::unique_ptr<Image> img(Image::create(context.get(), flags, surfaceFormat, &imgDesc, nullptr, retVal));
    cl_mem memObj = img.get();

    retVal = pKernel->setArg(0, sizeof(memObj), &memObj);
    EXPECT_EQ(1u, pKernel->getPatchedArgumentsNum());
    EXPECT_TRUE(pKernel->getKernelArguments()[0].isPatched);
    pKernel->unsetArg(0);
    EXPECT_EQ(0u, pKernel->getPatchedArgumentsNum());
    EXPECT_FALSE(pKernel->getKernelArguments()[0].isPatched);
}

TEST_F(KernelImageArgTest, givenNullKernelWhenClSetKernelArgCalledThenInvalidKernelCodeReturned) {
    cl_mem memObj = NULL;
    retVal = clSetKernelArg(
        NULL,
        1000,
        sizeof(memObj),
        &memObj);

    EXPECT_EQ(retVal, CL_INVALID_KERNEL);
}

class MockSharingHandler : public SharingHandler {
  public:
    void synchronizeObject(UpdateData *updateData) override {
        updateData->synchronizationStatus = ACQUIRE_SUCCESFUL;
    }
};

TEST_F(KernelImageArgTest, givenKernelWithSharedImageWhenSetArgCalledThenUsingSharedObjArgsShouldBeTrue) {
    cl_image_format imgFormat = {CL_RGBA, CL_UNORM_INT8};
    cl_image_desc imgDesc = {};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    imgDesc.image_width = 5;
    imgDesc.image_height = 5;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(0, &imgFormat);
    std::unique_ptr<Image> img(Image::create(context.get(), flags, surfaceFormat, &imgDesc, nullptr, retVal));
    cl_mem memObj = img.get();

    MockSharingHandler *mockSharingHandler = new MockSharingHandler;
    img->setSharingHandler(mockSharingHandler);

    retVal = pKernel->setArg(0, sizeof(memObj), &memObj);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, pKernel->getPatchedArgumentsNum());
    EXPECT_TRUE(pKernel->getKernelArguments()[0].isPatched);
    EXPECT_TRUE(pKernel->isUsingSharedObjArgs());
}
