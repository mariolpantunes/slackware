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

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/mem_obj/buffer.h"
#include "unit_tests/aub_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "test.h"

using namespace OCLRT;

struct ReadBufferRectHw
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

typedef ReadBufferRectHw AUBReadBufferRect;
static const size_t width = 10;

HWTEST_P(AUBReadBufferRect, simple3D) {
    MockContext context(this->pDevice);
    size_t rowPitch = width;
    size_t slicePitch = rowPitch * rowPitch;

    size_t bufferSizeBuff = rowPitch * rowPitch * rowPitch;
    size_t bufferSize = alignUp(bufferSizeBuff, 4096);

    size_t zHostOffs;
    size_t zBuffOffs;
    std::tie(zBuffOffs, zHostOffs) = GetParam();

    ASSERT_LT(zBuffOffs, width);
    ASSERT_LT(zHostOffs, width);

    uint8_t *srcMemory = (uint8_t *)::alignedMalloc(bufferSize, 4096);
    uint8_t *destMemory = (uint8_t *)::alignedMalloc(bufferSize, 4096);

    for (unsigned int i = 0; i < bufferSize; i++)
        srcMemory[i] = i;

    memset(destMemory, 0x00, bufferSize);

    auto retVal = CL_INVALID_VALUE;
    auto srcBuffer = Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        bufferSize,
        srcMemory,
        retVal);
    ASSERT_NE(nullptr, srcBuffer);

    cl_bool blockingRead = CL_TRUE;

    pCommandStreamReceiver->createAllocationAndHandleResidency(destMemory, bufferSize);

    size_t bufferOrigin[] = {0, 0, zBuffOffs};
    size_t hostOrigin[] = {0, 0, zHostOffs};
    size_t region[] = {rowPitch, rowPitch, 1};

    retVal = pCmdQ->enqueueReadBufferRect(
        srcBuffer,
        blockingRead,
        bufferOrigin,
        hostOrigin,
        region,
        rowPitch,
        slicePitch,
        rowPitch,
        slicePitch,
        destMemory,
        0,
        nullptr,
        nullptr);

    delete srcBuffer;

    EXPECT_EQ(CL_SUCCESS, retVal);

    char *ptr = new char[slicePitch];
    memset(ptr, 0, slicePitch);
    for (unsigned int i = 0; i < rowPitch; i++) {
        //one slice will be copied from src. all others should be zeros
        if (i == zHostOffs) {
            AUBCommandStreamFixture::expectMemory<FamilyType>(destMemory + slicePitch * i, srcMemory + slicePitch * zBuffOffs, slicePitch);
        } else {
            AUBCommandStreamFixture::expectMemory<FamilyType>(destMemory + slicePitch * i, ptr, slicePitch);
        }
    }
    delete[] ptr;

    ::alignedFree(srcMemory);
    ::alignedFree(destMemory);
}
INSTANTIATE_TEST_CASE_P(AUBReadBufferRect_simple,
                        AUBReadBufferRect,
                        ::testing::Combine(
                            ::testing::Values(0, 1, 2, 3, 4),
                            ::testing::Values(0, 1, 2, 3, 4)));
