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

#include "CL/cl.h"
#include "runtime/kernel/kernel.h"
#include "runtime/mem_obj/buffer.h"
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"
#include "gtest/gtest.h"
#include "test.h"

#include <memory>

using namespace OCLRT;

class KernelArgSvmFixture_ : public ContextFixture, public DeviceFixture {

    using ContextFixture::SetUp;

  public:
    KernelArgSvmFixture_() {
    }

  protected:
    void SetUp() {
        DeviceFixture::SetUp();
        cl_device_id device = pDevice;
        ContextFixture::SetUp(1, &device);

        // define kernel info
        pKernelInfo = KernelInfo::create();

        // setup kernel arg offsets
        KernelArgPatchInfo kernelArgPatchInfo;

        kernelHeader.SurfaceStateHeapSize = sizeof(pSshLocal);
        pKernelInfo->heapInfo.pSsh = pSshLocal;
        pKernelInfo->heapInfo.pKernelHeader = &kernelHeader;
        pKernelInfo->usesSsh = true;
        pKernelInfo->requiresSshForBuffers = true;

        pKernelInfo->kernelArgInfo.resize(1);
        pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);

        pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset = 0x30;
        pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].size = (uint32_t)sizeof(void *);

        pProgram = new MockProgram(pContext, false);

        pKernel = new MockKernel(pProgram, *pKernelInfo, *pDevice);
        ASSERT_EQ(CL_SUCCESS, pKernel->initialize());
        pKernel->setCrossThreadData(pCrossThreadData, sizeof(pCrossThreadData));
    }

    void TearDown() override {
        delete pKernelInfo;
        delete pKernel;
        delete pProgram;
        ContextFixture::TearDown();
        DeviceFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    MockProgram *pProgram = nullptr;
    MockKernel *pKernel = nullptr;
    KernelInfo *pKernelInfo = nullptr;
    SKernelBinaryHeaderCommon kernelHeader;
    char pSshLocal[64];
    char pCrossThreadData[64];
};

typedef Test<KernelArgSvmFixture_> KernelArgSvmTest;

TEST_F(KernelArgSvmTest, SetKernelArgValidSvmPtr) {
    char *svmPtr = new char[256];

    auto retVal = pKernel->setArgSvm(0, 256, svmPtr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pKernelArg = (void **)(pKernel->getCrossThreadData() +
                                pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);
    EXPECT_EQ(svmPtr, *pKernelArg);

    delete[] svmPtr;
}

TEST_F(KernelArgSvmTest, SetKernelArgValidSvmPtrStateless) {
    char *svmPtr = new char[256];

    pKernelInfo->usesSsh = false;
    pKernelInfo->requiresSshForBuffers = false;

    auto retVal = pKernel->setArgSvm(0, 256, svmPtr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(0u, pKernel->getSurfaceStateHeapSize());

    delete[] svmPtr;
}

HWTEST_F(KernelArgSvmTest, SetKernelArgValidSvmPtrStateful) {
    char *svmPtr = new char[256];

    pKernelInfo->usesSsh = true;
    pKernelInfo->requiresSshForBuffers = true;

    auto retVal = pKernel->setArgSvm(0, 256, svmPtr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(0u, pKernel->getSurfaceStateHeapSize());

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->kernelArgInfo[0].offsetHeap));

    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());
    EXPECT_EQ(svmPtr, surfaceAddress);

    delete[] svmPtr;
}

TEST_F(KernelArgSvmTest, SetKernelArgValidSvmAlloc) {
    char *svmPtr = new char[256];

    GraphicsAllocation svmAlloc(svmPtr, 256);

    auto retVal = pKernel->setArgSvmAlloc(0, svmPtr, &svmAlloc);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pKernelArg = (void **)(pKernel->getCrossThreadData() +
                                pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);
    EXPECT_EQ(svmPtr, *pKernelArg);

    delete[] svmPtr;
}

