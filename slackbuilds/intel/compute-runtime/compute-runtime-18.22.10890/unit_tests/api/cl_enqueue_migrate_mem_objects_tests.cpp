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
#include "runtime/command_queue/command_queue.h"

using namespace OCLRT;

typedef api_tests clEnqueueMigrateMemObjectsTests;

TEST_F(clEnqueueMigrateMemObjectsTests, NullCommandQueue_returnsError) {
    unsigned int bufferSize = 16;
    auto pHostMem = new unsigned char[bufferSize];
    memset(pHostMem, 0xaa, bufferSize);

    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    auto buffer = clCreateBuffer(
        pContext,
        flags,
        bufferSize,
        pHostMem,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    cl_event eventReturned = nullptr;
    auto Result = clEnqueueMigrateMemObjects(
        nullptr,
        1,
        &buffer,
        CL_MIGRATE_MEM_OBJECT_HOST,
        0,
        nullptr,
        &eventReturned);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, Result);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete[] pHostMem;

    clReleaseEvent(eventReturned);
}

TEST_F(clEnqueueMigrateMemObjectsTests, ValidInputsExpectSuccess) {
    unsigned int bufferSize = 16;
    auto pHostMem = new unsigned char[bufferSize];
    memset(pHostMem, 0xaa, bufferSize);

    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    auto buffer = clCreateBuffer(
        pContext,
        flags,
        bufferSize,
        pHostMem,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    cl_event eventReturned = nullptr;
    auto Result = clEnqueueMigrateMemObjects(
        pCommandQueue,
        1,
        &buffer,
        CL_MIGRATE_MEM_OBJECT_HOST,
        0,
        nullptr,
        &eventReturned);
    EXPECT_EQ(CL_SUCCESS, Result);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete[] pHostMem;

    clReleaseEvent(eventReturned);
}

TEST_F(clEnqueueMigrateMemObjectsTests, NullMemObjectsReturnsInvalidValue) {

    cl_event eventReturned = nullptr;
    auto Result = clEnqueueMigrateMemObjects(
        pCommandQueue,
        1,
        nullptr,
        CL_MIGRATE_MEM_OBJECT_HOST,
        0,
        nullptr,
        &eventReturned);
    EXPECT_EQ(CL_INVALID_VALUE, Result);
}

TEST_F(clEnqueueMigrateMemObjectsTests, ZeroMemObjectsReturnsInvalidValue) {

    cl_event eventReturned = nullptr;
    auto Result = clEnqueueMigrateMemObjects(
        pCommandQueue,
        0,
        nullptr,
        CL_MIGRATE_MEM_OBJECT_HOST,
        0,
        nullptr,
        &eventReturned);
    EXPECT_EQ(CL_INVALID_VALUE, Result);
}

TEST_F(clEnqueueMigrateMemObjectsTests, NonZeroEventsNullWaitlistReturnsInvalidWaitlist) {

    cl_event eventReturned = nullptr;
    auto Result = clEnqueueMigrateMemObjects(
        pCommandQueue,
        0,
        nullptr,
        CL_MIGRATE_MEM_OBJECT_HOST,
        2,
        nullptr,
        &eventReturned);
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, Result);
}

TEST_F(clEnqueueMigrateMemObjectsTests, ZeroEventsNonNullWaitlistReturnsInvalidWaitlist) {

    cl_event eventReturned = nullptr;
    Event event(pCommandQueue, CL_COMMAND_MIGRATE_MEM_OBJECTS, 0, 0);

    auto Result = clEnqueueMigrateMemObjects(
        pCommandQueue,
        0,
        nullptr,
        CL_MIGRATE_MEM_OBJECT_HOST,
        0,
        (cl_event *)&event,
        &eventReturned);
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, Result);
}

TEST_F(clEnqueueMigrateMemObjectsTests, ValidFlagsReturnsSuccess) {
    unsigned int bufferSize = 16;
    auto pHostMem = new unsigned char[bufferSize];
    memset(pHostMem, 0xaa, bufferSize);

    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    auto buffer = clCreateBuffer(
        pContext,
        flags,
        bufferSize,
        pHostMem,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    cl_mem_migration_flags validFlags[] = {0, CL_MIGRATE_MEM_OBJECT_HOST, CL_MIGRATE_MEM_OBJECT_CONTENT_UNDEFINED, CL_MIGRATE_MEM_OBJECT_HOST | CL_MIGRATE_MEM_OBJECT_CONTENT_UNDEFINED};

    for (unsigned int i = 0; i < sizeof(validFlags) / sizeof(cl_mem_migration_flags); i++) {
        cl_event eventReturned = nullptr;
        auto Result = clEnqueueMigrateMemObjects(
            pCommandQueue,
            1,
            &buffer,
            validFlags[i],
            0,
            nullptr,
            &eventReturned);
        EXPECT_EQ(CL_SUCCESS, Result);
        clReleaseEvent(eventReturned);
    }

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete[] pHostMem;
}

TEST_F(clEnqueueMigrateMemObjectsTests, InvalidFlagsReturnsError) {
    unsigned int bufferSize = 16;
    auto pHostMem = new unsigned char[bufferSize];
    memset(pHostMem, 0xaa, bufferSize);

    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    auto buffer = clCreateBuffer(
        pContext,
        flags,
        bufferSize,
        pHostMem,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    cl_mem_migration_flags validFlags[] = {(cl_mem_migration_flags)0xffffffff, CL_MIGRATE_MEM_OBJECT_HOST | (1 << 10), CL_MIGRATE_MEM_OBJECT_CONTENT_UNDEFINED | (1 << 10), (cl_mem_migration_flags)12345};

    for (unsigned int i = 0; i < sizeof(validFlags) / sizeof(cl_mem_migration_flags); i++) {
        cl_event eventReturned = nullptr;
        auto Result = clEnqueueMigrateMemObjects(
            pCommandQueue,
            1,
            &buffer,
            validFlags[i],
            0,
            nullptr,
            &eventReturned);
        EXPECT_EQ(CL_INVALID_VALUE, Result);
        clReleaseEvent(eventReturned);
    }

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete[] pHostMem;
}
