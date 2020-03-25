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

#include "unit_tests/fixtures/execution_model_kernel_fixture.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"
#include "test.h"

#include <memory>

using namespace OCLRT;

typedef ExecutionModelKernelFixture ParentKernelFromBinaryTest;

class MockKernelWithArgumentAccess : public Kernel {
  public:
    std::vector<SimpleKernelArgInfo> &getKernelArguments() {
        return kernelArguments;
    }

    class ObjectCountsPublic : public Kernel::ObjectCounts {
    };

    MockKernelWithArgumentAccess(Program *programArg, const KernelInfo &kernelInfoArg, const Device &deviceArg) : Kernel(programArg, kernelInfoArg, deviceArg) {
    }

    void getParentObjectCountsPublic(MockKernelWithArgumentAccess::ObjectCountsPublic &objectCount) {
        getParentObjectCounts(objectCount);
    }
};

TEST(ParentKernelTest, GetObjectCounts) {
    KernelInfo info;
    MockDevice *device = new MockDevice(*platformDevices[0]);
    MockProgram program;

    SPatchExecutionEnvironment environment;
    environment.HasDeviceEnqueue = 1;

    info.patchInfo.executionEnvironment = &environment;

    MockKernelWithArgumentAccess kernel(&program, info, *device);

    std::vector<Kernel::SimpleKernelArgInfo> &args = kernel.getKernelArguments();

    Kernel::SimpleKernelArgInfo argInfo;
    argInfo.type = Kernel::kernelArgType::SAMPLER_OBJ;
    args.push_back(argInfo);
    argInfo.type = Kernel::kernelArgType::IMAGE_OBJ;
    args.push_back(argInfo);

    MockKernelWithArgumentAccess::ObjectCountsPublic objectCounts;

    kernel.getParentObjectCountsPublic(objectCounts);

    EXPECT_EQ(1u, objectCounts.imageCount);
    EXPECT_EQ(1u, objectCounts.samplerCount);

    delete device;
}

TEST(ParentKernelTest, patchBlocksSimdSize) {
    MockDevice *device = new MockDevice(*platformDevices[0]);
    MockParentKernel *parentKernel = MockParentKernel::create(*device, true);
    MockProgram *program = (MockProgram *)parentKernel->mockProgram;

    parentKernel->patchBlocksSimdSize();

    void *blockSimdSize = ptrOffset(parentKernel->getCrossThreadData(), parentKernel->getKernelInfo().childrenKernelsIdOffset[0].second);
    uint32_t *simdSize = reinterpret_cast<uint32_t *>(blockSimdSize);

    EXPECT_EQ(program->getBlockKernelInfo(0)->getMaxSimdSize(), *simdSize);
    delete parentKernel;
    delete device;
}

TEST(ParentKernelTest, hasDeviceEnqueue) {
    MockDevice device(*platformDevices[0]);
    std::unique_ptr<MockParentKernel> parentKernel(MockParentKernel::create(device));

    EXPECT_TRUE(parentKernel->getKernelInfo().hasDeviceEnqueue());
}

TEST(ParentKernelTest, doesnthaveDeviceEnqueue) {
    MockDevice device(*platformDevices[0]);
    MockKernelWithInternals kernel(device);

    EXPECT_FALSE(kernel.kernelInfo.hasDeviceEnqueue());
}

TEST(ParentKernelTest, initializeOnParentKernelPatchesBlocksSimdSize) {
    MockDevice *device = new MockDevice(*platformDevices[0]);
    MockParentKernel *parentKernel = MockParentKernel::create(*device, true);
    MockProgram *program = (MockProgram *)parentKernel->mockProgram;

    parentKernel->initialize();

    void *blockSimdSize = ptrOffset(parentKernel->getCrossThreadData(), parentKernel->getKernelInfo().childrenKernelsIdOffset[0].second);
    uint32_t *simdSize = reinterpret_cast<uint32_t *>(blockSimdSize);

    EXPECT_EQ(program->getBlockKernelInfo(0)->getMaxSimdSize(), *simdSize);
    delete parentKernel;
    delete device;
}

