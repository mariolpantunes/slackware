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

#include "runtime/api/cl_types.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/device_queue/device_queue.h"
#include "runtime/helpers/base_object.h"
#include "runtime/context/context.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/platform/platform.h"
#include "runtime/accelerators/intel_accelerator.h"
#include "runtime/program/program.h"
#include "runtime/sampler/sampler.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "runtime/api/api.h"

namespace OCLRT {
typedef struct _cl_object_for_test2 *cl_object_for_test2;

struct _cl_object_for_test2 : public ClDispatch {
};

template <>
struct OpenCLObjectMapper<_cl_object_for_test2> {
    typedef struct ObjectForTest2 DerivedType;
};

template <>
struct OpenCLObjectMapper<ObjectForTest2> {
    typedef _cl_object_for_test2 BaseType;
};

struct ObjectForTest2 : public OCLRT::BaseObject<_cl_object_for_test2> {
    static const cl_ulong objectMagic = 0x13650a12b79ce4dfLL;
};

template <typename TypeParam>
struct BaseObjectTests : public ::testing::Test {
};

template <typename BaseType>
class MockObject : public BaseType {
  public:
    void setInvalidMagic() {
        validMagic = this->magic;
        this->magic = 0x0101010101010101LL;
    }
    void setValidMagic() {
        this->magic = validMagic;
    }

    bool isObjectValid() const {
        return this->isValid();
    }

    cl_ulong validMagic;
};

template <>
class MockObject<Buffer> : public Buffer {
  public:
    void setInvalidMagic() {
        validMagic = this->magic;
        this->magic = 0x0101010101010101LL;
    }
    void setValidMagic() {
        this->magic = validMagic;
    }

    bool isObjectValid() const {
        return this->isValid();
    }

    void setArgStateful(void *memory) override {
    }

