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

#pragma once

#include "runtime/helpers/string.h"
#include "runtime/kernel/kernel.h"
#include "runtime/scheduler/scheduler_kernel.h"
#include "runtime/device/device.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/mocks/mock_context.h"
#include <cassert>

namespace OCLRT {

////////////////////////////////////////////////////////////////////////////////
// Kernel - Core implementation
////////////////////////////////////////////////////////////////////////////////
class MockKernel : public Kernel {
  public:
    struct BlockPatchValues {
        uint64_t offset;
        uint32_t size;
        uint64_t address;
    };

    class ReflectionSurfaceHelperPublic : public Kernel::ReflectionSurfaceHelper {
      public:
        static BlockPatchValues devQueue;
        static BlockPatchValues defaultQueue;
        static BlockPatchValues eventPool;
        static BlockPatchValues printfBuffer;
        static const uint64_t undefinedOffset = (uint64_t)-1;

        static void patchBlocksCurbeMock(void *reflectionSurface, uint32_t blockID,
                                         uint64_t defaultDeviceQueueCurbeOffset, uint32_t patchSizeDefaultQueue, uint64_t defaultDeviceQueueGpuAddress,
                                         uint64_t eventPoolCurbeOffset, uint32_t patchSizeEventPool, uint64_t eventPoolGpuAddress,
                                         uint64_t deviceQueueCurbeOffset, uint32_t patchSizeDeviceQueue, uint64_t deviceQueueGpuAddress,
                                         uint64_t printfBufferOffset, uint32_t patchSizePrintfBuffer, uint64_t printfBufferGpuAddress) {
            defaultQueue.address = defaultDeviceQueueGpuAddress;
            defaultQueue.offset = defaultDeviceQueueCurbeOffset;
            defaultQueue.size = patchSizeDefaultQueue;

            devQueue.address = deviceQueueGpuAddress;
            devQueue.offset = deviceQueueCurbeOffset;
            devQueue.size = patchSizeDeviceQueue;

            eventPool.address = eventPoolGpuAddress;
            eventPool.offset = eventPoolCurbeOffset;
            eventPool.size = patchSizeEventPool;

            printfBuffer.address = printfBufferGpuAddress;
            printfBuffer.offset = printfBufferOffset;
            printfBuffer.size = patchSizePrintfBuffer;
        }

        static uint32_t getConstantBufferOffset(void *reflectionSurface, uint32_t blockID) {
            IGIL_KernelDataHeader *pKernelHeader = reinterpret_cast<IGIL_KernelDataHeader *>(reflectionSurface);
            assert(blockID < pKernelHeader->m_numberOfKernels);

            IGIL_KernelAddressData *addressData = pKernelHeader->m_data;
            assert(addressData[blockID].m_ConstantBufferOffset != 0);

            return addressData[blockID].m_ConstantBufferOffset;
        }
    };

    MockKernel(Program *programArg, const KernelInfo &kernelInfoArg, const Device &deviceArg, bool scheduler = false)
        : Kernel(programArg, kernelInfoArg, deviceArg, scheduler) {
    }

    ~MockKernel() {
        // prevent double deletion
        if (Kernel::crossThreadData == mockCrossThreadData.data()) {
            Kernel::crossThreadData = nullptr;
        }

        if (Kernel::pSshLocal == mockSshLocal.data()) {
            Kernel::pSshLocal = nullptr;
        }

        if (kernelInfoAllocated) {
            delete kernelInfoAllocated->heapInfo.pKernelHeader;
            delete kernelInfoAllocated->patchInfo.executionEnvironment;
            delete kernelInfoAllocated->patchInfo.threadPayload;
            delete kernelInfoAllocated;
        }
    }

