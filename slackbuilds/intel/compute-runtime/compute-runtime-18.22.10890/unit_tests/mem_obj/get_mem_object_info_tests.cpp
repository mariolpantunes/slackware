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

#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/aligned_memory.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "runtime/helpers/options.h"
#include "unit_tests/mocks/mock_context.h"
#include "gtest/gtest.h"
#include <memory>

using namespace OCLRT;

class GetMemObjectInfo : public ::testing::Test, public PlatformFixture, public DeviceFixture {
    using PlatformFixture::SetUp;
    using DeviceFixture::SetUp;

  public:
    void SetUp() override {
        PlatformFixture::SetUp(numPlatformDevices, platformDevices);
        DeviceFixture::SetUp();
        BufferDefaults::context = new MockContext;
    }

    void TearDown() override {
        delete BufferDefaults::context;
        PlatformFixture::TearDown();
        DeviceFixture::TearDown();
    }
};

TEST_F(GetMemObjectInfo, InvalidFlags_returnsError) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    size_t sizeReturned = 0;
    auto retVal = buffer->getMemObjectInfo(
        0,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(GetMemObjectInfo, MEM_TYPE) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    size_t sizeReturned = 0;
    auto retVal = buffer->getMemObjectInfo(
        CL_MEM_TYPE,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_mem_object_type), sizeReturned);

    cl_mem_object_type object_type = 0;
    retVal = buffer->getMemObjectInfo(
        CL_MEM_TYPE,
        sizeof(cl_mem_object_type),
        &object_type,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(static_cast<cl_mem_object_type>(CL_MEM_OBJECT_BUFFER), object_type);
}

TEST_F(GetMemObjectInfo, MEM_FLAGS) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    size_t sizeReturned = 0;
    cl_mem_flags mem_flags = 0;
    auto retVal = buffer->getMemObjectInfo(
        CL_MEM_FLAGS,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(mem_flags), sizeReturned);

    retVal = buffer->getMemObjectInfo(
        CL_MEM_FLAGS,
        sizeof(mem_flags),
        &mem_flags,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_READ_WRITE), mem_flags);
}

TEST_F(GetMemObjectInfo, MEM_SIZE) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    size_t sizeReturned = 0;
    size_t mem_size = 0;
    auto retVal = buffer->getMemObjectInfo(
        CL_MEM_SIZE,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(mem_size), sizeReturned);

    retVal = buffer->getMemObjectInfo(
        CL_MEM_SIZE,
        sizeof(mem_size),
        &mem_size,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(buffer->getSize(), mem_size);
}

TEST_F(GetMemObjectInfo, MEM_HOST_PTR) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    size_t sizeReturned = 0;
    void *host_ptr = nullptr;
    auto retVal = buffer->getMemObjectInfo(
        CL_MEM_HOST_PTR,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(host_ptr), sizeReturned);

    retVal = buffer->getMemObjectInfo(
        CL_MEM_HOST_PTR,
        sizeof(host_ptr),
        &host_ptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(buffer->getHostPtr(), host_ptr);
}

TEST_F(GetMemObjectInfo, MEM_CONTEXT) {
    MockContext context;
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create(&context));

    size_t sizeReturned = 0;
    cl_context contextReturned = nullptr;
    auto retVal = buffer->getMemObjectInfo(
        CL_MEM_CONTEXT,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(contextReturned), sizeReturned);

    retVal = buffer->getMemObjectInfo(
        CL_MEM_CONTEXT,
        sizeof(contextReturned),
        &contextReturned,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(&context, contextReturned);
}

TEST_F(GetMemObjectInfo, MEM_USES_SVM_POINTER_FALSE) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<BufferUseHostPtr<>>::create());

    size_t sizeReturned = 0;
    cl_bool usesSVMPointer = false;
    auto retVal = buffer->getMemObjectInfo(
        CL_MEM_USES_SVM_POINTER,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(usesSVMPointer), sizeReturned);

    retVal = buffer->getMemObjectInfo(
        CL_MEM_USES_SVM_POINTER,
        sizeof(usesSVMPointer),
        &usesSVMPointer,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(static_cast<cl_bool>(CL_FALSE), usesSVMPointer);
}

TEST_F(GetMemObjectInfo, MEM_USES_SVM_POINTER_TRUE) {
    const DeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        auto hostPtr = clSVMAlloc(BufferDefaults::context, CL_MEM_READ_WRITE, BufferUseHostPtr<>::sizeInBytes, 64);
        ASSERT_NE(nullptr, hostPtr);
        cl_int retVal;

        auto buffer = Buffer::create(
            BufferDefaults::context,
            CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
            BufferUseHostPtr<>::sizeInBytes,
            hostPtr,
            retVal);

        size_t sizeReturned = 0;
        cl_bool usesSVMPointer = false;

        retVal = buffer->getMemObjectInfo(
            CL_MEM_USES_SVM_POINTER,
            0,
            nullptr,
            &sizeReturned);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(sizeof(usesSVMPointer), sizeReturned);

        retVal = buffer->getMemObjectInfo(
            CL_MEM_USES_SVM_POINTER,
            sizeof(usesSVMPointer),
            &usesSVMPointer,
            nullptr);

        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), usesSVMPointer);

        delete buffer;
        clSVMFree(BufferDefaults::context, hostPtr);
    }
}

TEST_F(GetMemObjectInfo, MEM_OFFSET) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    size_t sizeReturned = 0;
    size_t offset = false;
    auto retVal = buffer->getMemObjectInfo(
        CL_MEM_OFFSET,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(offset), sizeReturned);

    retVal = buffer->getMemObjectInfo(
        CL_MEM_OFFSET,
        sizeof(offset),
        &offset,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, offset);
}

TEST_F(GetMemObjectInfo, MEM_ASSOCIATED_MEMOBJECT) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    size_t sizeReturned = 0;
    cl_mem object = nullptr;
    auto retVal = buffer->getMemObjectInfo(
        CL_MEM_ASSOCIATED_MEMOBJECT,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(object), sizeReturned);

    retVal = buffer->getMemObjectInfo(
        CL_MEM_ASSOCIATED_MEMOBJECT,
        sizeof(object),
        &object,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, object);
}

TEST_F(GetMemObjectInfo, MEM_MAP_COUNT) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    size_t sizeReturned = 0;
    cl_uint mapCount = static_cast<uint32_t>(-1);
    auto retVal = buffer->getMemObjectInfo(
        CL_MEM_MAP_COUNT,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(mapCount), sizeReturned);

    retVal = buffer->getMemObjectInfo(
        CL_MEM_MAP_COUNT,
        sizeof(mapCount),
        &mapCount,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(mapCount), sizeReturned);
}

TEST_F(GetMemObjectInfo, MEM_REFERENCE_COUNT) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    size_t sizeReturned = 0;
    cl_uint refCount = static_cast<uint32_t>(-1);
    auto retVal = buffer->getMemObjectInfo(
        CL_MEM_REFERENCE_COUNT,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(refCount), sizeReturned);

    retVal = buffer->getMemObjectInfo(
        CL_MEM_REFERENCE_COUNT,
        sizeof(refCount),
        &refCount,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(refCount), sizeReturned);
}