    cl_ulong validMagic;
};

typedef ::testing::Types<
    Platform,
    IntelAccelerator,
    //Context,
    //Program,
    //Kernel,
    //Sampler
    //others...
    CommandQueue,
    DeviceQueue>
    BaseObjectTypes;

typedef ::testing::Types<
    Platform,
    IntelAccelerator,
    Context,
    Program,
    Buffer,
    CommandQueue,
    DeviceQueue>
    BaseObjectTypesForCastInvalidMagicTest;

TYPED_TEST_CASE(BaseObjectTests, BaseObjectTypes);

// "typedef" BaseObjectTests template to use with different TypeParams for testing
template <typename T>
using BaseObjectWithDefaultCtorTests = BaseObjectTests<T>;
TYPED_TEST_CASE(BaseObjectWithDefaultCtorTests, BaseObjectTypesForCastInvalidMagicTest);

TYPED_TEST(BaseObjectWithDefaultCtorTests, castToObjectWithInvalidMagicReturnsNullptr) {
    MockObject<TypeParam> *object = new MockObject<TypeParam>;
    EXPECT_TRUE(object->isObjectValid());
    object->setInvalidMagic();
    EXPECT_FALSE(object->isObjectValid());

    auto objectCasted = castToObject<TypeParam>(object);
    EXPECT_EQ(nullptr, objectCasted);

    object->setValidMagic();
    delete object;
}

TYPED_TEST(BaseObjectTests, retain) {
    TypeParam *object = new TypeParam;
    object->retain();
    EXPECT_EQ(2, object->getReference());

    object->release();
    EXPECT_EQ(1, object->getReference());

    object->release();

    // MemoryLeakListener will detect a leak
    // if release doesn't delete memory.
}

TYPED_TEST(BaseObjectTests, castToObjectFromNullptr) {
    typename TypeParam::BaseType *handle = nullptr;
    auto object = castToObject<TypeParam>(handle);
    EXPECT_EQ(nullptr, object);
}

TYPED_TEST(BaseObjectTests, castToFromBaseTypeYieldsDerivedType) {
    TypeParam object;
    typename TypeParam::BaseType *baseObject = &object;

    auto objectNew = castToObject<TypeParam>(baseObject);
    EXPECT_EQ(&object, objectNew);
}

TYPED_TEST(BaseObjectTests, castToSameTypeReturnsSameObject) {
    TypeParam object;

    auto objectNew = castToObject<TypeParam>(&object);
    EXPECT_EQ(&object, objectNew);
}

TYPED_TEST(BaseObjectTests, castToDifferentTypeReturnsNullPtr) {
    TypeParam object;
    typename TypeParam::BaseType *baseObject = &object;

    auto notOriginalType = reinterpret_cast<ObjectForTest2::BaseType *>(baseObject);
    auto invalidObject = castToObject<ObjectForTest2>(notOriginalType);
    EXPECT_EQ(nullptr, invalidObject);
}

TYPED_TEST(BaseObjectTests, commonRuntimeExpectsDispatchTableAtFirstPointerInObject) {
    TypeParam objectDrv;

    // Automatic downcasting to _cl_type *.
    typename TypeParam::BaseType *objectCL = &objectDrv;
    sharingFactory.fillGlobalDispatchTable();

    // Common runtime casts to generic type assuming
    // the dispatch table is the first ptr in the structure
    auto genericObject = reinterpret_cast<ClDispatch *>(objectCL);
    EXPECT_EQ(globalDispatchTable.icdDispatch, genericObject->dispatch.icdDispatch);
    EXPECT_EQ(globalDispatchTable.crtDispatch, genericObject->dispatch.crtDispatch);

    EXPECT_EQ(reinterpret_cast<void *>(clGetKernelArgInfo), genericObject->dispatch.crtDispatch->clGetKernelArgInfo);

    EXPECT_EQ(reinterpret_cast<void *>(clGetImageParamsINTEL), genericObject->dispatch.crtDispatch->clGetImageParamsINTEL);

    EXPECT_EQ(reinterpret_cast<void *>(clCreateAcceleratorINTEL), genericObject->dispatch.crtDispatch->clCreateAcceleratorINTEL);
    EXPECT_EQ(reinterpret_cast<void *>(clGetAcceleratorInfoINTEL), genericObject->dispatch.crtDispatch->clGetAcceleratorInfoINTEL);
    EXPECT_EQ(reinterpret_cast<void *>(clRetainAcceleratorINTEL), genericObject->dispatch.crtDispatch->clRetainAcceleratorINTEL);
    EXPECT_EQ(reinterpret_cast<void *>(clReleaseAcceleratorINTEL), genericObject->dispatch.crtDispatch->clReleaseAcceleratorINTEL);

    EXPECT_EQ(reinterpret_cast<void *>(clCreatePerfCountersCommandQueueINTEL),
              genericObject->dispatch.crtDispatch->clCreatePerfCountersCommandQueueINTEL);

    EXPECT_EQ(reinterpret_cast<void *>(clSetPerformanceConfigurationINTEL),
              genericObject->dispatch.crtDispatch->clSetPerformanceConfigurationINTEL);

    // Check empty placeholder dispatch table entries are null
    EXPECT_EQ(nullptr, genericObject->dispatch.crtDispatch->placeholder12);
    EXPECT_EQ(nullptr, genericObject->dispatch.crtDispatch->placeholder13);

    EXPECT_EQ(nullptr, genericObject->dispatch.crtDispatch->placeholder18);
    EXPECT_EQ(nullptr, genericObject->dispatch.crtDispatch->placeholder19);
    EXPECT_EQ(nullptr, genericObject->dispatch.crtDispatch->placeholder20);
    EXPECT_EQ(nullptr, genericObject->dispatch.crtDispatch->placeholder21);
}

TEST(BaseObjectTests, commonRuntimeSetsSharedContextFlag) {
    MockContext newContext;

    //cast to cl_context
    cl_context clContext = &newContext;
    EXPECT_FALSE(newContext.isSharedContext);

    clContext->isSharedContext = true;

    EXPECT_TRUE(newContext.isSharedContext);
}

TYPED_TEST(BaseObjectTests, WhenAlreadyOwnedByThreadOnTakeOrReleaseOwnershipUsesRecursiveOwnageCounter) {
    TypeParam obj;
    EXPECT_FALSE(obj.hasOwnership());

    obj.takeOwnership(false);
    EXPECT_TRUE(obj.hasOwnership());

    obj.takeOwnership(false);
    EXPECT_TRUE(obj.hasOwnership());

    obj.takeOwnership(false);
    EXPECT_TRUE(obj.hasOwnership());

    obj.releaseOwnership();
    EXPECT_TRUE(obj.hasOwnership());

    obj.releaseOwnership();
    EXPECT_TRUE(obj.hasOwnership());

    obj.releaseOwnership();
    EXPECT_FALSE(obj.hasOwnership());
}

TEST(CastToBuffer, fromMemObj) {
    MockContext context;
    auto buffer = BufferHelper<>::create(&context);
    MemObj *memObj = buffer;
    cl_mem clObj = buffer;

    EXPECT_EQ(buffer, castToObject<Buffer>(clObj));
    EXPECT_EQ(memObj, castToObject<MemObj>(clObj));
    EXPECT_EQ(nullptr, castToObject<Image>(clObj));

    buffer->release();
}

TEST(CastToImage, fromMemObj) {
    MockContext context;
    auto image = Image2dHelper<>::create(&context);
    MemObj *memObj = image;
    cl_mem clObj = image;

    EXPECT_EQ(image, castToObject<Image>(clObj));
    EXPECT_EQ(memObj, castToObject<MemObj>(clObj));
    EXPECT_EQ(nullptr, castToObject<Buffer>(clObj));

    image->release();
}

extern std::thread::id tempThreadID;
class MockBuffer : public MockBufferStorage, public Buffer {
  public:
    MockBuffer() : MockBufferStorage(), Buffer(nullptr, CL_MEM_USE_HOST_PTR, sizeof(data), &data, &data, &mockGfxAllocation, true, false, false) {
    }