    template <typename KernelType = MockKernel>
    static KernelType *create(Device &device, Program *program) {
        KernelInfo *info = new KernelInfo();
        const size_t crossThreadSize = 160;

        SKernelBinaryHeaderCommon *header = new SKernelBinaryHeaderCommon;
        header->DynamicStateHeapSize = 0;
        header->GeneralStateHeapSize = 0;
        header->KernelHeapSize = 0;
        header->KernelNameSize = 0;
        header->PatchListSize = 0;
        header->SurfaceStateHeapSize = 0;
        info->heapInfo.pKernelHeader = header;

        SPatchThreadPayload *threadPayload = new SPatchThreadPayload;
        threadPayload->LocalIDXPresent = 0;
        threadPayload->LocalIDYPresent = 0;
        threadPayload->LocalIDZPresent = 0;
        threadPayload->HeaderPresent = 0;
        threadPayload->Size = 128;

        info->patchInfo.threadPayload = threadPayload;

        SPatchExecutionEnvironment *executionEnvironment = new SPatchExecutionEnvironment;
        memset(executionEnvironment, 0, sizeof(SPatchExecutionEnvironment));
        executionEnvironment->HasDeviceEnqueue = 0;
        info->patchInfo.executionEnvironment = executionEnvironment;

        info->crossThreadData = new char[crossThreadSize];

        auto kernel = new KernelType(program, *info, device);
        kernel->crossThreadData = new char[crossThreadSize];
        memset(kernel->crossThreadData, 0, crossThreadSize);
        kernel->crossThreadDataSize = crossThreadSize;

        kernel->kernelInfoAllocated = info;

        return kernel;
    }

    uint32_t getPatchedArgumentsNum() const { return patchedArgumentsNum; }

    cl_int initialize() {
        return Kernel::initialize();
    }
    bool isPatched() const override;

    bool canTransformImages() const override;

    ////////////////////////////////////////////////////////////////////////////////
    void setCrossThreadData(const void *crossThreadDataPattern, uint32_t newCrossThreadDataSize) {
        if ((Kernel::crossThreadData != nullptr) && (Kernel::crossThreadData != mockCrossThreadData.data())) {
            delete[] Kernel::crossThreadData;
            Kernel::crossThreadData = nullptr;
            Kernel::crossThreadDataSize = 0;
        }
        if (crossThreadDataPattern && (newCrossThreadDataSize > 0)) {
            mockCrossThreadData.clear();
            mockCrossThreadData.insert(mockCrossThreadData.begin(), (char *)crossThreadDataPattern, ((char *)crossThreadDataPattern) + newCrossThreadDataSize);
        } else {
            mockCrossThreadData.resize(newCrossThreadDataSize, 0);
        }

        if (newCrossThreadDataSize == 0) {
            return;
        }
        Kernel::crossThreadData = mockCrossThreadData.data();
        Kernel::crossThreadDataSize = static_cast<uint32_t>(mockCrossThreadData.size());
    }

    void setSshLocal(const void *sshPattern, uint32_t newSshSize) {
        if ((Kernel::pSshLocal != nullptr) && (Kernel::pSshLocal != mockSshLocal.data())) {
            delete[] Kernel::pSshLocal;
            Kernel::pSshLocal = nullptr;
            Kernel::sshLocalSize = 0;
        }

        if (sshPattern && (newSshSize > 0)) {
            mockSshLocal.clear();
            mockSshLocal.insert(mockSshLocal.begin(), (char *)sshPattern, ((char *)sshPattern) + newSshSize);
        } else {
            mockSshLocal.resize(newSshSize, 0);
        }

        if (newSshSize == 0) {
            return;
        }

        Kernel::pSshLocal = mockSshLocal.data();
        Kernel::sshLocalSize = static_cast<uint32_t>(mockSshLocal.size());
    }

    void setPrivateSurface(GraphicsAllocation *gfxAllocation, uint32_t size) {
        privateSurface = gfxAllocation;
        privateSurfaceSize = size;
    }

    GraphicsAllocation *getPrivateSurface() const {
        return privateSurface;
    }

    void setTotalSLMSize(uint32_t size) {
        slmTotalSize = size;
    }

    void setKernelArguments(std::vector<SimpleKernelArgInfo> kernelArguments) {
        this->kernelArguments = kernelArguments;
    }

    template <typename PatchTokenT>
    void patchWithImplicitSurface(void *ptrToPatchInCrossThreadData, GraphicsAllocation &allocation, const PatchTokenT &patch) {
        Kernel::patchWithImplicitSurface(ptrToPatchInCrossThreadData, allocation, patch);
    }

    void *patchBufferOffset(const KernelArgInfo &argInfo, void *svmPtr, GraphicsAllocation *svmAlloc) {
        return Kernel::patchBufferOffset(argInfo, svmPtr, svmAlloc);
    }

    KernelInfo *getAllocatedKernelInfo() {
        return kernelInfoAllocated;
    }

    std::vector<char> mockCrossThreadData;
    std::vector<char> mockSshLocal;

    // Make protected members from base class publicly accessible in mock class
    using Kernel::kernelArgHandlers;

