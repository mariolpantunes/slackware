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

#include "runtime/command_queue/command_queue.h"
#include "runtime/event/event.h"
#include "unit_tests/command_queue/buffer_operations_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "gtest/gtest.h"
#include <memory>

using namespace OCLRT;

TEST_F(EnqueueWriteBufferTypeTest, eventShouldBeReturned) {
    cl_bool blockingWrite = CL_TRUE;
    size_t offset = 0;
    size_t size = sizeof(cl_float);
    cl_float pDestMemory[] = {0.0f, 0.0f, 0.0f, 0.0f};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event event = nullptr;

    auto retVal = CL_INVALID_VALUE;
    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    retVal = pCmdQ->enqueueWriteBuffer(
        srcBuffer.get(),
        blockingWrite,
        offset,
        size,
        pDestMemory,
        numEventsInWaitList,
        eventWaitList,
        &event);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_EQ(pCmdQ->taskLevel, pEvent->taskLevel);

    // Check CL_EVENT_COMMAND_TYPE
    {
        cl_command_type cmdType = 0;
        size_t sizeReturned = 0;
        auto result = clGetEventInfo(pEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
        ASSERT_EQ(CL_SUCCESS, result);
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_WRITE_BUFFER), cmdType);
        EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    }

    delete pEvent;
}

TEST_F(EnqueueWriteBufferTypeTest, eventReturnedShouldBeMaxOfInputEventsAndCmdQPlus1) {
    uint32_t taskLevelCmdQ = 17;
    pCmdQ->taskLevel = taskLevelCmdQ;

    uint32_t taskLevelEvent1 = 8;
    uint32_t taskLevelEvent2 = 19;
    Event event1(nullptr, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent1, 4);
    Event event2(nullptr, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent2, 10);

    cl_bool blockingWrite = CL_TRUE;
    size_t offset = 0;
    size_t size = sizeof(cl_float);
    cl_float pDestMemory[] = {0.0f, 0.0f, 0.0f, 0.0f};
    cl_event eventWaitList[] =
        {
            &event1,
            &event2};
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;

    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    auto retVal = pCmdQ->enqueueWriteBuffer(
        srcBuffer.get(),
        blockingWrite,
        offset,
        size,
        pDestMemory,
        numEventsInWaitList,
        eventWaitList,
        &event);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_LE(19u, pEvent->taskLevel);

    delete pEvent;
}

TEST_F(EnqueueWriteBufferTypeTest, givenInOrderQueueAndEnabledSupportCpuCopiesAndDstPtrEqualSrcPtrWithEventsWhenWriteBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.DoCpuCopyOnWriteBuffer.set(true);
    cl_int retVal = CL_SUCCESS;
    uint32_t taskLevelCmdQ = 17;
    pCmdQ->taskLevel = taskLevelCmdQ;

    uint32_t taskLevelEvent1 = 8;
    uint32_t taskLevelEvent2 = 19;
    Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent1, 4);
    Event event2(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent2, 10);

    cl_bool blockingRead = CL_TRUE;
    size_t size = sizeof(cl_float);
    cl_event eventWaitList[] =
        {
            &event1,
            &event2};
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;
    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();
    retVal = pCmdQ->enqueueWriteBuffer(srcBuffer.get(),
                                       blockingRead,
                                       0,
                                       size,
                                       ptr,
                                       numEventsInWaitList,
                                       eventWaitList,
                                       &event);

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_EQ(19u, pEvent->taskLevel);
    EXPECT_EQ(17u, pCmdQ->taskLevel);

    pEvent->release();
}
TEST_F(EnqueueWriteBufferTypeTest, givenOutOfOrderQueueAndEnabledSupportCpuCopiesAndDstPtrEqualSrcPtrWithEventsWhenWriteBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.DoCpuCopyOnWriteBuffer.set(true);
    std::unique_ptr<CommandQueue> pCmdOOQ(createCommandQueue(pDevice, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE));
    cl_int retVal = CL_SUCCESS;
    uint32_t taskLevelCmdQ = 17;
    pCmdOOQ->taskLevel = taskLevelCmdQ;

    uint32_t taskLevelEvent1 = 8;
    uint32_t taskLevelEvent2 = 19;
    Event event1(pCmdOOQ.get(), CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent1, 4);
    Event event2(pCmdOOQ.get(), CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent2, 10);

    cl_bool blockingRead = CL_TRUE;
    size_t size = sizeof(cl_float);
    cl_event eventWaitList[] =
        {
            &event1,
            &event2};
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;
    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();
    retVal = pCmdOOQ->enqueueWriteBuffer(srcBuffer.get(),
                                         blockingRead,
                                         0,
                                         size,
                                         ptr,
                                         numEventsInWaitList,
                                         eventWaitList,
                                         &event);

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_EQ(19u, pEvent->taskLevel);
    EXPECT_EQ(17u, pCmdOOQ->taskLevel);

    pEvent->release();
}
TEST_F(EnqueueWriteBufferTypeTest, givenInOrderQueueAndDisabledSupportCpuCopiesAndDstPtrEqualSrcPtrWithEventsWhenWriteBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.DoCpuCopyOnWriteBuffer.set(false);
    cl_int retVal = CL_SUCCESS;
    uint32_t taskLevelCmdQ = 17;
    pCmdQ->taskLevel = taskLevelCmdQ;

    uint32_t taskLevelEvent1 = 8;
    uint32_t taskLevelEvent2 = 19;
    Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent1, 4);
    Event event2(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent2, 10);

    cl_bool blockingRead = CL_TRUE;
    size_t size = sizeof(cl_float);
    cl_event eventWaitList[] =
        {
            &event1,
            &event2};
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;
    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();
    retVal = pCmdQ->enqueueWriteBuffer(srcBuffer.get(),
                                       blockingRead,
                                       0,
                                       size,
                                       ptr,
                                       numEventsInWaitList,
                                       eventWaitList,
                                       &event);

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_EQ(19u, pEvent->taskLevel);
    EXPECT_EQ(19u, pCmdQ->taskLevel);

    pEvent->release();
}
TEST_F(EnqueueWriteBufferTypeTest, givenOutOfOrderQueueAndDisabledSupportCpuCopiesAndDstPtrEqualSrcPtrWithEventsWhenWriteBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.DoCpuCopyOnWriteBuffer.set(false);
    std::unique_ptr<CommandQueue> pCmdOOQ(createCommandQueue(pDevice, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE));
    cl_int retVal = CL_SUCCESS;
    uint32_t taskLevelCmdQ = 17;
    pCmdOOQ->taskLevel = taskLevelCmdQ;

    uint32_t taskLevelEvent1 = 8;
    uint32_t taskLevelEvent2 = 19;
    Event event1(pCmdOOQ.get(), CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent1, 4);
    Event event2(pCmdOOQ.get(), CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent2, 10);

    cl_bool blockingRead = CL_TRUE;
    size_t size = sizeof(cl_float);
    cl_event eventWaitList[] =
        {
            &event1,
            &event2};
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;
    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();
    retVal = pCmdOOQ->enqueueWriteBuffer(srcBuffer.get(),
                                         blockingRead,
                                         0,
                                         size,
                                         ptr,
                                         numEventsInWaitList,
                                         eventWaitList,
                                         &event);

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_EQ(19u, pEvent->taskLevel);
    EXPECT_EQ(19u, pCmdOOQ->taskLevel);

    pEvent->release();
}