TEST_F(KernelArgSvmTest, SetKernelArgValidSvmAllocStateless) {
    char *svmPtr = new char[256];

    GraphicsAllocation svmAlloc(svmPtr, 256);

    pKernelInfo->usesSsh = false;
    pKernelInfo->requiresSshForBuffers = false;

    auto retVal = pKernel->setArgSvmAlloc(0, svmPtr, &svmAlloc);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(0u, pKernel->getSurfaceStateHeapSize());

    delete[] svmPtr;
}

HWTEST_F(KernelArgSvmTest, SetKernelArgValidSvmAllocStateful) {
    char *svmPtr = new char[256];

    GraphicsAllocation svmAlloc(svmPtr, 256);

    pKernelInfo->usesSsh = true;
    pKernelInfo->requiresSshForBuffers = true;

    auto retVal = pKernel->setArgSvmAlloc(0, svmPtr, &svmAlloc);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(0u, pKernel->getSurfaceStateHeapSize());

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->kernelArgInfo[0].offsetHeap));

    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());
    EXPECT_EQ(svmPtr, surfaceAddress);

    delete[] svmPtr;
}

HWTEST_F(KernelArgSvmTest, givenOffsetedSvmPointerWhenSetArgSvmAllocIsCalledThenProperSvmAddressIsPatched) {
    std::unique_ptr<char[]> svmPtr(new char[256]);

    auto offsetedPtr = svmPtr.get() + 4;

    GraphicsAllocation svmAlloc(svmPtr.get(), 256);
    pKernelInfo->usesSsh = true;
    pKernelInfo->requiresSshForBuffers = true;

    pKernel->setArgSvmAlloc(0, offsetedPtr, &svmAlloc);

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->kernelArgInfo[0].offsetHeap));

    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());
    EXPECT_EQ(offsetedPtr, surfaceAddress);
}

TEST_F(KernelArgSvmTest, SetKernelArgImmediateInvalidArgValue) {
    auto retVal = pKernel->setArgImmediate(0, 256, nullptr);
    EXPECT_EQ(CL_INVALID_ARG_VALUE, retVal);
}

HWTEST_F(KernelArgSvmTest, PatchWithImplicitSurface) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    constexpr size_t rendSurfSize = sizeof(RENDER_SURFACE_STATE);

    std::vector<char> svmPtr;
    svmPtr.resize(256);

    pKernel->setCrossThreadData(nullptr, sizeof(void *));
    pKernel->setSshLocal(nullptr, rendSurfSize);
    pKernelInfo->requiresSshForBuffers = true;
    pKernelInfo->usesSsh = true;
    {
        GraphicsAllocation svmAlloc(svmPtr.data(), svmPtr.size());

        SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization patch;
        memset(&patch, 0, sizeof(patch));
        patch.DataParamOffset = 0;
        patch.DataParamSize = sizeof(void *);
        patch.SurfaceStateHeapOffset = 0;

        constexpr size_t patchOffset = 16;
        void *ptrToPatch = svmPtr.data() + patchOffset;
        ASSERT_GE(pKernel->getCrossThreadDataSize(), sizeof(void *));
        *reinterpret_cast<void **>(pKernel->getCrossThreadData()) = 0U;

        ASSERT_GE(pKernel->getSurfaceStateHeapSize(), rendSurfSize);
        RENDER_SURFACE_STATE *surfState = reinterpret_cast<RENDER_SURFACE_STATE *>(pKernel->getSurfaceStateHeap());
        memset(surfState, 0, rendSurfSize);

        pKernel->patchWithImplicitSurface(ptrToPatch, svmAlloc, patch);

        // verify cross thread data was properly patched
        EXPECT_EQ(ptrToPatch, *reinterpret_cast<void **>(pKernel->getCrossThreadData()));

        // create surface state for comparison
        RENDER_SURFACE_STATE expectedSurfaceState;
        memset(&expectedSurfaceState, 0, rendSurfSize);
        {
            void *addressToPatch = svmAlloc.getUnderlyingBuffer();
            size_t sizeToPatch = svmAlloc.getUnderlyingBufferSize();
            Buffer::setSurfaceState(pContext, &expectedSurfaceState, sizeToPatch, addressToPatch, &svmAlloc);
        }

        // verify ssh was properly patched
        EXPECT_EQ(0, memcmp(&expectedSurfaceState, surfState, rendSurfSize));

        // when cross thread and ssh data is not available then should not do anything
        pKernel->setCrossThreadData(nullptr, 0);
        pKernel->setSshLocal(nullptr, 0);
        pKernel->patchWithImplicitSurface(ptrToPatch, svmAlloc, patch);
    }
}