    void setUsingSharedArgs(bool usingSharedArgValue) { this->usingSharedObjArgs = usingSharedArgValue; }

    void makeResident(CommandStreamReceiver &commandStreamReceiver) override;
    void getResidency(std::vector<Surface *> &dst) override;

    uint32_t makeResidentCalls = 0;
    uint32_t getResidencyCalls = 0;

    bool canKernelTransformImages = true;

  protected:
    KernelInfo *kernelInfoAllocated = nullptr;
};

//class below have enough internals to service Enqueue operation.
class MockKernelWithInternals {
  public:
    MockKernelWithInternals(const Device &deviceArg, Context *context = nullptr) {
        memset(&kernelHeader, 0, sizeof(SKernelBinaryHeaderCommon));
        memset(&threadPayload, 0, sizeof(SPatchThreadPayload));
        memset(&executionEnvironment, 0, sizeof(SPatchExecutionEnvironment));
        memset(&executionEnvironmentBlock, 0, sizeof(SPatchExecutionEnvironment));
        memset(&dataParameterStream, 0, sizeof(SPatchDataParameterStream));
        kernelHeader.SurfaceStateHeapSize = sizeof(sshLocal);
        threadPayload.LocalIDXPresent = 1;
        threadPayload.LocalIDYPresent = 1;
        threadPayload.LocalIDZPresent = 1;
        kernelInfo.heapInfo.pKernelHeap = kernelIsa;
        kernelInfo.heapInfo.pSsh = sshLocal;
        kernelInfo.heapInfo.pKernelHeader = &kernelHeader;
        kernelInfo.patchInfo.dataParameterStream = &dataParameterStream;
        kernelInfo.patchInfo.executionEnvironment = &executionEnvironment;
        kernelInfo.patchInfo.threadPayload = &threadPayload;

        if (context == nullptr) {
            mockContext = new MockContext;
            context = mockContext;
        } else {
            context->incRefInternal();
            mockContext = context;
        }

        mockProgram = new MockProgram(context, false);
        mockKernel = new MockKernel(mockProgram, kernelInfo, deviceArg);
        mockKernel->setCrossThreadData(&crossThreadData, sizeof(crossThreadData));
        mockKernel->setSshLocal(&sshLocal, sizeof(sshLocal));
    }
    ~MockKernelWithInternals() {
        mockKernel->decRefInternal();
        mockProgram->decRefInternal();
        mockContext->decRefInternal();
    }

    operator MockKernel *() {
        return mockKernel;
    }