    void setArgStateful(void *memory) override {
    }

    void setFakeOwnership() {
        this->owner = tempThreadID;
    }
};

TEST(BaseObjectTest, takeOwnership) {
    MockBuffer buffer;
    EXPECT_FALSE(buffer.hasOwnership());
    buffer.takeOwnership(false);
    EXPECT_TRUE(buffer.hasOwnership());
    buffer.takeOwnership(false);
    EXPECT_TRUE(buffer.hasOwnership());
    buffer.releaseOwnership();
    EXPECT_TRUE(buffer.hasOwnership());
    buffer.releaseOwnership();
    EXPECT_FALSE(buffer.hasOwnership());
    buffer.setFakeOwnership();
    EXPECT_FALSE(buffer.hasOwnership());
    buffer.takeOwnership(false);
    EXPECT_FALSE(buffer.hasOwnership());
}

TEST(BaseObjectTest, takeOwnershipWrapper) {
    MockBuffer buffer;
    {
        TakeOwnershipWrapper<Buffer> bufferOwnership(buffer, false);
        EXPECT_FALSE(buffer.hasOwnership());
    }
    {
        TakeOwnershipWrapper<Buffer> bufferOwnership(buffer, true);
        EXPECT_TRUE(buffer.hasOwnership());
        bufferOwnership.unlock();
        EXPECT_FALSE(buffer.hasOwnership());
    }
}

TYPED_TEST(BaseObjectTests, getCond) {
    TypeParam *object = new TypeParam;

    EXPECT_EQ(0U, object->getCond().peekNumWaiters());
    object->release();
}

TYPED_TEST(BaseObjectTests, convertToInternalObject) {
    class ObjectForTest : public OCLRT::MemObj {
      public:
        ObjectForTest() : MemObj(nullptr, 0, 0, 0u, nullptr, nullptr, nullptr, false, false, false) {
        }

        void convertToInternalObject(void) {
            OCLRT::BaseObject<_cl_mem>::convertToInternalObject();
        }
    };
    ObjectForTest *object = new ObjectForTest;
    EXPECT_EQ(1, object->getRefApiCount());
    EXPECT_EQ(1, object->getRefInternalCount());
    object->convertToInternalObject();
    EXPECT_EQ(0, object->getRefApiCount());
    EXPECT_EQ(1, object->getRefInternalCount());
    object->decRefInternal();
}

TYPED_TEST(BaseObjectTests, castToObjectOrAbortFromNullptrAbort) {
    EXPECT_ANY_THROW(castToObjectOrAbort<TypeParam>(nullptr));
}

TYPED_TEST(BaseObjectTests, castToObjectOrAbortFromBaseTypeYieldsDerivedType) {
    TypeParam object;
    typename TypeParam::BaseType *baseObject = &object;

    auto objectNew = castToObjectOrAbort<TypeParam>(baseObject);
    EXPECT_EQ(&object, objectNew);
}

TYPED_TEST(BaseObjectTests, castToOrAbortDifferentTypeAborts) {
    TypeParam object;
    typename TypeParam::BaseType *baseObject = &object;

    auto notOriginalType = reinterpret_cast<ObjectForTest2::BaseType *>(baseObject);
    EXPECT_ANY_THROW(castToObjectOrAbort<ObjectForTest2>(notOriginalType));
}
} // namespace OCLRT
