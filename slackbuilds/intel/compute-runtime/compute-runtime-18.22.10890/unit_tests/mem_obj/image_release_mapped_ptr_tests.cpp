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

#include "test.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/mem_obj/image.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_event.h"
#include "unit_tests/fixtures/device_fixture.h"

using namespace OCLRT;

template <typename Family>
class MyMockCommandQueue : public CommandQueueHw<Family> {

  public:
    MyMockCommandQueue(Context *context) : CommandQueueHw<Family>(context, nullptr, 0){};

    cl_int enqueueWriteImage(Image *dstImage, cl_bool blockingWrite,
                             const size_t *origin, const size_t *region,
                             size_t inputRowPitch, size_t inputSlicePitch,
                             const void *ptr, cl_uint numEventsInWaitList,
                             const cl_event *eventWaitList,
                             cl_event *event) override {
        passedBlockingWrite = blockingWrite;
        passedPtr = (void *)ptr;
        enqueueWriteImageCalled++;
        return CL_SUCCESS;
    }
    cl_int finish(bool dcFlush) override {
        EXPECT_TRUE(dcFlush);
        finishCalled++;
        return CL_SUCCESS;
    }
    void *passedPtr = nullptr;
    cl_bool passedBlockingWrite = CL_INVALID_VALUE;
    unsigned int enqueueWriteImageCalled = 0;
    unsigned int finishCalled = 0;
};

class ImageUnmapTest : public ::testing::Test {
  public:
    void SetUp() override {
        image.reset(ImageHelper<ImageReadOnly<Image3dDefaults>>::create(&context));
    }
    MockContext context;
    std::unique_ptr<Image> image;
};

HWTEST_F(ImageUnmapTest, givenImageWhenUnmapMemObjIsCalledThenEnqueueNonBlockingMapImage) {
    std::unique_ptr<MyMockCommandQueue<FamilyType>> commandQueue(new MyMockCommandQueue<FamilyType>(&context));
    void *ptr = alignedMalloc(MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);
    MemObjOffsetArray origin = {{0, 0, 0}};
    MemObjSizeArray region = {{1, 1, 1}};
    image->setAllocatedMapPtr(ptr);
    cl_map_flags mapFlags = CL_MAP_WRITE;
    image->addMappedPtr(ptr, 1, mapFlags, region, origin, 0);
    commandQueue->enqueueUnmapMemObject(image.get(), ptr, 0, nullptr, nullptr);
    EXPECT_EQ(ptr, commandQueue->passedPtr);
    EXPECT_EQ((cl_bool)CL_FALSE, commandQueue->passedBlockingWrite);
    EXPECT_EQ(1u, commandQueue->enqueueWriteImageCalled);
}

HWTEST_F(ImageUnmapTest, givenImageWhenUnmapMemObjIsCalledWithMemUseHostPtrAndWithoutEventsThenFinishIsCalled) {
    std::unique_ptr<MyMockCommandQueue<FamilyType>> commandQueue(new MyMockCommandQueue<FamilyType>(&context));
    image.reset(ImageHelper<ImageUseHostPtr<Image3dDefaults>>::create(&context));
    auto ptr = image->getBasePtrForMap();
    MemObjOffsetArray origin = {{0, 0, 0}};
    MemObjSizeArray region = {{1, 1, 1}};
    cl_map_flags mapFlags = CL_MAP_WRITE;
    image->addMappedPtr(ptr, 1, mapFlags, region, origin, 0);
    commandQueue->enqueueUnmapMemObject(image.get(), ptr, 0, nullptr, nullptr);
    EXPECT_EQ(1u, commandQueue->finishCalled);
}

HWTEST_F(ImageUnmapTest, givenImageWhenUnmapMemObjIsCalledWithoutMemUseHostPtrThenFinishIsCalled) {
    std::unique_ptr<MyMockCommandQueue<FamilyType>> commandQueue(new MyMockCommandQueue<FamilyType>(&context));
    auto ptr = image->getBasePtrForMap();
    MemObjOffsetArray origin = {{0, 0, 0}};
    MemObjSizeArray region = {{1, 1, 1}};
    cl_map_flags mapFlags = CL_MAP_WRITE;
    image->addMappedPtr(ptr, 2, mapFlags, region, origin, 0);
    commandQueue->enqueueUnmapMemObject(image.get(), ptr, 0, nullptr, nullptr);
    EXPECT_EQ(1u, commandQueue->finishCalled);
}

HWTEST_F(ImageUnmapTest, givenImageWhenUnmapMemObjIsCalledWithMemUseHostPtrAndWithNotReadyEventsThenFinishIsNotCalled) {
    std::unique_ptr<MyMockCommandQueue<FamilyType>> commandQueue(new MyMockCommandQueue<FamilyType>(&context));
    image.reset(ImageHelper<ImageUseHostPtr<Image3dDefaults>>::create(&context));

    MockEvent<UserEvent> mockEvent(&context);
    mockEvent.setStatus(Event::eventNotReady);
    cl_event clEvent = &mockEvent;
    commandQueue->enqueueUnmapMemObject(image.get(), nullptr, 1, &clEvent, nullptr);
    EXPECT_EQ(0u, commandQueue->finishCalled);
}

HWTEST_F(ImageUnmapTest, givenImageWhenUnmapMemObjIsCalledWithMemUseHostPtrAndWithoutNotReadyEventsThenFinishIsCalled) {
    std::unique_ptr<MyMockCommandQueue<FamilyType>> commandQueue(new MyMockCommandQueue<FamilyType>(&context));
    image.reset(ImageHelper<ImageUseHostPtr<Image3dDefaults>>::create(&context));
    MockEvent<UserEvent> mockEvent(&context);
    mockEvent.setStatus(0);
    cl_event clEvent = &mockEvent;
    auto ptr = image->getBasePtrForMap();
    MemObjOffsetArray origin = {{0, 0, 0}};
    MemObjSizeArray region = {{1, 1, 1}};
    cl_map_flags mapFlags = CL_MAP_WRITE;
    image->addMappedPtr(ptr, 1, mapFlags, region, origin, 0);
    commandQueue->enqueueUnmapMemObject(image.get(), ptr, 1, &clEvent, nullptr);
    EXPECT_EQ(1u, commandQueue->finishCalled);
}

TEST_F(ImageUnmapTest, givenImageWhenEnqueueMapImageIsCalledTwiceThenAllocatedMemoryPtrIsNotOverridden) {
    cl_int retVal;
    size_t origin[] = {0, 0, 0};
    size_t region[] = {0, 0, 0};
    std::unique_ptr<MockDevice> device(DeviceHelper<>::create());
    std::unique_ptr<CommandQueue> commandQueue(CommandQueue::create(&context, device.get(), nullptr, retVal));
    commandQueue->enqueueMapImage(image.get(), CL_FALSE, 0, origin, region, nullptr, nullptr, 0, nullptr, nullptr, retVal);
    EXPECT_NE(nullptr, image->getAllocatedMapPtr());
    void *ptr = image->getAllocatedMapPtr();
    EXPECT_EQ(alignUp(ptr, MemoryConstants::pageSize), ptr);
    commandQueue->enqueueMapImage(image.get(), CL_FALSE, 0, origin, region, nullptr, nullptr, 0, nullptr, nullptr, retVal);
    EXPECT_EQ(ptr, image->getAllocatedMapPtr());
    commandQueue->enqueueUnmapMemObject(image.get(), ptr, 0, nullptr, nullptr);
}