TEST_F(KernelArgSvmTest, patchBufferOffset) {
    std::vector<char> svmPtr;
    svmPtr.resize(256);

    pKernel->setCrossThreadData(nullptr, sizeof(uint32_t));
    {
        constexpr uint32_t initVal = 7U;
        constexpr uint32_t svmOffset = 13U;

        GraphicsAllocation svmAlloc(svmPtr.data(), 256);
        uint32_t *expectedPatchPtr = reinterpret_cast<uint32_t *>(pKernel->getCrossThreadData());

        KernelArgInfo kai;
        void *returnedPtr = nullptr;

        kai.offsetBufferOffset = static_cast<uint32_t>(-1);
        *expectedPatchPtr = initVal;
        returnedPtr = pKernel->patchBufferOffset(kai, svmPtr.data(), &svmAlloc);
        EXPECT_EQ(svmPtr.data(), returnedPtr);
        EXPECT_EQ(initVal, *expectedPatchPtr);

        kai.offsetBufferOffset = static_cast<uint32_t>(-1);
        *expectedPatchPtr = initVal;
        returnedPtr = pKernel->patchBufferOffset(kai, svmPtr.data(), nullptr);
        EXPECT_EQ(svmPtr.data(), returnedPtr);
        EXPECT_EQ(initVal, *expectedPatchPtr);

        kai.offsetBufferOffset = 0U;
        *expectedPatchPtr = initVal;
        returnedPtr = pKernel->patchBufferOffset(kai, svmPtr.data(), &svmAlloc);
        EXPECT_EQ(svmPtr.data(), returnedPtr);
        EXPECT_EQ(0U, *expectedPatchPtr);

        kai.offsetBufferOffset = 0U;
        *expectedPatchPtr = initVal;
        returnedPtr = pKernel->patchBufferOffset(kai, svmPtr.data() + svmOffset, nullptr);
        void *expectedPtr = alignDown(svmPtr.data() + svmOffset, 4);
        // expecting to see DWORD alignment restriction in offset
        uint32_t expectedOffset = static_cast<uint32_t>(ptrDiff(svmPtr.data() + svmOffset, expectedPtr));
        EXPECT_EQ(expectedPtr, returnedPtr);
        EXPECT_EQ(expectedOffset, *expectedPatchPtr);

        kai.offsetBufferOffset = 0U;
        *expectedPatchPtr = initVal;
        returnedPtr = pKernel->patchBufferOffset(kai, svmPtr.data() + svmOffset, &svmAlloc);
        EXPECT_EQ(svmPtr.data(), returnedPtr);
        EXPECT_EQ(svmOffset, *expectedPatchPtr);
    }
}

template <typename SetArgHandlerT>
class KernelArgSvmTestTyped : public KernelArgSvmTest {
};

struct SetArgHandlerSetArgSvm {
    static void setArg(Kernel &kernel, uint32_t argNum, void *ptrToPatch, size_t allocSize, GraphicsAllocation &alloc) {
        kernel.setArgSvm(argNum, allocSize, ptrToPatch, &alloc);
    }

    static constexpr bool supportsOffsets() {
        return true;
    }
};

struct SetArgHandlerSetArgSvmAlloc {
    static void setArg(Kernel &kernel, uint32_t argNum, void *ptrToPatch, size_t allocSize, GraphicsAllocation &alloc) {
        kernel.setArgSvmAlloc(argNum, ptrToPatch, &alloc);
    }

    static constexpr bool supportsOffsets() {
        return true;
    }
};

struct SetArgHandlerSetArgBuffer {
    static void setArg(Kernel &kernel, uint32_t argNum, void *ptrToPatch, size_t allocSize, GraphicsAllocation &alloc) {
        MockBuffer mb{alloc};
        cl_mem memObj = &mb;
        kernel.setArgBuffer(argNum, sizeof(cl_mem), &memObj);
    }