    MockKernel *mockKernel;
    MockProgram *mockProgram;
    Context *mockContext;
    KernelInfo kernelInfo;
    SKernelBinaryHeaderCommon kernelHeader;
    SPatchThreadPayload threadPayload;
    SPatchDataParameterStream dataParameterStream;
    SPatchExecutionEnvironment executionEnvironment;
    SPatchExecutionEnvironment executionEnvironmentBlock;
    uint32_t kernelIsa[32];
    char crossThreadData[256];
    char sshLocal[128];
};

class MockParentKernel : public Kernel {
  public:
    using Kernel::patchBlocksCurbeWithConstantValues;
    static MockParentKernel *create(Device &device, bool addChildSimdSize = false, bool addChildGlobalMemory = false, bool addChildConstantMemory = false, bool addPrintfForParent = true, bool addPrintfForBlock = true) {
        KernelInfo *info = new KernelInfo();
        const size_t crossThreadSize = 160;
        uint32_t crossThreadOffset = 0;
        uint32_t crossThreadOffsetBlock = 0;

        SKernelBinaryHeaderCommon *header = new SKernelBinaryHeaderCommon;
        header->DynamicStateHeapSize = 0;
        header->GeneralStateHeapSize = 0;
        header->KernelHeapSize = 0;
        header->KernelNameSize = 0;
        header->PatchListSize = 0;
        header->SurfaceStateHeapSize = 0;
        info->heapInfo.pKernelHeader = header;

        SPatchThreadPayload *threadPayload = new SPatchThreadPayload;
        threadPayload->LocalIDXPresent = 0;
        threadPayload->LocalIDYPresent = 0;
        threadPayload->LocalIDZPresent = 0;
        threadPayload->HeaderPresent = 0;
        threadPayload->Size = 128;

        info->patchInfo.threadPayload = threadPayload;

        SPatchExecutionEnvironment *executionEnvironment = new SPatchExecutionEnvironment;
        executionEnvironment->HasDeviceEnqueue = 1;
        info->patchInfo.executionEnvironment = executionEnvironment;

        SPatchAllocateStatelessDefaultDeviceQueueSurface *allocateDeviceQueue = new SPatchAllocateStatelessDefaultDeviceQueueSurface;
        allocateDeviceQueue->DataParamOffset = crossThreadOffset;
        allocateDeviceQueue->DataParamSize = 8;
        allocateDeviceQueue->SurfaceStateHeapOffset = 0;
        allocateDeviceQueue->Size = 8;
        info->patchInfo.pAllocateStatelessDefaultDeviceQueueSurface = allocateDeviceQueue;

        crossThreadOffset += 8;

        SPatchAllocateStatelessEventPoolSurface *eventPool = new SPatchAllocateStatelessEventPoolSurface;
        eventPool->DataParamOffset = crossThreadOffset;
        eventPool->DataParamSize = 8;
        eventPool->EventPoolSurfaceIndex = 0;
        eventPool->Size = 8;
        info->patchInfo.pAllocateStatelessEventPoolSurface = eventPool;

        crossThreadOffset += 8;
        if (addPrintfForParent) {
            SPatchAllocateStatelessPrintfSurface *printfBuffer = new SPatchAllocateStatelessPrintfSurface;
            printfBuffer->DataParamOffset = crossThreadOffset;
            printfBuffer->DataParamSize = 8;
            printfBuffer->PrintfSurfaceIndex = 0;
            printfBuffer->Size = 8;
            printfBuffer->SurfaceStateHeapOffset = 0;
            printfBuffer->Token = 0;
            info->patchInfo.pAllocateStatelessPrintfSurface = printfBuffer;

            crossThreadOffset += 8;
        }

        MockProgram *mockProgram = new MockProgram();
        mockProgram->setContext(getContext());
        mockProgram->setDevice(&device);

        if (addChildSimdSize) {
            info->childrenKernelsIdOffset.push_back({0, crossThreadOffset});
        }

        crossThreadOffset += 8;

        assert(crossThreadSize >= crossThreadOffset);
        info->crossThreadData = new char[crossThreadSize];

        auto parent = new MockParentKernel(mockProgram, *info, device);
        parent->crossThreadData = new char[crossThreadSize];
        memset(parent->crossThreadData, 0, crossThreadSize);
        parent->crossThreadDataSize = crossThreadSize;

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
        if (addPrintfForBlock) {
            SPatchAllocateStatelessPrintfSurface *printfBufferBlock = new SPatchAllocateStatelessPrintfSurface;
            printfBufferBlock->DataParamOffset = crossThreadOffsetBlock;
            printfBufferBlock->DataParamSize = 8;
            printfBufferBlock->PrintfSurfaceIndex = 0;
            printfBufferBlock->Size = 8;
            printfBufferBlock->SurfaceStateHeapOffset = 0;
            printfBufferBlock->Token = 0;
            infoBlock->patchInfo.pAllocateStatelessPrintfSurface = printfBufferBlock;

            crossThreadOffsetBlock += 8;
        }

        infoBlock->patchInfo.pAllocateStatelessGlobalMemorySurfaceWithInitialization = nullptr;
        infoBlock->patchInfo.pAllocateStatelessConstantMemorySurfaceWithInitialization = nullptr;

        if (addChildGlobalMemory) {
            SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization *globalMemoryBlock = new SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization;
            globalMemoryBlock->DataParamOffset = crossThreadOffsetBlock;
            globalMemoryBlock->DataParamSize = 8;
            globalMemoryBlock->Size = 8;
            globalMemoryBlock->SurfaceStateHeapOffset = 0;
            globalMemoryBlock->Token = 0;
            infoBlock->patchInfo.pAllocateStatelessGlobalMemorySurfaceWithInitialization = globalMemoryBlock;
            crossThreadOffsetBlock += 8;
        }

        if (addChildConstantMemory) {
            SPatchAllocateStatelessConstantMemorySurfaceWithInitialization *constantMemoryBlock = new SPatchAllocateStatelessConstantMemorySurfaceWithInitialization;
            constantMemoryBlock->DataParamOffset = crossThreadOffsetBlock;
            constantMemoryBlock->DataParamSize = 8;
            constantMemoryBlock->Size = 8;
            constantMemoryBlock->SurfaceStateHeapOffset = 0;
            constantMemoryBlock->Token = 0;
            infoBlock->patchInfo.pAllocateStatelessConstantMemorySurfaceWithInitialization = constantMemoryBlock;
            crossThreadOffsetBlock += 8;
        }

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

        infoBlock->heapInfo.pDsh = (void *)new uint64_t[64];
        infoBlock->crossThreadData = new char[crossThreadOffsetBlock > crossThreadSize ? crossThreadOffsetBlock : crossThreadSize];

        mockProgram->addBlockKernel(infoBlock);
        parent->mockProgram = mockProgram;

        return parent;
    }