TEST(ParentKernelTest, initializeOnParentKernelAllocatesPrivateMemoryForBlocks) {
    MockDevice *device = new MockDevice(*platformDevices[0]);
    MockParentKernel *parentKernel = MockParentKernel::create(*device, true);
    MockProgram *program = (MockProgram *)parentKernel->mockProgram;

    uint32_t crossThreadOffsetBlock = 0;

    KernelInfo *infoBlock = new KernelInfo();
    SPatchAllocateStatelessDefaultDeviceQueueSurface *allocateDeviceQueueBlock = new SPatchAllocateStatelessDefaultDeviceQueueSurface;
    allocateDeviceQueueBlock->DataParamOffset = crossThreadOffsetBlock;
    allocateDeviceQueueBlock->DataParamSize = 8;
    allocateDeviceQueueBlock->SurfaceStateHeapOffset = 0;
    allocateDeviceQueueBlock->Size = 8;
    infoBlock->patchInfo.pAllocateStatelessDefaultDeviceQueueSurface = allocateDeviceQueueBlock;

    crossThreadOffsetBlock += 8;

    SPatchAllocateStatelessEventPoolSurface *eventPoolBlock = new SPatchAllocateStatelessEventPoolSurface;
    eventPoolBlock->DataParamOffset = crossThreadOffsetBlock;
    eventPoolBlock->DataParamSize = 8;
    eventPoolBlock->EventPoolSurfaceIndex = 0;
    eventPoolBlock->Size = 8;
    infoBlock->patchInfo.pAllocateStatelessEventPoolSurface = eventPoolBlock;

    crossThreadOffsetBlock += 8;

    SPatchAllocateStatelessPrivateSurface *privateSurfaceBlock = new SPatchAllocateStatelessPrivateSurface;
    privateSurfaceBlock->DataParamOffset = crossThreadOffsetBlock;
    privateSurfaceBlock->DataParamSize = 8;
    privateSurfaceBlock->Size = 8;
    privateSurfaceBlock->SurfaceStateHeapOffset = 0;
    privateSurfaceBlock->Token = 0;
    privateSurfaceBlock->PerThreadPrivateMemorySize = 1000;
    infoBlock->patchInfo.pAllocateStatelessPrivateSurface = privateSurfaceBlock;

    crossThreadOffsetBlock += 8;

    SKernelBinaryHeaderCommon *headerBlock = new SKernelBinaryHeaderCommon;
    headerBlock->DynamicStateHeapSize = 0;
    headerBlock->GeneralStateHeapSize = 0;
    headerBlock->KernelHeapSize = 0;
    headerBlock->KernelNameSize = 0;
    headerBlock->PatchListSize = 0;
    headerBlock->SurfaceStateHeapSize = 0;
    infoBlock->heapInfo.pKernelHeader = headerBlock;

    SPatchThreadPayload *threadPayloadBlock = new SPatchThreadPayload;
    threadPayloadBlock->LocalIDXPresent = 0;
    threadPayloadBlock->LocalIDYPresent = 0;
    threadPayloadBlock->LocalIDZPresent = 0;
    threadPayloadBlock->HeaderPresent = 0;
    threadPayloadBlock->Size = 128;

    infoBlock->patchInfo.threadPayload = threadPayloadBlock;

    SPatchExecutionEnvironment *executionEnvironmentBlock = new SPatchExecutionEnvironment;
    executionEnvironmentBlock->HasDeviceEnqueue = 1;
    infoBlock->patchInfo.executionEnvironment = executionEnvironmentBlock;

    SPatchDataParameterStream *streamBlock = new SPatchDataParameterStream;
    streamBlock->DataParameterStreamSize = 0;
    streamBlock->Size = 0;
    infoBlock->patchInfo.dataParameterStream = streamBlock;

    SPatchBindingTableState *bindingTable = new SPatchBindingTableState;
    bindingTable->Count = 0;
    bindingTable->Offset = 0;
    bindingTable->Size = 0;
    bindingTable->SurfaceStateOffset = 0;
    infoBlock->patchInfo.bindingTableState = bindingTable;

    SPatchInterfaceDescriptorData *idData = new SPatchInterfaceDescriptorData;
    idData->BindingTableOffset = 0;
    idData->KernelOffset = 0;
    idData->Offset = 0;
    idData->SamplerStateOffset = 0;
    idData->Size = 0;
    infoBlock->patchInfo.interfaceDescriptorData = idData;

    infoBlock->patchInfo.pAllocateStatelessGlobalMemorySurfaceWithInitialization = nullptr;
    infoBlock->patchInfo.pAllocateStatelessConstantMemorySurfaceWithInitialization = nullptr;

    infoBlock->heapInfo.pDsh = (void *)new uint64_t[64];
    infoBlock->crossThreadData = new char[crossThreadOffsetBlock];

    program->addBlockKernel(infoBlock);

    parentKernel->initialize();

    EXPECT_NE(nullptr, program->getBlockKernelManager()->getPrivateSurface(program->getBlockKernelManager()->getCount() - 1));

    delete privateSurfaceBlock;
    delete parentKernel;
    delete device;
}

TEST_P(ParentKernelFromBinaryTest, getInstructionHeapSizeForExecutionModelReturnsNonZeroForParentKernel) {
    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.") != std::string::npos) {
        EXPECT_TRUE(pKernel->isParentKernel);

        EXPECT_LT(0u, pKernel->getInstructionHeapSizeForExecutionModel());
    }
}

static const char *binaryFile = "simple_block_kernel";
static const char *KernelNames[] = {"simple_block_kernel"};

INSTANTIATE_TEST_CASE_P(ParentKernelFromBinaryTest,
                        ParentKernelFromBinaryTest,
                        ::testing::Combine(
                            ::testing::Values(binaryFile),
                            ::testing::ValuesIn(KernelNames)));