    static constexpr bool supportsOffsets() {
        return false;
    }
};

using SetArgHandlers = ::testing::Types<SetArgHandlerSetArgSvm, SetArgHandlerSetArgSvmAlloc, SetArgHandlerSetArgBuffer>;

TYPED_TEST_CASE(KernelArgSvmTestTyped, SetArgHandlers);
HWTEST_TYPED_TEST(KernelArgSvmTestTyped, GivenBufferKernelArgWhenBufferOffsetIsNeededTheSetArgSetsIt) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    constexpr size_t rendSurfSize = sizeof(RENDER_SURFACE_STATE);

    uint32_t svmSize = MemoryConstants::pageSize;
    char *svmPtr = reinterpret_cast<char *>(alignedMalloc(svmSize, MemoryConstants::pageSize));

    KernelArgInfo &kai = this->pKernelInfo->kernelArgInfo[0];
    kai.offsetHeap = 0;
    kai.kernelArgPatchInfoVector[0].sourceOffset = 0;
    kai.kernelArgPatchInfoVector[0].crossthreadOffset = 0;
    kai.kernelArgPatchInfoVector[0].size = sizeof(void *);
    kai.offsetBufferOffset = kai.kernelArgPatchInfoVector[0].size;

    this->pKernel->setCrossThreadData(nullptr, kai.offsetBufferOffset + sizeof(uint32_t));
    this->pKernel->setSshLocal(nullptr, rendSurfSize);
    this->pKernelInfo->requiresSshForBuffers = true;
    this->pKernelInfo->usesSsh = true;
    {
        GraphicsAllocation svmAlloc(svmPtr, svmSize);

        constexpr size_t patchOffset = 16;
        void *ptrToPatch = svmPtr + patchOffset;
        size_t sizeToPatch = svmSize - patchOffset;
        ASSERT_GE(this->pKernel->getCrossThreadDataSize(), kai.offsetBufferOffset + sizeof(uint32_t));

        void **expectedPointerPatchPtr = reinterpret_cast<void **>(this->pKernel->getCrossThreadData());
        uint32_t *expectedOffsetPatchPtr = reinterpret_cast<uint32_t *>(ptrOffset(this->pKernel->getCrossThreadData(), kai.offsetBufferOffset));
        *expectedPointerPatchPtr = reinterpret_cast<void *>(0U);
        *expectedOffsetPatchPtr = 0U;

        ASSERT_GE(this->pKernel->getSurfaceStateHeapSize(), rendSurfSize);
        RENDER_SURFACE_STATE *surfState = reinterpret_cast<RENDER_SURFACE_STATE *>(this->pKernel->getSurfaceStateHeap());
        memset(surfState, 0, rendSurfSize);

        TypeParam::setArg(*this->pKernel, 0U, ptrToPatch, sizeToPatch, svmAlloc);

        // surface state for comparison
        RENDER_SURFACE_STATE expectedSurfaceState;
        memset(&expectedSurfaceState, 0, rendSurfSize);

        if (TypeParam::supportsOffsets()) {
            // setArgSvm, setArgSvmAlloc
            EXPECT_EQ(ptrToPatch, *expectedPointerPatchPtr);
            EXPECT_EQ(patchOffset, *expectedOffsetPatchPtr);
        } else {
            // setArgBuffer
            EXPECT_EQ(svmAlloc.getUnderlyingBuffer(), *expectedPointerPatchPtr);
            EXPECT_EQ(0U, *expectedOffsetPatchPtr);
        }

        Buffer::setSurfaceState(this->pContext, &expectedSurfaceState, svmAlloc.getUnderlyingBufferSize(), svmAlloc.getUnderlyingBuffer(), &svmAlloc);

        // verify ssh was properly patched
        int32_t cmpResult = memcmp(&expectedSurfaceState, surfState, rendSurfSize);
        EXPECT_EQ(0, cmpResult);
    }

    alignedFree(svmPtr);
}
