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

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/mem_obj/buffer.h"
#include "unit_tests/aub_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "test.h"

using namespace OCLRT;

struct CopyBufferHw
    : public CommandEnqueueAUBFixture,
      public ::testing::WithParamInterface<std::tuple<size_t, size_t>>,
      public ::testing::Test {

    void SetUp() override {
        CommandEnqueueAUBFixture::SetUp();
    }

    void TearDown() override {
        CommandEnqueueAUBFixture::TearDown();
    }
};

typedef CopyBufferHw AUBCopyBuffer;

HWTEST_P(AUBCopyBuffer, simple) {
    MockContext context(&pCmdQ->getDevice());

    cl_float srcMemory[] = {1.0f, 2.0f, 3.0f, 4.0f};
    cl_float dstMemory[] = {0.0f, 0.0f, 0.0f, 0.0f};
    auto retVal = CL_INVALID_VALUE;
    auto srcBuffer = Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        sizeof(srcMemory),
        srcMemory,
        retVal);
    ASSERT_NE(nullptr, srcBuffer);

    auto dstBuffer = Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        sizeof(dstMemory),
        dstMemory,
        retVal);
    ASSERT_NE(nullptr, dstBuffer);

    auto pSrcMemory = &srcMemory[0];

    size_t srcOffset = std::get<0>(GetParam());
    size_t dstOffset = std::get<1>(GetParam());
    size_t sizeCopied = sizeof(cl_float);
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    auto pDstMemory = (cl_float *)(dstBuffer->getGraphicsAllocation()->getGpuAddress());

    retVal = pCmdQ->enqueueCopyBuffer(
        srcBuffer,
        dstBuffer,
        srcOffset,
        dstOffset,
        sizeCopied,
        numEventsInWaitList,
        eventWaitList,
        event);

    EXPECT_EQ(CL_SUCCESS, retVal);

    pCmdQ->flush();

    pSrcMemory = ptrOffset(pSrcMemory, srcOffset);
    pDstMemory = ptrOffset(pDstMemory, dstOffset);

    // Compute our memory expecations based on kernel execution
    size_t sizeUserMemory = sizeof(dstMemory);
    AUBCommandStreamFixture::expectMemory<FamilyType>(pDstMemory, pSrcMemory, sizeCopied);

    // If the copykernel wasn't max sized, ensure we didn't overwrite existing memory
    if (dstOffset + sizeCopied < sizeUserMemory) {
        pDstMemory = ptrOffset(pDstMemory, sizeCopied);
        float *dstMemoryRef = ptrOffset(dstMemory, sizeCopied);

        size_t sizeRemaining = sizeUserMemory - sizeCopied - dstOffset;
        AUBCommandStreamFixture::expectMemory<FamilyType>(pDstMemory, dstMemoryRef, sizeRemaining);
    }

    delete srcBuffer;
    delete dstBuffer;
}

INSTANTIATE_TEST_CASE_P(AUBCopyBuffer_simple,
                        AUBCopyBuffer,
                        ::testing::Combine(
                            ::testing::Values( // srcOffset
                                0 * sizeof(cl_float),
                                1 * sizeof(cl_float),
                                2 * sizeof(cl_float),
                                3 * sizeof(cl_float)),
                            ::testing::Values( // dstOffset
                                0 * sizeof(cl_float),
                                1 * sizeof(cl_float),
                                2 * sizeof(cl_float),
                                3 * sizeof(cl_float))));
