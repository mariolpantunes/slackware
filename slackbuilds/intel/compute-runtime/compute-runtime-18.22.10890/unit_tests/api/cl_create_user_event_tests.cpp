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

#include "cl_api_tests.h"
#include "runtime/context/context.h"

using namespace OCLRT;

typedef api_tests clCreateUserEventTests;

namespace ULT {

TEST_F(clCreateUserEventTests, createUserEventReturnsSuccess) {
    auto userEvent = clCreateUserEvent(
        pContext,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, userEvent);

    retVal = clReleaseEvent(userEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateUserEventTests, nullContext) {
    auto userEvent = clCreateUserEvent(
        nullptr,
        &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, userEvent);
}

TEST_F(clCreateUserEventTests, getEventInfoForUserEventsCmdQueue) {
    auto userEvent = clCreateUserEvent(
        pContext,
        &retVal);

    size_t retSize;
    retVal = clGetEventInfo(userEvent, CL_EVENT_COMMAND_QUEUE, 0, nullptr, &retSize);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_command_queue), retSize);
    auto cmdQueue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(0xdeadbeaf));
    retVal = clGetEventInfo(userEvent, CL_EVENT_COMMAND_QUEUE, retSize, &cmdQueue, 0);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, cmdQueue);
    retVal = clGetEventInfo(userEvent, CL_EVENT_COMMAND_TYPE, 0, nullptr, &retSize);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_event_info), retSize);
    auto cmd_type = CL_COMMAND_SVM_UNMAP;
    retVal = clGetEventInfo(userEvent, CL_EVENT_COMMAND_TYPE, retSize, &cmd_type, 0);
    EXPECT_EQ(CL_COMMAND_USER, cmd_type);

    retVal = clReleaseEvent(userEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateUserEventTests, checkSetUserEventStatusReturnSuccessAndUpdatesStatus) {
    auto userEvent = clCreateUserEvent(
        pContext,
        &retVal);

    retVal = clSetUserEventStatus(userEvent, CL_COMPLETE);
    ASSERT_EQ(CL_SUCCESS, retVal);

    size_t retSize;
    retVal = clGetEventInfo(userEvent, CL_EVENT_COMMAND_EXECUTION_STATUS, 0, nullptr, &retSize);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_int), retSize);
    auto status = CL_SUBMITTED;
    retVal = clGetEventInfo(userEvent, CL_EVENT_COMMAND_EXECUTION_STATUS, retSize, &status, 0);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(CL_COMPLETE, status);

    retVal = clReleaseEvent(userEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateUserEventTests, userEventHasValidContext) {
    auto userEvent = clCreateUserEvent(
        pContext,
        &retVal);

    size_t retSize;
    retVal = clGetEventInfo(userEvent, CL_EVENT_CONTEXT, 0, nullptr, &retSize);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_context), retSize);
    cl_context oclContext;
    retVal = clGetEventInfo(userEvent, CL_EVENT_CONTEXT, retSize, &oclContext, 0);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(oclContext, pContext);

    retVal = clReleaseEvent(userEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateUserEventTests, WaitForUserEventThatIsCLCompleteReturnsImmidietaly) {
    auto userEvent = clCreateUserEvent(
        pContext,
        &retVal);

    retVal = clSetUserEventStatus(userEvent, CL_COMPLETE);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clWaitForEvents(1, &userEvent);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseEvent(userEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateUserEventTests, WaitForUserEventThatIsTerminatedReturnsImmidietaly) {
    auto userEvent = clCreateUserEvent(
        pContext,
        &retVal);

    retVal = clSetUserEventStatus(userEvent, -1);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clWaitForEvents(1, &userEvent);
    ASSERT_EQ(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST, retVal);

    retVal = clReleaseEvent(userEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT
