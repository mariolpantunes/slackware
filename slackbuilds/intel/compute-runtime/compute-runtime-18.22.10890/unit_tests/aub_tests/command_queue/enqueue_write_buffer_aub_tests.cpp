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
#include "runtime/device/device.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/mem_obj/buffer.h"
#include "unit_tests/aub_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "test.h"

using namespace OCLRT;

struct WriteBufferHw
    : public CommandEnqueueAUBFixture,
      public ::testing::WithParamInterface<size_t>,
      public ::testing::Test {

    void SetUp() override {
        CommandEnqueueAUBFixture::SetUp();
    }

    void TearDown() override {
        CommandEnqueueAUBFixture::TearDown();
    }
};

typedef WriteBufferHw AUBWriteBuffer;

HWTEST_P(AUBWriteBuffer, simple) {
    MockContext context(&this->pCmdQ->getDevice());

    cl_float *srcMemory = new float[1024];
    cl_float *destMemory = new float[1024];
    cl_float *zeroMemory = new float[1024];

    for (int i = 0; i < 1024; i++) {
        srcMemory[i] = (float)i + 1.0f;
        destMemory[i] = 0;
        zeroMemory[i] = 0;
    }

    auto retVal = CL_INVALID_VALUE;
    auto dstBuffer = Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        1024 * sizeof(float),
        destMemory,
        retVal);
    ASSERT_NE(nullptr, dstBuffer);

    auto pSrcMemory = &srcMemory[0];

    cl_bool blockingWrite = CL_TRUE;
    size_t offset = GetParam();
    size_t sizeWritten = sizeof(cl_float);
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    dstBuffer->forceDisallowCPUCopy = true;
    retVal = pCmdQ->enqueueWriteBuffer(
        dstBuffer,
        blockingWrite,
        offset,
        sizeWritten,
        pSrcMemory,
        numEventsInWaitList,
        eventWaitList,
        event);

    auto pDestMemory = reinterpret_cast<decltype(destMemory)>((dstBuffer->getGraphicsAllocation()->getGpuAddress()));
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);

    // Compute our memory expecations based on kernel execution
    size_t sizeUserMemory = 1024 * sizeof(float);
    auto pVal = ptrOffset(pDestMemory, offset);
    AUBCommandStreamFixture::expectMemory<FamilyType>(pVal, pSrcMemory, sizeWritten);

    // if offset provided, check the beginning
    if (offset > 0) {
        AUBCommandStreamFixture::expectMemory<FamilyType>(pDestMemory, zeroMemory, offset);
    }
    // If the copykernel wasn't max sized, ensure we didn't overwrite existing memory
    if (offset + sizeWritten < sizeUserMemory) {
        pDestMemory = ptrOffset(pVal, sizeWritten);

        size_t sizeRemaining = sizeUserMemory - sizeWritten - offset;
        AUBCommandStreamFixture::expectMemory<FamilyType>(pDestMemory, zeroMemory, sizeRemaining);
    }
    delete dstBuffer;
    delete[] srcMemory;
    delete[] destMemory;
    delete[] zeroMemory;
}

INSTANTIATE_TEST_CASE_P(AUBWriteBuffer_simple,
                        AUBWriteBuffer,
                        ::testing::Values(
                            0 * sizeof(cl_float),
                            1 * sizeof(cl_float),
                            2 * sizeof(cl_float),
                            3 * sizeof(cl_float)));
