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
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_event.h"

#include "test.h"
#include "gtest/gtest.h"

using namespace OCLRT;

struct CommandQueueSimpleTest
    : public DeviceFixture,
      public ContextFixture,
      public ::testing::Test {

    using ContextFixture::SetUp;

    CommandQueueSimpleTest() {
    }

    void SetUp() override {
        DeviceFixture::SetUp();
        cl_device_id device = pDevice;
        ContextFixture::SetUp(1, &device);
    }

    void TearDown() override {
        ContextFixture::TearDown();
        DeviceFixture::TearDown();
    }
};

HWTEST_F(CommandQueueSimpleTest, flushWaitlistDoesNotFlushSingleEventWhenTaskCountIsAlreadySent) {
    MockCommandQueue commandQueue(pContext, pDevice, 0);

    MockEvent<Event> event(&commandQueue, CL_COMMAND_NDRANGE_KERNEL, 1, 1);
    cl_event clEvent = &event;

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto *gfxAllocation = pDevice->getMemoryManager()->allocateGraphicsMemory(4096);
    IndirectHeap stream(gfxAllocation);

    // Update latestSentTaskCount to == 1
    DispatchFlags dispatchFlags;
    dispatchFlags.blocking = true;
    dispatchFlags.dcFlush = true;

    csr.flushTask(stream, 0, stream, stream, stream, 0, dispatchFlags);

    EXPECT_EQ(1u, csr.peekLatestSentTaskCount());
    csr.taskCount = 1;
    // taskLevel less than event's level
    csr.taskLevel = 0;

    commandQueue.flushWaitList(1, &clEvent, true);

    EXPECT_EQ(0u, csr.peekTaskLevel());
    EXPECT_EQ(1u, csr.peekTaskCount());

    pDevice->getMemoryManager()->freeGraphicsMemory(gfxAllocation);
}