    MockParentKernel(Program *programArg, const KernelInfo &kernelInfoArg, const Device &deviceArg) : Kernel(programArg, kernelInfoArg, deviceArg) {
    }

    ~MockParentKernel() {
        delete kernelInfo.patchInfo.executionEnvironment;
        delete kernelInfo.patchInfo.pAllocateStatelessDefaultDeviceQueueSurface;
        delete kernelInfo.patchInfo.pAllocateStatelessEventPoolSurface;
        delete kernelInfo.patchInfo.pAllocateStatelessPrintfSurface;
        delete kernelInfo.patchInfo.threadPayload;
        delete kernelInfo.heapInfo.pKernelHeader;
        delete &kernelInfo;
        cleanContext();
        BlockKernelManager *blockManager = program->getBlockKernelManager();

        for (uint32_t i = 0; i < blockManager->getCount(); i++) {
            const KernelInfo *blockInfo = blockManager->getBlockKernelInfo(i);
            delete blockInfo->patchInfo.pAllocateStatelessDefaultDeviceQueueSurface;
            delete blockInfo->patchInfo.pAllocateStatelessEventPoolSurface;
            delete blockInfo->patchInfo.pAllocateStatelessPrintfSurface;
            delete blockInfo->heapInfo.pKernelHeader;
            delete blockInfo->patchInfo.threadPayload;
            delete blockInfo->patchInfo.executionEnvironment;
            delete blockInfo->patchInfo.dataParameterStream;
            delete blockInfo->patchInfo.bindingTableState;
            delete blockInfo->patchInfo.interfaceDescriptorData;
            delete blockInfo->patchInfo.pAllocateStatelessConstantMemorySurfaceWithInitialization;
            delete blockInfo->patchInfo.pAllocateStatelessGlobalMemorySurfaceWithInitialization;
            delete[](uint64_t *) blockInfo->heapInfo.pDsh;
        }
        if (mockProgram) {
            mockProgram->decRefInternal();
        }
    }

    static MockContext *&getContext() {
        static MockContext *context;
        if (context == nullptr)
            context = new MockContext;
        return context;
    }

    static void cleanContext() {
        MockContext *&context = getContext();
        delete context;
        context = nullptr;
    }

    void setReflectionSurface(GraphicsAllocation *reflectionSurface) {
        kernelReflectionSurface = reflectionSurface;
    }

    MockProgram *mockProgram;
};

class MockSchedulerKernel : public SchedulerKernel {
  public:
    MockSchedulerKernel(Program *programArg, const KernelInfo &kernelInfoArg, const Device &deviceArg) : SchedulerKernel(programArg, kernelInfoArg, deviceArg){};
};

class MockDebugKernel : public MockKernel {
  public:
    MockDebugKernel(Program *program, KernelInfo &kernelInfo, const Device &device) : MockKernel(program, kernelInfo, device) {
        if (!kernelInfo.patchInfo.pAllocateSystemThreadSurface) {
            SPatchAllocateSystemThreadSurface *patchToken = new SPatchAllocateSystemThreadSurface;

            patchToken->BTI = 0;
            patchToken->Offset = 0;
            patchToken->PerThreadSystemThreadSurfaceSize = MockDebugKernel::perThreadSystemThreadSurfaceSize;
            patchToken->Size = sizeof(SPatchAllocateSystemThreadSurface);
            patchToken->Token = iOpenCL::PATCH_TOKEN_ALLOCATE_SIP_SURFACE;

            kernelInfo.patchInfo.pAllocateSystemThreadSurface = patchToken;

            systemThreadSurfaceAllocated = true;
        }
    }

    ~MockDebugKernel() override {
        if (systemThreadSurfaceAllocated) {
            delete kernelInfo.patchInfo.pAllocateSystemThreadSurface;
        }
    }
    static const uint32_t perThreadSystemThreadSurfaceSize;
    bool systemThreadSurfaceAllocated = false;
};

} // namespace OCLRT
