/*
 * Copyright (c) 2018, Intel Corporation
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

#include "kernel.h"
#include "runtime/accelerators/intel_accelerator.h"
#include "runtime/accelerators/intel_motion_estimation.h"
#include "runtime/built_ins/built_ins.h"
#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/context/context.h"
#include "runtime/device_queue/device_queue.h"
#include "runtime/execution_model/device_enqueue.h"
#include "runtime/gtpin/gtpin_notify.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/get_info.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/per_thread_data.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/sampler_helpers.h"
#include "runtime/helpers/surface_formats.h"
#include "runtime/kernel/image_transformer.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/image.h"
#include "runtime/mem_obj/pipe.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/memory_manager/surface.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/program/kernel_info.h"
#include "runtime/program/printf_handler.h"
#include "runtime/sampler/sampler.h"
#include "patch_list.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "runtime/kernel/kernel.inl"

using namespace iOpenCL;

namespace OCLRT {
class Surface;

uint32_t Kernel::dummyPatchLocation = 0xbaddf00d;

Kernel::Kernel(Program *programArg, const KernelInfo &kernelInfoArg, const Device &deviceArg, bool schedulerKernel)
    : globalWorkOffsetX(&Kernel::dummyPatchLocation),
      globalWorkOffsetY(&Kernel::dummyPatchLocation),
      globalWorkOffsetZ(&Kernel::dummyPatchLocation),
      localWorkSizeX(&Kernel::dummyPatchLocation),
      localWorkSizeY(&Kernel::dummyPatchLocation),
      localWorkSizeZ(&Kernel::dummyPatchLocation),
      localWorkSizeX2(&Kernel::dummyPatchLocation),
      localWorkSizeY2(&Kernel::dummyPatchLocation),
      localWorkSizeZ2(&Kernel::dummyPatchLocation),
      globalWorkSizeX(&Kernel::dummyPatchLocation),
      globalWorkSizeY(&Kernel::dummyPatchLocation),
      globalWorkSizeZ(&Kernel::dummyPatchLocation),
      enqueuedLocalWorkSizeX(&Kernel::dummyPatchLocation),
      enqueuedLocalWorkSizeY(&Kernel::dummyPatchLocation),
      enqueuedLocalWorkSizeZ(&Kernel::dummyPatchLocation),
      numWorkGroupsX(&Kernel::dummyPatchLocation),
      numWorkGroupsY(&Kernel::dummyPatchLocation),
      numWorkGroupsZ(&Kernel::dummyPatchLocation),
      maxWorkGroupSize(&Kernel::dummyPatchLocation),
      workDim(&Kernel::dummyPatchLocation),
      dataParameterSimdSize(&Kernel::dummyPatchLocation),
      parentEventOffset(&Kernel::dummyPatchLocation),
      prefferedWkgMultipleOffset(&Kernel::dummyPatchLocation),
      slmTotalSize(kernelInfoArg.workloadInfo.slmStaticSize),
      isBuiltIn(false),
      isParentKernel((kernelInfoArg.patchInfo.executionEnvironment != nullptr) ? (kernelInfoArg.patchInfo.executionEnvironment->HasDeviceEnqueue != 0) : false),
      isSchedulerKernel(schedulerKernel),
      program(programArg),
      context(nullptr),
      device(deviceArg),
      kernelInfo(kernelInfoArg),
      numberOfBindingTableStates(0),
      localBindingTableOffset(0),
      pSshLocal(nullptr),
      sshLocalSize(0),
      crossThreadData(nullptr),
      crossThreadDataSize(0),
      privateSurface(nullptr),
      privateSurfaceSize(0),
      kernelReflectionSurface(nullptr),
      usingSharedObjArgs(false) {
    program->retain();
    imageTransformer.reset(new ImageTransformer);
}

Kernel::~Kernel() {
    delete[] pSshLocal;
    pSshLocal = nullptr;

    delete[] crossThreadData;
    crossThreadData = nullptr;
    crossThreadDataSize = 0;

    if (privateSurface) {
        device.getMemoryManager()->checkGpuUsageAndDestroyGraphicsAllocations(privateSurface);
        privateSurface = nullptr;
    }

    if (kernelReflectionSurface) {
        device.getMemoryManager()->freeGraphicsMemory(kernelReflectionSurface);
        kernelReflectionSurface = nullptr;
    }

    kernelArgHandlers.clear();
    program->release();
}

// Checks if patch offset is invalid (undefined)
inline bool isInvalidOffset(uint32_t offset) {
    return (offset == KernelArgInfo::undefinedOffset);
}

// Checks if patch offset is valid
inline bool isValidOffset(uint32_t offset) {
    return isInvalidOffset(offset) == false;
}

// If dstOffsetBytes is not an invalid offset, then patches dst at dstOffsetBytes
// with src casted to DstT type.
template <typename DstT, typename SrcT>
inline void patch(const SrcT &src, void *dst, uint32_t dstOffsetBytes) {
    if (isInvalidOffset(dstOffsetBytes)) {
        return;
    }

    DstT *patchLocation = reinterpret_cast<DstT *>(ptrOffset(dst, dstOffsetBytes));
    *patchLocation = static_cast<DstT>(src);
}

template <typename PatchTokenT>
void Kernel::patchWithImplicitSurface(void *ptrToPatchInCrossThreadData, GraphicsAllocation &allocation, const PatchTokenT &patch) {
    uint32_t crossThreadDataOffset = patch.DataParamOffset;
    uint32_t pointerSize = patch.DataParamSize;
    uint32_t sshOffset = patch.SurfaceStateHeapOffset;
    void *crossThreadData = getCrossThreadData();
    void *ssh = getSurfaceStateHeap();
    if (crossThreadData != nullptr) {
        auto pp = ptrOffset(crossThreadData, crossThreadDataOffset);
        uintptr_t addressToPatch = reinterpret_cast<uintptr_t>(ptrToPatchInCrossThreadData);
        patchWithRequiredSize(pp, pointerSize, addressToPatch);
    }

    if (ssh) {
        auto surfaceState = ptrOffset(ssh, sshOffset);
        void *addressToPatch = reinterpret_cast<void *>(allocation.getUnderlyingBuffer());
        size_t sizeToPatch = allocation.getUnderlyingBufferSize();
        Buffer::setSurfaceState(&getContext(), surfaceState, sizeToPatch, addressToPatch, &allocation);
    }
}

template void Kernel::patchWithImplicitSurface(void *ptrToPatchInCrossThreadData, GraphicsAllocation &allocation, const SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization &patch);

template void Kernel::patchWithImplicitSurface(void *ptrToPatchInCrossThreadData, GraphicsAllocation &allocation, const SPatchAllocateStatelessPrivateSurface &patch);

template void Kernel::patchWithImplicitSurface(void *ptrToPatchInCrossThreadData, GraphicsAllocation &allocation, const SPatchAllocateStatelessConstantMemorySurfaceWithInitialization &patch);

cl_int Kernel::initialize() {
    cl_int retVal = CL_OUT_OF_HOST_MEMORY;
    do {
        const auto &workloadInfo = kernelInfo.workloadInfo;
        const auto &heapInfo = kernelInfo.heapInfo;
        const auto &patchInfo = kernelInfo.patchInfo;

        crossThreadDataSize = patchInfo.dataParameterStream
                                  ? patchInfo.dataParameterStream->DataParameterStreamSize
                                  : 0;

        // now allocate our own cross-thread data, if necessary
        if (crossThreadDataSize) {
            crossThreadData = new char[crossThreadDataSize];

            if (kernelInfo.crossThreadData) {
                memcpy_s(crossThreadData, crossThreadDataSize, kernelInfo.crossThreadData, crossThreadDataSize);
            } else {
                memset(crossThreadData, 0x00, crossThreadDataSize);
            }

            auto crossThread = reinterpret_cast<uint32_t *>(crossThreadData);
            globalWorkOffsetX = workloadInfo.globalWorkOffsetOffsets[0] != WorkloadInfo::undefinedOffset ? ptrOffset(crossThread, workloadInfo.globalWorkOffsetOffsets[0]) : globalWorkOffsetX;
            globalWorkOffsetY = workloadInfo.globalWorkOffsetOffsets[1] != WorkloadInfo::undefinedOffset ? ptrOffset(crossThread, workloadInfo.globalWorkOffsetOffsets[1]) : globalWorkOffsetY;
            globalWorkOffsetZ = workloadInfo.globalWorkOffsetOffsets[2] != WorkloadInfo::undefinedOffset ? ptrOffset(crossThread, workloadInfo.globalWorkOffsetOffsets[2]) : globalWorkOffsetZ;

            localWorkSizeX = workloadInfo.localWorkSizeOffsets[0] != WorkloadInfo::undefinedOffset ? ptrOffset(crossThread, workloadInfo.localWorkSizeOffsets[0]) : localWorkSizeX;
            localWorkSizeY = workloadInfo.localWorkSizeOffsets[1] != WorkloadInfo::undefinedOffset ? ptrOffset(crossThread, workloadInfo.localWorkSizeOffsets[1]) : localWorkSizeY;
            localWorkSizeZ = workloadInfo.localWorkSizeOffsets[2] != WorkloadInfo::undefinedOffset ? ptrOffset(crossThread, workloadInfo.localWorkSizeOffsets[2]) : localWorkSizeZ;

            localWorkSizeX2 = workloadInfo.localWorkSizeOffsets2[0] != WorkloadInfo::undefinedOffset ? ptrOffset(crossThread, workloadInfo.localWorkSizeOffsets2[0]) : localWorkSizeX2;
            localWorkSizeY2 = workloadInfo.localWorkSizeOffsets2[1] != WorkloadInfo::undefinedOffset ? ptrOffset(crossThread, workloadInfo.localWorkSizeOffsets2[1]) : localWorkSizeY2;
            localWorkSizeZ2 = workloadInfo.localWorkSizeOffsets2[2] != WorkloadInfo::undefinedOffset ? ptrOffset(crossThread, workloadInfo.localWorkSizeOffsets2[2]) : localWorkSizeZ2;

            globalWorkSizeX = workloadInfo.globalWorkSizeOffsets[0] != WorkloadInfo::undefinedOffset ? ptrOffset(crossThread, workloadInfo.globalWorkSizeOffsets[0]) : globalWorkSizeX;
            globalWorkSizeY = workloadInfo.globalWorkSizeOffsets[1] != WorkloadInfo::undefinedOffset ? ptrOffset(crossThread, workloadInfo.globalWorkSizeOffsets[1]) : globalWorkSizeY;
            globalWorkSizeZ = workloadInfo.globalWorkSizeOffsets[2] != WorkloadInfo::undefinedOffset ? ptrOffset(crossThread, workloadInfo.globalWorkSizeOffsets[2]) : globalWorkSizeZ;

            enqueuedLocalWorkSizeX = workloadInfo.enqueuedLocalWorkSizeOffsets[0] != WorkloadInfo::undefinedOffset ? ptrOffset(crossThread, workloadInfo.enqueuedLocalWorkSizeOffsets[0]) : enqueuedLocalWorkSizeX;
            enqueuedLocalWorkSizeY = workloadInfo.enqueuedLocalWorkSizeOffsets[1] != WorkloadInfo::undefinedOffset ? ptrOffset(crossThread, workloadInfo.enqueuedLocalWorkSizeOffsets[1]) : enqueuedLocalWorkSizeY;
            enqueuedLocalWorkSizeZ = workloadInfo.enqueuedLocalWorkSizeOffsets[2] != WorkloadInfo::undefinedOffset ? ptrOffset(crossThread, workloadInfo.enqueuedLocalWorkSizeOffsets[2]) : enqueuedLocalWorkSizeZ;

            numWorkGroupsX = workloadInfo.numWorkGroupsOffset[0] != WorkloadInfo::undefinedOffset ? ptrOffset(crossThread, workloadInfo.numWorkGroupsOffset[0]) : numWorkGroupsX;
            numWorkGroupsY = workloadInfo.numWorkGroupsOffset[1] != WorkloadInfo::undefinedOffset ? ptrOffset(crossThread, workloadInfo.numWorkGroupsOffset[1]) : numWorkGroupsY;
            numWorkGroupsZ = workloadInfo.numWorkGroupsOffset[2] != WorkloadInfo::undefinedOffset ? ptrOffset(crossThread, workloadInfo.numWorkGroupsOffset[2]) : numWorkGroupsZ;

            maxWorkGroupSize = workloadInfo.maxWorkGroupSizeOffset != WorkloadInfo::undefinedOffset ? ptrOffset(crossThread, workloadInfo.maxWorkGroupSizeOffset) : maxWorkGroupSize;
            workDim = workloadInfo.workDimOffset != WorkloadInfo::undefinedOffset ? ptrOffset(crossThread, workloadInfo.workDimOffset) : workDim;
            dataParameterSimdSize = workloadInfo.simdSizeOffset != WorkloadInfo::undefinedOffset ? ptrOffset(crossThread, workloadInfo.simdSizeOffset) : dataParameterSimdSize;
            parentEventOffset = workloadInfo.parentEventOffset != WorkloadInfo::undefinedOffset ? ptrOffset(crossThread, workloadInfo.parentEventOffset) : parentEventOffset;
            prefferedWkgMultipleOffset = workloadInfo.prefferedWkgMultipleOffset != WorkloadInfo::undefinedOffset ? ptrOffset(crossThread, workloadInfo.prefferedWkgMultipleOffset) : prefferedWkgMultipleOffset;

            *maxWorkGroupSize = static_cast<uint32_t>(device.getDeviceInfo().maxWorkGroupSize);
            *dataParameterSimdSize = getKernelInfo().getMaxSimdSize();
            *prefferedWkgMultipleOffset = getKernelInfo().getMaxSimdSize();
            *parentEventOffset = WorkloadInfo::invalidParentEvent;
        }

        // allocate our own SSH, if necessary
        sshLocalSize = heapInfo.pKernelHeader
                           ? heapInfo.pKernelHeader->SurfaceStateHeapSize
                           : 0;

        if (sshLocalSize) {
            pSshLocal = new char[sshLocalSize];

            // copy the ssh into our local copy
            memcpy_s(pSshLocal, sshLocalSize, heapInfo.pSsh, sshLocalSize);
        }
        numberOfBindingTableStates = (patchInfo.bindingTableState != nullptr) ? patchInfo.bindingTableState->Count : 0;
        localBindingTableOffset = (patchInfo.bindingTableState != nullptr) ? patchInfo.bindingTableState->Offset : 0;

        // patch crossthread data and ssh with inline surfaces, if necessary
        privateSurfaceSize = patchInfo.pAllocateStatelessPrivateSurface
                                 ? patchInfo.pAllocateStatelessPrivateSurface->PerThreadPrivateMemorySize
                                 : 0;

        if (privateSurfaceSize) {
            privateSurfaceSize *= device.getDeviceInfo().computeUnitsUsedForScratch * getKernelInfo().getMaxSimdSize();
            DEBUG_BREAK_IF(privateSurfaceSize == 0);
            if ((is32Bit() || device.getMemoryManager()->peekForce32BitAllocations()) && (privateSurfaceSize > std::numeric_limits<uint32_t>::max())) {
                retVal = CL_OUT_OF_RESOURCES;
                break;
            }

            privateSurface = device.getMemoryManager()->createGraphicsAllocationWithRequiredBitness(static_cast<size_t>(privateSurfaceSize), nullptr);
            if (privateSurface == nullptr) {
                retVal = CL_OUT_OF_RESOURCES;
                break;
            }
            const auto &patch = patchInfo.pAllocateStatelessPrivateSurface;
            patchWithImplicitSurface(reinterpret_cast<void *>(privateSurface->getGpuAddressToPatch()), *privateSurface, *patch);
        }

        if (patchInfo.pAllocateStatelessConstantMemorySurfaceWithInitialization) {
            DEBUG_BREAK_IF(program->getConstantSurface() == nullptr);
            uintptr_t constMemory = isBuiltIn ? (uintptr_t)program->getConstantSurface()->getUnderlyingBuffer() : (uintptr_t)program->getConstantSurface()->getGpuAddressToPatch();

            const auto &patch = patchInfo.pAllocateStatelessConstantMemorySurfaceWithInitialization;
            patchWithImplicitSurface(reinterpret_cast<void *>(constMemory), *program->getConstantSurface(), *patch);
        }

        if (patchInfo.pAllocateStatelessGlobalMemorySurfaceWithInitialization) {
            DEBUG_BREAK_IF(program->getGlobalSurface() == nullptr);
            uintptr_t globalMemory = isBuiltIn ? (uintptr_t)program->getGlobalSurface()->getUnderlyingBuffer() : (uintptr_t)program->getGlobalSurface()->getGpuAddressToPatch();

            const auto &patch = patchInfo.pAllocateStatelessGlobalMemorySurfaceWithInitialization;
            patchWithImplicitSurface(reinterpret_cast<void *>(globalMemory), *program->getGlobalSurface(), *patch);
        }

        if (patchInfo.pAllocateStatelessEventPoolSurface) {
            if (requiresSshForBuffers()) {
                auto surfaceState = ptrOffset(reinterpret_cast<uintptr_t *>(getSurfaceStateHeap()),
                                              patchInfo.pAllocateStatelessEventPoolSurface->SurfaceStateHeapOffset);
                Buffer::setSurfaceState(&getContext(), surfaceState, 0, nullptr);
            }
        }

        if (patchInfo.pAllocateStatelessDefaultDeviceQueueSurface) {

            if (requiresSshForBuffers()) {
                auto surfaceState = ptrOffset(reinterpret_cast<uintptr_t *>(getSurfaceStateHeap()),
                                              patchInfo.pAllocateStatelessDefaultDeviceQueueSurface->SurfaceStateHeapOffset);
                Buffer::setSurfaceState(&getContext(), surfaceState, 0, nullptr);
            }
        }

        patchBlocksSimdSize();

        provideInitializationHints();
        // resolve the new kernel info to account for kernel handlers
        // I think by this time we have decoded the binary and know the number of args etc.
        // double check this assumption
        auto numArgs = kernelInfo.kernelArgInfo.size();
        kernelArguments.resize(numArgs);
        slmSizes.resize(numArgs);
        kernelArgHandlers.resize(numArgs);

        for (uint32_t i = 0; i < numArgs; ++i) {
            storeKernelArg(i, NONE_OBJ, nullptr, nullptr, 0);
            slmSizes[i] = 0;

            // set the argument handler
            auto &argInfo = kernelInfo.kernelArgInfo[i];
            if (argInfo.addressQualifier == CL_KERNEL_ARG_ADDRESS_LOCAL) {
                kernelArgHandlers[i] = &Kernel::setArgLocal;
            } else if (argInfo.isAccelerator) {
                kernelArgHandlers[i] = &Kernel::setArgAccelerator;
            } else if (argInfo.typeQualifierStr.find("pipe") != std::string::npos) {
                kernelArgHandlers[i] = &Kernel::setArgPipe;
                kernelArguments[i].type = PIPE_OBJ;
            } else if ((argInfo.typeStr.find("*") != std::string::npos) || argInfo.isBuffer) {
                kernelArgHandlers[i] = &Kernel::setArgBuffer;
                kernelArguments[i].type = BUFFER_OBJ;
            } else if (argInfo.isImage) {
                kernelArgHandlers[i] = &Kernel::setArgImage;
                kernelArguments[i].type = IMAGE_OBJ;
                DEBUG_BREAK_IF(argInfo.typeStr.find("image") == std::string::npos);
            } else if (argInfo.isSampler) {
                kernelArgHandlers[i] = &Kernel::setArgSampler;
                kernelArguments[i].type = SAMPLER_OBJ;
                DEBUG_BREAK_IF(!(*argInfo.typeStr.c_str() == '\0' || argInfo.typeStr.find("sampler") != std::string::npos));
            } else if (argInfo.isDeviceQueue) {
                kernelArgHandlers[i] = &Kernel::setArgDevQueue;
                kernelArguments[i].type = DEVICE_QUEUE_OBJ;
            } else {
                kernelArgHandlers[i] = &Kernel::setArgImmediate;
            }
        }

        if (isParentKernel) {
            program->allocateBlockPrivateSurfaces();
        }

        retVal = CL_SUCCESS;

    } while (false);

    return retVal;
}

cl_int Kernel::cloneKernel(Kernel *pSourceKernel) {
    // copy cross thread data to store arguments set to source kernel with clSetKernelArg on immediate data (non-pointer types)
    memcpy_s(crossThreadData, crossThreadDataSize, pSourceKernel->crossThreadData, pSourceKernel->crossThreadDataSize);
    DEBUG_BREAK_IF(pSourceKernel->crossThreadDataSize != crossThreadDataSize);

    // copy arguments set to source kernel with clSetKernelArg or clSetKernelArgSVMPointer
    for (uint32_t i = 0; i < pSourceKernel->kernelArguments.size(); i++) {
        if (0 == pSourceKernel->getKernelArgInfo(i).size) {
            // skip copying arguments that haven't been set to source kernel
            continue;
        }
        switch (pSourceKernel->kernelArguments[i].type) {
        case NONE_OBJ:
            // all arguments with immediate data (non-pointer types) have been copied in cross thread data
            storeKernelArg(i, NONE_OBJ, nullptr, nullptr, pSourceKernel->getKernelArgInfo(i).size);
            patchedArgumentsNum++;
            kernelArguments[i].isPatched = true;
            break;
        case SVM_OBJ:
            setArgSvm(i, pSourceKernel->getKernelArgInfo(i).size, const_cast<void *>(pSourceKernel->getKernelArgInfo(i).value),
                      pSourceKernel->getKernelArgInfo(i).pSvmAlloc, pSourceKernel->getKernelArgInfo(i).svmFlags);
            break;
        case SVM_ALLOC_OBJ:
            setArgSvmAlloc(i, const_cast<void *>(pSourceKernel->getKernelArgInfo(i).value),
                           (GraphicsAllocation *)pSourceKernel->getKernelArgInfo(i).object);
            break;
        default:
            setArg(i, pSourceKernel->getKernelArgInfo(i).size, pSourceKernel->getKernelArgInfo(i).value);
            break;
        }
    }

    // copy additional information other than argument values set to source kernel with clSetKernelExecInfo
    for (auto gfxAlloc : pSourceKernel->kernelSvmGfxAllocations) {
        kernelSvmGfxAllocations.push_back(gfxAlloc);
    }

    return CL_SUCCESS;
}

cl_int Kernel::getInfo(cl_kernel_info paramName, size_t paramValueSize,
                       void *paramValue, size_t *paramValueSizeRet) const {
    cl_int retVal;
    const void *pSrc = nullptr;
    size_t srcSize = 0;
    cl_uint numArgs = 0;
    const _cl_program *prog;
    const _cl_context *ctxt;
    cl_uint refCount = 0;

    switch (paramName) {
    case CL_KERNEL_FUNCTION_NAME:
        pSrc = kernelInfo.name.c_str();
        srcSize = kernelInfo.name.length() + 1;
        break;

    case CL_KERNEL_NUM_ARGS:
        srcSize = sizeof(cl_uint);
        numArgs = (cl_uint)kernelInfo.kernelArgInfo.size();
        pSrc = &numArgs;
        break;

    case CL_KERNEL_CONTEXT:
        ctxt = &program->getContext();
        srcSize = sizeof(ctxt);
        pSrc = &ctxt;
        break;

    case CL_KERNEL_PROGRAM:
        prog = program;
        srcSize = sizeof(prog);
        pSrc = &prog;
        break;

    case CL_KERNEL_REFERENCE_COUNT:
        refCount = static_cast<cl_uint>(this->getReference());
        srcSize = sizeof(refCount);
        pSrc = &refCount;
        break;

    case CL_KERNEL_ATTRIBUTES:
        pSrc = kernelInfo.attributes.c_str();
        srcSize = kernelInfo.attributes.length() + 1;
        break;

    case CL_KERNEL_BINARY_PROGRAM_INTEL:
        pSrc = getKernelHeap();
        srcSize = getKernelHeapSize();
        break;

    default:
        break;
    }

    retVal = ::getInfo(paramValue, paramValueSize, pSrc, srcSize);

    if (paramValueSizeRet) {
        *paramValueSizeRet = srcSize;
    }

    return retVal;
}

cl_int Kernel::getArgInfo(cl_uint argIndx, cl_kernel_arg_info paramName, size_t paramValueSize,
                          void *paramValue, size_t *paramValueSizeRet) const {
    cl_int retVal;
    const void *pSrc = nullptr;
    size_t srcSize = 0;
    auto numArgs = (cl_uint)kernelInfo.kernelArgInfo.size();
    auto argInfoIdx = kernelInfo.kernelArgInfo[argIndx];

    if (argIndx >= numArgs) {
        retVal = CL_INVALID_ARG_INDEX;
        return retVal;
    }

    switch (paramName) {
    case CL_KERNEL_ARG_ADDRESS_QUALIFIER:
        srcSize = sizeof(cl_uint);
        pSrc = &argInfoIdx.addressQualifier;
        break;

    case CL_KERNEL_ARG_ACCESS_QUALIFIER:
        srcSize = sizeof(cl_uint);
        pSrc = &argInfoIdx.accessQualifier;
        break;

    case CL_KERNEL_ARG_TYPE_NAME:
        srcSize = argInfoIdx.typeStr.length() + 1;
        pSrc = argInfoIdx.typeStr.c_str();
        break;

    case CL_KERNEL_ARG_TYPE_QUALIFIER:
        srcSize = sizeof(argInfoIdx.typeQualifier);
        pSrc = &argInfoIdx.typeQualifier;
        break;

    case CL_KERNEL_ARG_NAME:
        srcSize = argInfoIdx.name.length() + 1;
        pSrc = argInfoIdx.name.c_str();
        break;

    default:
        break;
    }

    retVal = ::getInfo(paramValue, paramValueSize, pSrc, srcSize);

    if (paramValueSizeRet) {
        *paramValueSizeRet = srcSize;
    }

    return retVal;
}

cl_int Kernel::getWorkGroupInfo(cl_device_id device, cl_kernel_work_group_info paramName,
                                size_t paramValueSize, void *paramValue,
                                size_t *paramValueSizeRet) const {
    cl_int retVal = CL_INVALID_VALUE;
    struct size_t3 {
        size_t val[3];
    } requiredWorkGroupSize;
    cl_ulong localMemorySize;
    const auto &patchInfo = kernelInfo.patchInfo;
    size_t preferredWorkGroupSizeMultiple = 0;
    cl_ulong scratchSize;
    cl_ulong privateMemSize;
    size_t maxWorkgroupSize;

    GetInfoHelper info(paramValue, paramValueSize, paramValueSizeRet);

    switch (paramName) {
    case CL_KERNEL_WORK_GROUP_SIZE:
        maxWorkgroupSize = this->device.getDeviceInfo().maxWorkGroupSize;
        if (DebugManager.flags.UseMaxSimdSizeToDeduceMaxWorkgroupSize.get()) {
            auto divisionSize = 32 / patchInfo.executionEnvironment->LargestCompiledSIMDSize;
            maxWorkgroupSize /= divisionSize;
        }
        retVal = info.set<size_t>(maxWorkgroupSize);
        break;

    case CL_KERNEL_COMPILE_WORK_GROUP_SIZE:
        DEBUG_BREAK_IF(!patchInfo.executionEnvironment);
        requiredWorkGroupSize.val[0] = patchInfo.executionEnvironment->RequiredWorkGroupSizeX;
        requiredWorkGroupSize.val[1] = patchInfo.executionEnvironment->RequiredWorkGroupSizeY;
        requiredWorkGroupSize.val[2] = patchInfo.executionEnvironment->RequiredWorkGroupSizeZ;
        retVal = info.set<size_t3>(requiredWorkGroupSize);
        break;

    case CL_KERNEL_LOCAL_MEM_SIZE:
        localMemorySize = patchInfo.localsurface
                              ? patchInfo.localsurface->TotalInlineLocalMemorySize
                              : 0;
        retVal = info.set<cl_ulong>(localMemorySize);
        break;

    case CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE:
        DEBUG_BREAK_IF(!patchInfo.executionEnvironment);
        preferredWorkGroupSizeMultiple = patchInfo.executionEnvironment->LargestCompiledSIMDSize;
        retVal = info.set<size_t>(preferredWorkGroupSizeMultiple);
        break;

    case CL_KERNEL_SPILL_MEM_SIZE_INTEL:
        scratchSize = kernelInfo.patchInfo.mediavfestate ? kernelInfo.patchInfo.mediavfestate->PerThreadScratchSpace : 0;
        retVal = info.set<cl_ulong>(scratchSize);
        break;
    case CL_KERNEL_PRIVATE_MEM_SIZE:
        privateMemSize = kernelInfo.patchInfo.pAllocateStatelessPrivateSurface ? kernelInfo.patchInfo.pAllocateStatelessPrivateSurface->PerThreadPrivateMemorySize : 0;
        retVal = info.set<cl_ulong>(privateMemSize);
        break;
    default:
        retVal = CL_INVALID_VALUE;
        break;
    }

    return retVal;
}

cl_int Kernel::getSubGroupInfo(cl_kernel_sub_group_info paramName,
                               size_t inputValueSize, const void *inputValue,
                               size_t paramValueSize, void *paramValue,
                               size_t *paramValueSizeRet) const {
    size_t numDimensions = 0;
    size_t WGS = 1;
    auto maxSimdSize = static_cast<size_t>(getKernelInfo().getMaxSimdSize());
    auto maxRequiredWorkGroupSize = static_cast<size_t>(getKernelInfo().getMaxRequiredWorkGroupSize(device.getDeviceInfo().maxWorkGroupSize));
    auto largestCompiledSIMDSize = static_cast<size_t>(getKernelInfo().patchInfo.executionEnvironment->LargestCompiledSIMDSize);

    GetInfoHelper info(paramValue, paramValueSize, paramValueSizeRet);

    if ((paramName == CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT) ||
        (paramName == CL_KERNEL_MAX_NUM_SUB_GROUPS) ||
        (paramName == CL_KERNEL_COMPILE_NUM_SUB_GROUPS)) {
        if (device.getEnabledClVersion() < 21) {
            return CL_INVALID_VALUE;
        }
    }

    if ((paramName == CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE_KHR) ||
        (paramName == CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE_KHR)) {
        if (!inputValue) {
            return CL_INVALID_VALUE;
        }
        if (inputValueSize % sizeof(size_t) != 0) {
            return CL_INVALID_VALUE;
        }
        numDimensions = inputValueSize / sizeof(size_t);
        if (numDimensions == 0 ||
            numDimensions > static_cast<size_t>(device.getDeviceInfo().maxWorkItemDimensions)) {
            return CL_INVALID_VALUE;
        }
    }

    if (paramName == CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT) {
        if (!paramValue) {
            return CL_INVALID_VALUE;
        }
        if (paramValueSize % sizeof(size_t) != 0) {
            return CL_INVALID_VALUE;
        }
        numDimensions = paramValueSize / sizeof(size_t);
        if (numDimensions == 0 ||
            numDimensions > static_cast<size_t>(device.getDeviceInfo().maxWorkItemDimensions)) {
            return CL_INVALID_VALUE;
        }
    }

    switch (paramName) {
    case CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE_KHR: {
        for (size_t i = 0; i < numDimensions; i++) {
            WGS *= ((size_t *)inputValue)[i];
        }
        return info.set<size_t>(std::min(WGS, maxSimdSize));
    }
    case CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE_KHR: {
        for (size_t i = 0; i < numDimensions; i++) {
            WGS *= ((size_t *)inputValue)[i];
        }
        return info.set<size_t>((WGS / maxSimdSize) +
                                std::min(static_cast<size_t>(1), WGS % maxSimdSize)); // add 1 if WGS % maxSimdSize != 0
    }
    case CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT: {
        auto subGroupsNum = *(size_t *)inputValue;
        auto workGroupSize = subGroupsNum * largestCompiledSIMDSize;
        // return workgroup size in first dimension, the rest shall be 1 in positive case
        if (workGroupSize > maxRequiredWorkGroupSize) {
            workGroupSize = 0;
        }
        // If no work group size can accommodate the requested number of subgroups, return 0 in each element of the returned array.
        switch (numDimensions) {
        case 1:
            return info.set<size_t>(workGroupSize);
        case 2:
            struct size_t2 {
                size_t val[2];
            } workGroupSize2;
            workGroupSize2.val[0] = workGroupSize;
            workGroupSize2.val[1] = (workGroupSize > 0) ? 1 : 0;
            return info.set<size_t2>(workGroupSize2);
        case 3:
        default:
            struct size_t3 {
                size_t val[3];
            } workGroupSize3;
            workGroupSize3.val[0] = workGroupSize;
            workGroupSize3.val[1] = (workGroupSize > 0) ? 1 : 0;
            workGroupSize3.val[2] = (workGroupSize > 0) ? 1 : 0;
            return info.set<size_t3>(workGroupSize3);
        }
    }
    case CL_KERNEL_MAX_NUM_SUB_GROUPS: {
        // round-up maximum number of subgroups
        return info.set<size_t>((maxRequiredWorkGroupSize + largestCompiledSIMDSize - 1) / largestCompiledSIMDSize);
    }
    case CL_KERNEL_COMPILE_NUM_SUB_GROUPS: {
        return info.set<size_t>(static_cast<size_t>(getKernelInfo().patchInfo.executionEnvironment->CompiledSubGroupsNumber));
    }
    case CL_KERNEL_COMPILE_SUB_GROUP_SIZE_INTEL: {
        return info.set<size_t>(getKernelInfo().requiredSubGroupSize);
    }
    default:
        return CL_INVALID_VALUE;
    }
}

const void *Kernel::getKernelHeap() const {
    return kernelInfo.heapInfo.pKernelHeap;
}

size_t Kernel::getKernelHeapSize() const {
    return kernelInfo.heapInfo.pKernelHeader->KernelHeapSize;
}

void Kernel::substituteKernelHeap(void *newKernelHeap, size_t newKernelHeapSize) {
    KernelInfo *pKernelInfo = const_cast<KernelInfo *>(&kernelInfo);
    void **pKernelHeap = const_cast<void **>(&pKernelInfo->heapInfo.pKernelHeap);
    *pKernelHeap = newKernelHeap;
    SKernelBinaryHeaderCommon *pHeader = const_cast<SKernelBinaryHeaderCommon *>(pKernelInfo->heapInfo.pKernelHeader);
    pHeader->KernelHeapSize = static_cast<uint32_t>(newKernelHeapSize);
    pKernelInfo->isKernelHeapSubstituted = true;

    auto currentAllocationSize = pKernelInfo->kernelAllocation->getUnderlyingBufferSize();
    if (currentAllocationSize >= newKernelHeapSize) {
        memcpy_s(pKernelInfo->kernelAllocation->getUnderlyingBuffer(), newKernelHeapSize, newKernelHeap, newKernelHeapSize);
    } else {
        auto memoryManager = device.getMemoryManager();
        memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(pKernelInfo->kernelAllocation);
        pKernelInfo->kernelAllocation = nullptr;
        pKernelInfo->createKernelAllocation(memoryManager);
    }
}

bool Kernel::isKernelHeapSubstituted() const {
    return kernelInfo.isKernelHeapSubstituted;
}

uint64_t Kernel::getKernelId() const {
    return kernelInfo.kernelId;
}

void Kernel::setKernelId(uint64_t newKernelId) {
    KernelInfo *pKernelInfo = const_cast<KernelInfo *>(&kernelInfo);
    pKernelInfo->kernelId = newKernelId;
}

const void *Kernel::getSurfaceStateHeap() const {
    return kernelInfo.usesSsh
               ? pSshLocal
               : nullptr;
}

void *Kernel::getSurfaceStateHeap() {
    return const_cast<void *>(const_cast<const Kernel *>(this)->getSurfaceStateHeap());
}

size_t Kernel::getDynamicStateHeapSize() const {
    return kernelInfo.heapInfo.pKernelHeader->DynamicStateHeapSize;
}

const void *Kernel::getDynamicStateHeap() const {
    return kernelInfo.heapInfo.pDsh;
}

size_t Kernel::getSurfaceStateHeapSize() const {
    return kernelInfo.usesSsh
               ? sshLocalSize
               : 0;
}

size_t Kernel::getNumberOfBindingTableStates() const {
    return numberOfBindingTableStates;
}

void Kernel::resizeSurfaceStateHeap(void *pNewSsh, size_t newSshSize, size_t newBindingTableCount, size_t newBindingTableOffset) {
    delete[] pSshLocal;
    pSshLocal = reinterpret_cast<char *>(pNewSsh);
    sshLocalSize = static_cast<uint32_t>(newSshSize);
    numberOfBindingTableStates = newBindingTableCount;
    localBindingTableOffset = newBindingTableOffset;
}

uint32_t Kernel::getScratchSizeValueToProgramMediaVfeState(int scratchSize) {
    scratchSize >>= MemoryConstants::kiloByteShiftSize;
    uint32_t valueToProgram = 0;
    while (scratchSize >>= 1) {
        valueToProgram++;
    }
    return valueToProgram;
}

cl_int Kernel::setArg(uint32_t argIndex, size_t argSize, const void *argVal) {
    cl_int retVal = CL_SUCCESS;
    bool updateExposedKernel = true;
    if (getKernelInfo().builtinDispatchBuilder != nullptr) {
        updateExposedKernel = getKernelInfo().builtinDispatchBuilder->setExplicitArg(argIndex, argSize, argVal, retVal);
    }
    if (updateExposedKernel) {
        if (argIndex >= kernelArgHandlers.size()) {
            return CL_INVALID_ARG_INDEX;
        }
        auto argHandler = kernelArgHandlers[argIndex];
        retVal = (this->*argHandler)(argIndex, argSize, argVal);
    }
    if (retVal == CL_SUCCESS) {
        if (!kernelArguments[argIndex].isPatched) {
            patchedArgumentsNum++;
            kernelArguments[argIndex].isPatched = true;
        }
        resolveArgs();
    }
    return retVal;
}

cl_int Kernel::setArg(uint32_t argIndex, uint32_t argVal) {
    return setArg(argIndex, sizeof(argVal), &argVal);
}

cl_int Kernel::setArg(uint32_t argIndex, cl_mem argVal) {
    return setArg(argIndex, sizeof(argVal), &argVal);
}

cl_int Kernel::setArg(uint32_t argIndex, cl_mem argVal, uint32_t mipLevel) {
    return setArgImageWithMipLevel(argIndex, sizeof(argVal), &argVal, mipLevel);
}

void *Kernel::patchBufferOffset(const KernelArgInfo &argInfo, void *svmPtr, GraphicsAllocation *svmAlloc) {
    if (isInvalidOffset(argInfo.offsetBufferOffset)) {
        return svmPtr;
    }

    void *ptrToPatch = svmPtr;
    if (svmAlloc != nullptr) {
        ptrToPatch = svmAlloc->getUnderlyingBuffer();
    }

    constexpr uint32_t minimumAlignment = 4;
    ptrToPatch = alignDown(ptrToPatch, minimumAlignment);
    DEBUG_BREAK_IF(ptrDiff(svmPtr, ptrToPatch) != static_cast<uint32_t>(ptrDiff(svmPtr, ptrToPatch)));
    uint32_t offsetToPatch = static_cast<uint32_t>(ptrDiff(svmPtr, ptrToPatch));

    patch<uint32_t, uint32_t>(offsetToPatch, getCrossThreadData(), argInfo.offsetBufferOffset);
    return ptrToPatch;
}

cl_int Kernel::setArgSvm(uint32_t argIndex, size_t svmAllocSize, void *svmPtr, GraphicsAllocation *svmAlloc, cl_mem_flags svmFlags) {
    void *ptrToPatch = patchBufferOffset(kernelInfo.kernelArgInfo[argIndex], svmPtr, svmAlloc);
    setArgImmediate(argIndex, sizeof(void *), &svmPtr);

    storeKernelArg(argIndex, SVM_OBJ, nullptr, svmPtr, sizeof(void *), svmAlloc, svmFlags);

    if (requiresSshForBuffers()) {
        const auto &kernelArgInfo = kernelInfo.kernelArgInfo[argIndex];
        auto surfaceState = ptrOffset(getSurfaceStateHeap(), kernelArgInfo.offsetHeap);
        Buffer::setSurfaceState(&getContext(), surfaceState, svmAllocSize + ptrDiff(svmPtr, ptrToPatch), ptrToPatch, svmAlloc, svmFlags);
    }
    if (!kernelArguments[argIndex].isPatched) {
        patchedArgumentsNum++;
        kernelArguments[argIndex].isPatched = true;
    }
    return CL_SUCCESS;
}

cl_int Kernel::setArgSvmAlloc(uint32_t argIndex, void *svmPtr, GraphicsAllocation *svmAlloc) {
    DBG_LOG_INPUTS("setArgBuffer svm_alloc", svmAlloc);

    const auto &kernelArgInfo = kernelInfo.kernelArgInfo[argIndex];

    storeKernelArg(argIndex, SVM_ALLOC_OBJ, svmAlloc, svmPtr, sizeof(uintptr_t));

    void *ptrToPatch = patchBufferOffset(kernelArgInfo, svmPtr, svmAlloc);

    auto patchLocation = ptrOffset(getCrossThreadData(),
                                   kernelArgInfo.kernelArgPatchInfoVector[0].crossthreadOffset);

    auto patchSize = kernelArgInfo.kernelArgPatchInfoVector[0].size;

    patchWithRequiredSize(patchLocation, patchSize, reinterpret_cast<uintptr_t>(svmPtr));

    if (requiresSshForBuffers()) {
        const auto &kernelArgInfo = kernelInfo.kernelArgInfo[argIndex];
        auto surfaceState = ptrOffset(getSurfaceStateHeap(), kernelArgInfo.offsetHeap);
        size_t allocSize = 0;
        if (svmAlloc != nullptr) {
            allocSize = svmAlloc->getUnderlyingBufferSize();
            size_t offset = ptrDiff(ptrToPatch, svmAlloc->getUnderlyingBuffer());
            allocSize -= offset;
        }
        Buffer::setSurfaceState(&getContext(), surfaceState, allocSize, ptrToPatch, nullptr);
    }

    if (!kernelArguments[argIndex].isPatched) {
        patchedArgumentsNum++;
        kernelArguments[argIndex].isPatched = true;
    }
    return CL_SUCCESS;
}

void Kernel::storeKernelArg(uint32_t argIndex, kernelArgType argType, const void *argObject,
                            const void *argValue, size_t argSize,
                            GraphicsAllocation *argSvmAlloc, cl_mem_flags argSvmFlags) {
    kernelArguments[argIndex].type = argType;
    kernelArguments[argIndex].object = argObject;
    kernelArguments[argIndex].value = argValue;
    kernelArguments[argIndex].size = argSize;
    kernelArguments[argIndex].pSvmAlloc = argSvmAlloc;
    kernelArguments[argIndex].svmFlags = argSvmFlags;
}

const void *Kernel::getKernelArg(uint32_t argIndex) const {
    return kernelArguments[argIndex].object;
}

const Kernel::SimpleKernelArgInfo &Kernel::getKernelArgInfo(uint32_t argIndex) const {
    return kernelArguments[argIndex];
}

void Kernel::setKernelExecInfo(GraphicsAllocation *argValue) {
    kernelSvmGfxAllocations.push_back(argValue);
}

void Kernel::clearKernelExecInfo() {
    kernelSvmGfxAllocations.clear();
}

inline void Kernel::makeArgsResident(CommandStreamReceiver &commandStreamReceiver) {

    auto numArgs = kernelInfo.kernelArgInfo.size();
    for (decltype(numArgs) argIndex = 0; argIndex < numArgs; argIndex++) {
        if (kernelArguments[argIndex].object) {
            if (kernelArguments[argIndex].type == SVM_ALLOC_OBJ) {
                auto pSVMAlloc = (GraphicsAllocation *)kernelArguments[argIndex].object;
                commandStreamReceiver.makeResident(*pSVMAlloc);
            } else if (Kernel::isMemObj(kernelArguments[argIndex].type)) {
                auto clMem = const_cast<cl_mem>(static_cast<const _cl_mem *>(kernelArguments[argIndex].object));
                auto memObj = castToObjectOrAbort<MemObj>(clMem);
                DEBUG_BREAK_IF(memObj == nullptr);
                if (memObj->isImageFromImage()) {
                    commandStreamReceiver.setSamplerCacheFlushRequired(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushBefore);
                }
                commandStreamReceiver.makeResident(*memObj->getGraphicsAllocation());
                if (memObj->getMcsAllocation()) {
                    commandStreamReceiver.makeResident(*memObj->getMcsAllocation());
                }
            }
        }
    }
}

void Kernel::updateWithCompletionStamp(CommandStreamReceiver &commandStreamReceiver, CompletionStamp *completionStamp) {
    auto numArgs = kernelInfo.kernelArgInfo.size();
    for (decltype(numArgs) argIndex = 0; argIndex < numArgs; argIndex++) {
        if (kernelArguments[argIndex].object) {
            if (Kernel::isMemObj(kernelArguments[argIndex].type)) {
                auto clMem = const_cast<cl_mem>(static_cast<const _cl_mem *>(kernelArguments[argIndex].object));
                auto memObj = castToObjectOrAbort<MemObj>(clMem);
                DEBUG_BREAK_IF(memObj == nullptr);
                memObj->setCompletionStamp(*completionStamp, nullptr, nullptr);
            }
        }
    }
}

void Kernel::makeResident(CommandStreamReceiver &commandStreamReceiver) {
    if (privateSurface) {
        commandStreamReceiver.makeResident(*privateSurface);
    }

    if (program->getConstantSurface()) {
        commandStreamReceiver.makeResident(*(program->getConstantSurface()));
    }

    if (program->getGlobalSurface()) {
        commandStreamReceiver.makeResident(*(program->getGlobalSurface()));
    }

    for (auto gfxAlloc : kernelSvmGfxAllocations) {
        commandStreamReceiver.makeResident(*gfxAlloc);
    }

    makeArgsResident(commandStreamReceiver);

    auto kernelIsaAllocation = this->kernelInfo.kernelAllocation;
    if (kernelIsaAllocation) {
        commandStreamReceiver.makeResident(*kernelIsaAllocation);
    }

    gtpinNotifyMakeResident(this, &commandStreamReceiver);
}

void Kernel::getResidency(std::vector<Surface *> &dst) {
    if (privateSurface) {
        GeneralSurface *surface = new GeneralSurface(privateSurface);
        dst.push_back(surface);
    }

    if (program->getConstantSurface()) {
        GeneralSurface *surface = new GeneralSurface(program->getConstantSurface());
        dst.push_back(surface);
    }

    if (program->getGlobalSurface()) {
        GeneralSurface *surface = new GeneralSurface(program->getGlobalSurface());
        dst.push_back(surface);
    }

    for (auto gfxAlloc : kernelSvmGfxAllocations) {
        GeneralSurface *surface = new GeneralSurface(gfxAlloc);
        dst.push_back(surface);
    }

    auto numArgs = kernelInfo.kernelArgInfo.size();
    for (decltype(numArgs) argIndex = 0; argIndex < numArgs; argIndex++) {
        if (kernelArguments[argIndex].object) {
            if (kernelArguments[argIndex].type == SVM_ALLOC_OBJ) {
                auto pSVMAlloc = (GraphicsAllocation *)kernelArguments[argIndex].object;
                dst.push_back(new GeneralSurface(pSVMAlloc));
            } else if (Kernel::isMemObj(kernelArguments[argIndex].type)) {
                auto clMem = const_cast<cl_mem>(static_cast<const _cl_mem *>(kernelArguments[argIndex].object));
                auto memObj = castToObject<MemObj>(clMem);
                DEBUG_BREAK_IF(memObj == nullptr);
                dst.push_back(new MemObjSurface(memObj));
            }
        }
    }

    auto kernelIsaAllocation = this->kernelInfo.kernelAllocation;
    if (kernelIsaAllocation) {
        GeneralSurface *surface = new GeneralSurface(kernelIsaAllocation);
        dst.push_back(surface);
    }

    gtpinNotifyUpdateResidencyList(this, &dst);
}

bool Kernel::requiresCoherency() {
    auto numArgs = kernelInfo.kernelArgInfo.size();
    for (decltype(numArgs) argIndex = 0; argIndex < numArgs; argIndex++) {
        if (kernelArguments[argIndex].object) {
            if (kernelArguments[argIndex].type == SVM_ALLOC_OBJ) {
                auto pSVMAlloc = (GraphicsAllocation *)kernelArguments[argIndex].object;
                if (pSVMAlloc->isCoherent()) {
                    return true;
                }
            }

            if (Kernel::isMemObj(kernelArguments[argIndex].type)) {
                auto clMem = const_cast<cl_mem>(static_cast<const _cl_mem *>(kernelArguments[argIndex].object));
                auto memObj = castToObjectOrAbort<MemObj>(clMem);
                if (memObj->getGraphicsAllocation()->isCoherent()) {
                    return true;
                }
            }
        }
    }
    return false;
}

cl_int Kernel::setArgLocal(uint32_t argIndex,
                           size_t argSize,
                           const void *argVal) {
    auto crossThreadData = reinterpret_cast<uint32_t *>(getCrossThreadData());

    storeKernelArg(argIndex, SLM_OBJ, nullptr, argVal, argSize);

    slmSizes[argIndex] = argSize;

    // Extract our current slmOffset
    auto slmOffset = *ptrOffset(crossThreadData,
                                kernelInfo.kernelArgInfo[argIndex].kernelArgPatchInfoVector[0].crossthreadOffset);

    // Add our size
    slmOffset += static_cast<uint32_t>(argSize);

    // Update all slm offsets after this argIndex
    ++argIndex;
    while (argIndex < slmSizes.size()) {
        const auto &kernelArgInfo = kernelInfo.kernelArgInfo[argIndex];
        auto slmAlignment = kernelArgInfo.slmAlignment;

        // If an local argument, alignment should be non-zero
        if (slmAlignment) {
            // Align to specified alignment
            slmOffset = alignUp(slmOffset, slmAlignment);

            // Patch our new offset into cross thread data
            auto patchLocation = ptrOffset(crossThreadData,
                                           kernelArgInfo.kernelArgPatchInfoVector[0].crossthreadOffset);
            *patchLocation = slmOffset;
        }

        slmOffset += static_cast<uint32_t>(slmSizes[argIndex]);
        ++argIndex;
    }

    slmTotalSize = kernelInfo.workloadInfo.slmStaticSize + alignUp(slmOffset, KB);

    return CL_SUCCESS;
}

cl_int Kernel::setArgBuffer(uint32_t argIndex,
                            size_t argSize,
                            const void *argVal) {

    if (argSize != sizeof(cl_mem *))
        return CL_INVALID_ARG_VALUE;

    const auto &kernelArgInfo = kernelInfo.kernelArgInfo[argIndex];
    auto clMem = reinterpret_cast<const cl_mem *>(argVal);
    patchBufferOffset(kernelArgInfo, nullptr, nullptr);

    if (clMem && *clMem) {
        auto clMemObj = *clMem;
        DBG_LOG_INPUTS("setArgBuffer cl_mem", clMemObj);

        storeKernelArg(argIndex, BUFFER_OBJ, clMemObj, argVal, argSize);

        auto buffer = castToObject<Buffer>(clMemObj);
        if (!buffer)
            return CL_INVALID_MEM_OBJECT;

        if (buffer->peekSharingHandler()) {
            usingSharedObjArgs = true;
        }

        auto patchLocation = ptrOffset(getCrossThreadData(),
                                       kernelArgInfo.kernelArgPatchInfoVector[0].crossthreadOffset);

        auto patchSize = kernelArgInfo.kernelArgPatchInfoVector[0].size;

        uint64_t addressToPatch = buffer->setArgStateless(patchLocation, patchSize, !this->isBuiltIn);

        if (DebugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
            PatchInfoData patchInfoData(addressToPatch - buffer->getOffset(), static_cast<uint64_t>(buffer->getOffset()), PatchInfoAllocationType::KernelArg, reinterpret_cast<uint64_t>(getCrossThreadData()), static_cast<uint64_t>(kernelArgInfo.kernelArgPatchInfoVector[0].crossthreadOffset), PatchInfoAllocationType::IndirectObjectHeap, patchSize);
            this->patchInfoDataList.push_back(patchInfoData);
        }

        if (requiresSshForBuffers()) {
            auto surfaceState = ptrOffset(getSurfaceStateHeap(), kernelArgInfo.offsetHeap);
            buffer->setArgStateful(const_cast<void *>(surfaceState));
        }

        return CL_SUCCESS;
    } else {

        auto patchLocation = ptrOffset(getCrossThreadData(),
                                       kernelArgInfo.kernelArgPatchInfoVector[0].crossthreadOffset);

        patchWithRequiredSize(patchLocation, kernelArgInfo.kernelArgPatchInfoVector[0].size, 0u);

        storeKernelArg(argIndex, BUFFER_OBJ, nullptr, argVal, argSize);

        if (requiresSshForBuffers()) {
            auto surfaceState = ptrOffset(getSurfaceStateHeap(), kernelArgInfo.offsetHeap);
            Buffer::setSurfaceState(&getContext(), surfaceState, 0, nullptr);
        }

        return CL_SUCCESS;
    }
}

cl_int Kernel::setArgPipe(uint32_t argIndex,
                          size_t argSize,
                          const void *argVal) {

    if (argSize != sizeof(cl_mem *)) {
        return CL_INVALID_ARG_SIZE;
    }

    const auto &kernelArgInfo = kernelInfo.kernelArgInfo[argIndex];
    auto clMem = reinterpret_cast<const cl_mem *>(argVal);

    if (clMem && *clMem) {
        auto clMemObj = *clMem;
        DBG_LOG_INPUTS("setArgPipe cl_mem", clMemObj);

        storeKernelArg(argIndex, PIPE_OBJ, clMemObj, argVal, argSize);

        auto memObj = castToObject<MemObj>(clMemObj);
        if (!memObj) {
            return CL_INVALID_MEM_OBJECT;
        }

        auto pipe = castToObject<Pipe>(clMemObj);
        if (!pipe) {
            return CL_INVALID_ARG_VALUE;
        }

        if (memObj->getContext() != &(this->getContext())) {
            return CL_INVALID_MEM_OBJECT;
        }

        auto patchLocation = ptrOffset(getCrossThreadData(),
                                       kernelArgInfo.kernelArgPatchInfoVector[0].crossthreadOffset);

        auto patchSize = kernelArgInfo.kernelArgPatchInfoVector[0].size;

        pipe->setPipeArg(patchLocation, patchSize);

        if (requiresSshForBuffers()) {
            auto surfaceState = ptrOffset(getSurfaceStateHeap(), kernelArgInfo.offsetHeap);
            Buffer::setSurfaceState(&getContext(), surfaceState,
                                    pipe->getSize(), pipe->getCpuAddress(),
                                    pipe->getGraphicsAllocation());
        }

        return CL_SUCCESS;
    } else {
        return CL_INVALID_MEM_OBJECT;
    }
}

cl_int Kernel::setArgImage(uint32_t argIndex,
                           size_t argSize,
                           const void *argVal) {
    return setArgImageWithMipLevel(argIndex, argSize, argVal, 0u);
}

cl_int Kernel::setArgImageWithMipLevel(uint32_t argIndex,
                                       size_t argSize,
                                       const void *argVal, uint32_t mipLevel) {
    auto retVal = CL_INVALID_ARG_VALUE;
    patchBufferOffset(kernelInfo.kernelArgInfo[argIndex], nullptr, nullptr);

    auto clMemObj = *(static_cast<const cl_mem *>(argVal));
    auto pImage = castToObject<Image>(clMemObj);

    if (pImage && argSize == sizeof(cl_mem *)) {
        if (pImage->peekSharingHandler()) {
            usingSharedObjArgs = true;
        }
        const auto &kernelArgInfo = kernelInfo.kernelArgInfo[argIndex];

        DBG_LOG_INPUTS("setArgImage cl_mem", clMemObj);

        storeKernelArg(argIndex, IMAGE_OBJ, clMemObj, argVal, argSize);

        auto surfaceState = ptrOffset(getSurfaceStateHeap(), kernelArgInfo.offsetHeap);
        DEBUG_BREAK_IF(!kernelArgInfo.isImage);

        // Sets SS structure
        if (kernelArgInfo.isMediaImage) {
            DEBUG_BREAK_IF(!kernelInfo.isVmeWorkload);
            pImage->setMediaImageArg(const_cast<void *>(surfaceState));
        } else {
            pImage->setImageArg(const_cast<void *>(surfaceState), kernelArgInfo.isMediaBlockImage, mipLevel);
        }

        auto crossThreadData = reinterpret_cast<uint32_t *>(getCrossThreadData());
        auto &imageDesc = pImage->getImageDesc();
        auto &imageFormat = pImage->getImageFormat();

        if (imageDesc.image_type == CL_MEM_OBJECT_IMAGE3D) {
            imageTransformer->registerImage3d(argIndex);
        }

        patch<uint32_t, size_t>(imageDesc.image_width, crossThreadData, kernelArgInfo.offsetImgWidth);
        patch<uint32_t, size_t>(imageDesc.image_height, crossThreadData, kernelArgInfo.offsetImgHeight);
        patch<uint32_t, size_t>(imageDesc.image_depth, crossThreadData, kernelArgInfo.offsetImgDepth);
        patch<uint32_t, cl_uint>(imageDesc.num_samples, crossThreadData, kernelArgInfo.offsetNumSamples);
        patch<uint32_t, size_t>(imageDesc.image_array_size, crossThreadData, kernelArgInfo.offsetArraySize);
        patch<uint32_t, cl_channel_type>(imageFormat.image_channel_data_type, crossThreadData, kernelArgInfo.offsetChannelDataType);
        patch<uint32_t, cl_channel_order>(imageFormat.image_channel_order, crossThreadData, kernelArgInfo.offsetChannelOrder);
        patch<uint32_t, uint32_t>(kernelArgInfo.offsetHeap, crossThreadData, kernelArgInfo.offsetObjectId);
        patch<uint32_t, cl_uint>(imageDesc.num_mip_levels, crossThreadData, kernelArgInfo.offsetNumMipLevels);

        retVal = CL_SUCCESS;
    }

    return retVal;
}

cl_int Kernel::setArgImmediate(uint32_t argIndex,
                               size_t argSize,
                               const void *argVal) {

    auto retVal = CL_INVALID_ARG_VALUE;

    if (argVal) {
        const auto &kernelArgInfo = kernelInfo.kernelArgInfo[argIndex];
        DEBUG_BREAK_IF(kernelArgInfo.kernelArgPatchInfoVector.size() <= 0);

        storeKernelArg(argIndex, NONE_OBJ, nullptr, nullptr, argSize);

        auto crossThreadData = getCrossThreadData();
        auto crossThreadDataEnd = ptrOffset(crossThreadData, getCrossThreadDataSize());

        for (const auto &kernelArgPatchInfo : kernelArgInfo.kernelArgPatchInfoVector) {
            DEBUG_BREAK_IF(kernelArgPatchInfo.size <= 0);
            auto pDst = ptrOffset(crossThreadData, kernelArgPatchInfo.crossthreadOffset);

            auto pSrc = ptrOffset(argVal, kernelArgPatchInfo.sourceOffset);

            DEBUG_BREAK_IF(!(ptrOffset(pDst, kernelArgPatchInfo.size) <= crossThreadDataEnd));
            ((void)(crossThreadDataEnd));

            memcpy_s(pDst, kernelArgPatchInfo.size, pSrc, kernelArgPatchInfo.size);
        }
        retVal = CL_SUCCESS;
    }

    return retVal;
}

cl_int Kernel::setArgSampler(uint32_t argIndex,
                             size_t argSize,
                             const void *argVal) {
    auto retVal = CL_INVALID_SAMPLER;

    if (!argVal) {
        return retVal;
    }

    auto clSamplerObj = *(static_cast<const cl_sampler *>(argVal));
    auto pSampler = castToObject<Sampler>(clSamplerObj);

    if (pSampler && argSize == sizeof(cl_sampler *)) {
        const auto &kernelArgInfo = kernelInfo.kernelArgInfo[argIndex];

        storeKernelArg(argIndex, SAMPLER_OBJ, clSamplerObj, argVal, argSize);

        auto dsh = getDynamicStateHeap();
        auto samplerState = ptrOffset(dsh, kernelArgInfo.offsetHeap);

        pSampler->setArg(const_cast<void *>(samplerState));

        auto crossThreadData = reinterpret_cast<uint32_t *>(getCrossThreadData());
        patch<uint32_t, unsigned int>(pSampler->getSnapWaValue(), crossThreadData, kernelArgInfo.offsetSamplerSnapWa);
        patch<uint32_t, uint32_t>(GetAddrModeEnum(pSampler->addressingMode), crossThreadData, kernelArgInfo.offsetSamplerAddressingMode);
        patch<uint32_t, uint32_t>(GetNormCoordsEnum(pSampler->normalizedCoordinates), crossThreadData, kernelArgInfo.offsetSamplerNormalizedCoords);
        patch<uint32_t, uint32_t>(SAMPLER_OBJECT_ID_SHIFT + kernelArgInfo.offsetHeap, crossThreadData, kernelArgInfo.offsetObjectId);

        retVal = CL_SUCCESS;
    }

    return retVal;
}

cl_int Kernel::setArgAccelerator(uint32_t argIndex,
                                 size_t argSize,
                                 const void *argVal) {
    auto retVal = CL_INVALID_ARG_VALUE;

    if (argSize != sizeof(cl_accelerator_intel)) {
        return CL_INVALID_ARG_SIZE;
    }

    if (!argVal) {
        return retVal;
    }

    auto clAcceleratorObj = *(static_cast<const cl_accelerator_intel *>(argVal));
    DBG_LOG_INPUTS("setArgAccelerator cl_mem", clAcceleratorObj);

    const auto pAccelerator = castToObject<IntelAccelerator>(clAcceleratorObj);

    if (pAccelerator) {
        storeKernelArg(argIndex, ACCELERATOR_OBJ, clAcceleratorObj, argVal, argSize);

        const auto &kernelArgInfo = kernelInfo.kernelArgInfo[argIndex];

        if (kernelArgInfo.samplerArgumentType == iOpenCL::SAMPLER_OBJECT_VME) {
            auto crossThreadData = getCrossThreadData();

            const auto pVmeAccelerator = castToObjectOrAbort<VmeAccelerator>(pAccelerator);
            auto pDesc = static_cast<const cl_motion_estimation_desc_intel *>(pVmeAccelerator->getDescriptor());
            DEBUG_BREAK_IF(!pDesc);

            auto pVmeMbBlockTypeDst = reinterpret_cast<cl_uint *>(ptrOffset(crossThreadData, kernelArgInfo.offsetVmeMbBlockType));
            *pVmeMbBlockTypeDst = pDesc->mb_block_type;

            auto pVmeSubpixelMode = reinterpret_cast<cl_uint *>(ptrOffset(crossThreadData, kernelArgInfo.offsetVmeSubpixelMode));
            *pVmeSubpixelMode = pDesc->subpixel_mode;

            auto pVmeSadAdjustMode = reinterpret_cast<cl_uint *>(ptrOffset(crossThreadData, kernelArgInfo.offsetVmeSadAdjustMode));
            *pVmeSadAdjustMode = pDesc->sad_adjust_mode;

            auto pVmeSearchPathType = reinterpret_cast<cl_uint *>(ptrOffset(crossThreadData, kernelArgInfo.offsetVmeSearchPathType));
            *pVmeSearchPathType = pDesc->search_path_type;

            retVal = CL_SUCCESS;
        } else if (kernelArgInfo.samplerArgumentType == iOpenCL::SAMPLER_OBJECT_VE) {
            retVal = CL_SUCCESS;
        }
    }

    return retVal;
}

cl_int Kernel::setArgDevQueue(uint32_t argIndex,
                              size_t argSize,
                              const void *argVal) {
    if (argVal == nullptr) {
        return CL_INVALID_ARG_VALUE;
    }

    if (argSize != sizeof(cl_command_queue)) {
        return CL_INVALID_ARG_SIZE;
    }

    auto clDeviceQueue = *(static_cast<const device_queue *>(argVal));
    auto pDeviceQueue = castToObject<DeviceQueue>(clDeviceQueue);

    if (pDeviceQueue == nullptr) {
        return CL_INVALID_DEVICE_QUEUE;
    }

    storeKernelArg(argIndex, DEVICE_QUEUE_OBJ, clDeviceQueue, argVal, argSize);

    const auto &kernelArgPatchInfo = kernelInfo.kernelArgInfo[argIndex].kernelArgPatchInfoVector[0];
    auto patchLocation = ptrOffset(reinterpret_cast<uint32_t *>(getCrossThreadData()),
                                   kernelArgPatchInfo.crossthreadOffset);

    patchWithRequiredSize(patchLocation, kernelArgPatchInfo.size,
                          static_cast<uintptr_t>(pDeviceQueue->getQueueBuffer()->getGpuAddressToPatch()));

    return CL_SUCCESS;
}

void Kernel::setKernelArgHandler(uint32_t argIndex, KernelArgHandler handler) {
    if (kernelArgHandlers.size() <= argIndex) {
        kernelArgHandlers.resize(argIndex + 1);
    }

    kernelArgHandlers[argIndex] = handler;
}

void Kernel::unsetArg(uint32_t argIndex) {
    if (kernelArguments[argIndex].isPatched) {
        patchedArgumentsNum--;
        kernelArguments[argIndex].isPatched = false;
    }
}

void Kernel::createReflectionSurface() {
    if (this->isParentKernel && kernelReflectionSurface == nullptr) {
        auto &hwInfo = device.getHardwareInfo();
        auto &hwHelper = HwHelper::get(hwInfo.pPlatform->eRenderCoreFamily);
        BlockKernelManager *blockManager = program->getBlockKernelManager();
        uint32_t blockCount = static_cast<uint32_t>(blockManager->getCount());

        ObjectCounts objectCount;
        getParentObjectCounts(objectCount);
        uint32_t parentImageCount = objectCount.imageCount;
        uint32_t parentSamplerCount = objectCount.samplerCount;
        size_t maxConstantBufferSize = 0;

        std::vector<IGIL_KernelCurbeParams> *curbeParamsForBlocks = new std::vector<IGIL_KernelCurbeParams>[ blockCount ];

        uint64_t *tokenMask = new uint64_t[blockCount];
        uint32_t *sshTokenOffsetsFromKernelData = new uint32_t[blockCount];

        size_t kernelReflectionSize = alignUp(sizeof(IGIL_KernelDataHeader) + blockCount * sizeof(IGIL_KernelAddressData), sizeof(void *));
        uint32_t kernelDataOffset = static_cast<uint32_t>(kernelReflectionSize);
        uint32_t parentSSHAlignedSize = alignUp(this->kernelInfo.heapInfo.pKernelHeader->SurfaceStateHeapSize, hwHelper.getBindingTableStateAlignement());
        uint32_t btOffset = parentSSHAlignedSize;

        for (uint32_t i = 0; i < blockCount; i++) {
            const KernelInfo *pBlockInfo = blockManager->getBlockKernelInfo(i);
            size_t samplerStateAndBorderColorSize = 0;

            uint32_t firstSSHTokenIndex = 0;

            ReflectionSurfaceHelper::getCurbeParams(curbeParamsForBlocks[i], tokenMask[i], firstSSHTokenIndex, *pBlockInfo, hwInfo);

            maxConstantBufferSize = std::max(maxConstantBufferSize, static_cast<size_t>(pBlockInfo->patchInfo.dataParameterStream->DataParameterStreamSize));

            samplerStateAndBorderColorSize = pBlockInfo->getSamplerStateArraySize(hwInfo);
            samplerStateAndBorderColorSize = alignUp(samplerStateAndBorderColorSize, Sampler::samplerStateArrayAlignment);
            samplerStateAndBorderColorSize += pBlockInfo->getBorderColorStateSize();
            samplerStateAndBorderColorSize = alignUp(samplerStateAndBorderColorSize, sizeof(void *));

            sshTokenOffsetsFromKernelData[i] = offsetof(IGIL_KernelData, m_data) + sizeof(IGIL_KernelCurbeParams) * firstSSHTokenIndex;

            kernelReflectionSize += alignUp(sizeof(IGIL_KernelData) + sizeof(IGIL_KernelCurbeParams) * curbeParamsForBlocks[i].size(), sizeof(void *));
            kernelReflectionSize += parentSamplerCount * sizeof(IGIL_SamplerParams) + samplerStateAndBorderColorSize;
        }

        maxConstantBufferSize = alignUp(maxConstantBufferSize, sizeof(void *));
        kernelReflectionSize += blockCount * alignUp(maxConstantBufferSize, sizeof(void *));
        kernelReflectionSize += parentImageCount * sizeof(IGIL_ImageParamters);
        kernelReflectionSize += parentSamplerCount * sizeof(IGIL_ParentSamplerParams);
        kernelReflectionSurface = device.getMemoryManager()->allocateGraphicsMemory(kernelReflectionSize);

        for (uint32_t i = 0; i < blockCount; i++) {
            const KernelInfo *pBlockInfo = blockManager->getBlockKernelInfo(i);
            uint32_t newKernelDataOffset = ReflectionSurfaceHelper::setKernelData(kernelReflectionSurface->getUnderlyingBuffer(),
                                                                                  kernelDataOffset,
                                                                                  curbeParamsForBlocks[i],
                                                                                  tokenMask[i],
                                                                                  maxConstantBufferSize,
                                                                                  parentSamplerCount,
                                                                                  *pBlockInfo,
                                                                                  device.getHardwareInfo());

            uint32_t offset = static_cast<uint32_t>(offsetof(IGIL_KernelDataHeader, m_data) + sizeof(IGIL_KernelAddressData) * i);

            uint32_t samplerHeapOffset = static_cast<uint32_t>(alignUp(kernelDataOffset + sizeof(IGIL_KernelData) + curbeParamsForBlocks[i].size() * sizeof(IGIL_KernelCurbeParams), sizeof(void *)));
            uint32_t samplerHeapSize = static_cast<uint32_t>(alignUp(pBlockInfo->getSamplerStateArraySize(device.getHardwareInfo()), Sampler::samplerStateArrayAlignment) + pBlockInfo->getBorderColorStateSize());
            uint32_t constantBufferOffset = alignUp(samplerHeapOffset + samplerHeapSize, sizeof(void *));

            uint32_t samplerParamsOffset = 0;
            if (parentSamplerCount) {
                samplerParamsOffset = newKernelDataOffset - sizeof(IGIL_SamplerParams) * parentSamplerCount;
                IGIL_SamplerParams *pSamplerParams = (IGIL_SamplerParams *)ptrOffset(kernelReflectionSurface->getUnderlyingBuffer(), samplerParamsOffset);
                uint32_t sampler = 0;
                for (uint32_t argID = 0; argID < pBlockInfo->kernelArgInfo.size(); argID++) {
                    if (pBlockInfo->kernelArgInfo[argID].isSampler) {

                        pSamplerParams[sampler].m_ArgID = argID;
                        pSamplerParams[sampler].m_SamplerStateOffset = pBlockInfo->kernelArgInfo[argID].offsetHeap;
                        sampler++;
                    }
                }
            }

            ReflectionSurfaceHelper::setKernelAddressData(kernelReflectionSurface->getUnderlyingBuffer(),
                                                          offset,
                                                          kernelDataOffset,
                                                          samplerHeapOffset,
                                                          constantBufferOffset,
                                                          samplerParamsOffset,
                                                          sshTokenOffsetsFromKernelData[i] + kernelDataOffset,
                                                          btOffset,
                                                          *pBlockInfo,
                                                          device.getHardwareInfo());

            if (samplerHeapSize > 0) {
                void *pDst = ptrOffset(kernelReflectionSurface->getUnderlyingBuffer(), samplerHeapOffset);
                const void *pSrc = ptrOffset(pBlockInfo->heapInfo.pDsh, pBlockInfo->getBorderColorOffset());
                memcpy_s(pDst, samplerHeapSize, pSrc, samplerHeapSize);
            }

            void *pDst = ptrOffset(kernelReflectionSurface->getUnderlyingBuffer(), constantBufferOffset);
            const char *pSrc = pBlockInfo->crossThreadData;
            memcpy_s(pDst, pBlockInfo->getConstantBufferSize(), pSrc, pBlockInfo->getConstantBufferSize());

            btOffset += pBlockInfo->patchInfo.bindingTableState->Offset;
            kernelDataOffset = newKernelDataOffset;
        }

        uint32_t samplerOffset = 0;
        if (parentSamplerCount) {
            samplerOffset = kernelDataOffset + parentImageCount * sizeof(IGIL_ImageParamters);
        }
        ReflectionSurfaceHelper::setKernelDataHeader(kernelReflectionSurface->getUnderlyingBuffer(), blockCount, parentImageCount, parentSamplerCount, kernelDataOffset, samplerOffset);
        delete[] curbeParamsForBlocks;
        delete[] tokenMask;
        delete[] sshTokenOffsetsFromKernelData;

        // Patch constant values once after reflection surface creation
        patchBlocksCurbeWithConstantValues();
    }

    if (DebugManager.flags.ForceDispatchScheduler.get()) {
        if (this->isSchedulerKernel && kernelReflectionSurface == nullptr) {
            kernelReflectionSurface = device.getMemoryManager()->allocateGraphicsMemory(MemoryConstants::pageSize);
        }
    }
}

void Kernel::getParentObjectCounts(ObjectCounts &objectCount) {
    objectCount.imageCount = 0;
    objectCount.samplerCount = 0;
    DEBUG_BREAK_IF(!isParentKernel);

    for (size_t i = 0; i < this->kernelArguments.size(); i++) {
        if (kernelArguments[i].type == SAMPLER_OBJ) {
            objectCount.samplerCount++;
        } else if (kernelArguments[i].type == IMAGE_OBJ) {
            objectCount.imageCount++;
        }
    }
}

bool Kernel::hasPrintfOutput() const {
    return getKernelInfo().patchInfo.pAllocateStatelessPrintfSurface != nullptr;
}

size_t Kernel::getInstructionHeapSizeForExecutionModel() const {
    BlockKernelManager *blockManager = program->getBlockKernelManager();
    uint32_t blockCount = static_cast<uint32_t>(blockManager->getCount());

    size_t totalSize = 0;
    if (isParentKernel) {
        totalSize = kernelBinaryAlignement - 1; // for initial alignment
        for (uint32_t i = 0; i < blockCount; i++) {
            const KernelInfo *pBlockInfo = blockManager->getBlockKernelInfo(i);
            totalSize += pBlockInfo->heapInfo.pKernelHeader->KernelHeapSize;
            totalSize = alignUp(totalSize, kernelBinaryAlignement);
        }
    }
    return totalSize;
}

void Kernel::patchBlocksCurbeWithConstantValues() {

    BlockKernelManager *blockManager = program->getBlockKernelManager();
    uint32_t blockCount = static_cast<uint32_t>(blockManager->getCount());

    uint64_t globalMemoryGpuAddress = program->getGlobalSurface() != nullptr ? program->getGlobalSurface()->getGpuAddressToPatch() : 0;
    uint64_t constantMemoryGpuAddress = program->getConstantSurface() != nullptr ? program->getConstantSurface()->getGpuAddressToPatch() : 0;

    for (uint32_t blockID = 0; blockID < blockCount; blockID++) {
        const KernelInfo *pBlockInfo = blockManager->getBlockKernelInfo(blockID);

        uint64_t globalMemoryCurbeOffset = ReflectionSurfaceHelper::undefinedOffset;
        uint32_t globalMemoryPatchSize = 0;
        uint64_t constantMemoryCurbeOffset = ReflectionSurfaceHelper::undefinedOffset;
        uint32_t constantMemoryPatchSize = 0;

        if (pBlockInfo->patchInfo.pAllocateStatelessGlobalMemorySurfaceWithInitialization) {
            globalMemoryCurbeOffset = pBlockInfo->patchInfo.pAllocateStatelessGlobalMemorySurfaceWithInitialization->DataParamOffset;
            globalMemoryPatchSize = pBlockInfo->patchInfo.pAllocateStatelessGlobalMemorySurfaceWithInitialization->DataParamSize;
        }

        if (pBlockInfo->patchInfo.pAllocateStatelessConstantMemorySurfaceWithInitialization) {
            constantMemoryCurbeOffset = pBlockInfo->patchInfo.pAllocateStatelessConstantMemorySurfaceWithInitialization->DataParamOffset;
            constantMemoryPatchSize = pBlockInfo->patchInfo.pAllocateStatelessConstantMemorySurfaceWithInitialization->DataParamSize;
        }

        ReflectionSurfaceHelper::patchBlocksCurbeWithConstantValues(kernelReflectionSurface->getUnderlyingBuffer(), blockID,
                                                                    globalMemoryCurbeOffset, globalMemoryPatchSize, globalMemoryGpuAddress,
                                                                    constantMemoryCurbeOffset, constantMemoryPatchSize, constantMemoryGpuAddress,
                                                                    ReflectionSurfaceHelper::undefinedOffset, 0, 0);
    }
}

void Kernel::ReflectionSurfaceHelper::getCurbeParams(std::vector<IGIL_KernelCurbeParams> &curbeParamsOut, uint64_t &tokenMaskOut, uint32_t &firstSSHTokenIndex, const KernelInfo &kernelInfo, const HardwareInfo &hwInfo) {

    size_t numArgs = kernelInfo.kernelArgInfo.size();
    size_t patchTokenCount = +kernelInfo.kernelNonArgInfo.size();
    uint64_t tokenMask = 0;
    tokenMaskOut = 0;
    firstSSHTokenIndex = 0;
    curbeParamsOut.reserve(patchTokenCount * 5);

    uint32_t bindingTableIndex = 253;

    for (uint32_t argNumber = 0; argNumber < numArgs; argNumber++) {
        IGIL_KernelCurbeParams curbeParam;
        bindingTableIndex = 253;
        auto sizeOfkernelArgForSSH = kernelInfo.gpuPointerSize;

        if (kernelInfo.kernelArgInfo[argNumber].isBuffer) {
            curbeParam.m_patchOffset = kernelInfo.kernelArgInfo[argNumber].kernelArgPatchInfoVector[0].crossthreadOffset;
            curbeParam.m_parameterSize = kernelInfo.gpuPointerSize;
            curbeParam.m_parameterType = COMPILER_DATA_PARAMETER_GLOBAL_SURFACE;
            curbeParam.m_sourceOffset = argNumber;

            curbeParamsOut.push_back(curbeParam);
            tokenMask |= ((uint64_t)1 << 63);
        } else if (kernelInfo.kernelArgInfo[argNumber].isImage) {
            if (isValidOffset(kernelInfo.kernelArgInfo[argNumber].offsetImgWidth)) {
                curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{DATA_PARAMETER_IMAGE_WIDTH + 50, sizeof(uint32_t), kernelInfo.kernelArgInfo[argNumber].offsetImgWidth, argNumber});
            }

            if (isValidOffset(kernelInfo.kernelArgInfo[argNumber].offsetImgHeight)) {
                curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{DATA_PARAMETER_IMAGE_HEIGHT + 50, sizeof(uint32_t), kernelInfo.kernelArgInfo[argNumber].offsetImgHeight, argNumber});
            }

            if (isValidOffset(kernelInfo.kernelArgInfo[argNumber].offsetImgDepth)) {
                curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{DATA_PARAMETER_IMAGE_DEPTH + 50, sizeof(uint32_t), kernelInfo.kernelArgInfo[argNumber].offsetImgDepth, argNumber});
            }

            if (isValidOffset(kernelInfo.kernelArgInfo[argNumber].offsetChannelDataType)) {
                curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{DATA_PARAMETER_IMAGE_CHANNEL_DATA_TYPE + 50, sizeof(uint32_t), kernelInfo.kernelArgInfo[argNumber].offsetChannelDataType, argNumber});
            }

            if (isValidOffset(kernelInfo.kernelArgInfo[argNumber].offsetChannelOrder)) {
                curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{DATA_PARAMETER_IMAGE_CHANNEL_ORDER + 50, sizeof(uint32_t), kernelInfo.kernelArgInfo[argNumber].offsetChannelOrder, argNumber});
            }

            if (isValidOffset(kernelInfo.kernelArgInfo[argNumber].offsetArraySize)) {
                curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{DATA_PARAMETER_IMAGE_ARRAY_SIZE + 50, sizeof(uint32_t), kernelInfo.kernelArgInfo[argNumber].offsetArraySize, argNumber});
            }

            if (isValidOffset(kernelInfo.kernelArgInfo[argNumber].offsetObjectId)) {
                curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{DATA_PARAMETER_OBJECT_ID + 50, sizeof(uint32_t), kernelInfo.kernelArgInfo[argNumber].offsetObjectId, argNumber});
            }
            tokenMask |= ((uint64_t)1 << 50);

            if (kernelInfo.patchInfo.bindingTableState) {
                auto &hwHelper = HwHelper::get(hwInfo.pPlatform->eRenderCoreFamily);
                void *ssh = static_cast<char *>(kernelInfo.heapInfo.pSsh) + kernelInfo.patchInfo.bindingTableState->Offset;

                for (uint32_t i = 0; i < kernelInfo.patchInfo.bindingTableState->Count; i++) {

                    uint32_t pointer = hwHelper.getBindingTableStateSurfaceStatePointer(ssh, i);
                    if (pointer == kernelInfo.kernelArgInfo[argNumber].offsetHeap) {
                        bindingTableIndex = i;
                        break;
                    }
                }
                DEBUG_BREAK_IF(!((bindingTableIndex != 253) || (kernelInfo.patchInfo.bindingTableState->Count == 0)));
            }

        } else if (kernelInfo.kernelArgInfo[argNumber].isSampler) {
            if (isValidOffset(kernelInfo.kernelArgInfo[argNumber].offsetSamplerSnapWa)) {
                curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{DATA_PARAMETER_SAMPLER_COORDINATE_SNAP_WA_REQUIRED + 100, sizeof(uint32_t), kernelInfo.kernelArgInfo[argNumber].offsetSamplerSnapWa, argNumber});
            }

            if (isValidOffset(kernelInfo.kernelArgInfo[argNumber].offsetSamplerAddressingMode)) {
                curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{DATA_PARAMETER_SAMPLER_ADDRESS_MODE + 100, sizeof(uint32_t), kernelInfo.kernelArgInfo[argNumber].offsetSamplerAddressingMode, argNumber});
            }

            if (isValidOffset(kernelInfo.kernelArgInfo[argNumber].offsetSamplerNormalizedCoords)) {
                curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{DATA_PARAMETER_SAMPLER_NORMALIZED_COORDS + 100, sizeof(uint32_t), kernelInfo.kernelArgInfo[argNumber].offsetSamplerNormalizedCoords, argNumber});
            }

            if (isValidOffset(kernelInfo.kernelArgInfo[argNumber].offsetObjectId)) {
                curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{DATA_PARAMETER_OBJECT_ID + 100, sizeof(uint32_t), kernelInfo.kernelArgInfo[argNumber].offsetObjectId, argNumber});
            }
            tokenMask |= ((uint64_t)1 << 51);
        } else {
            bindingTableIndex = 0;
            sizeOfkernelArgForSSH = 0;
        }

        curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{1024, sizeOfkernelArgForSSH, bindingTableIndex, argNumber});

        if (kernelInfo.kernelArgInfo[argNumber].slmAlignment != 0) {
            DEBUG_BREAK_IF(kernelInfo.kernelArgInfo[argNumber].kernelArgPatchInfoVector.size() != 1);
            uint32_t offset = kernelInfo.kernelArgInfo[argNumber].kernelArgPatchInfoVector[0].crossthreadOffset;
            uint32_t srcOffset = kernelInfo.kernelArgInfo[argNumber].slmAlignment;
            curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES, 0, offset, srcOffset});
            tokenMask |= ((uint64_t)1 << DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES);
        }
    }

    for (uint32_t i = 0; i < kernelInfo.patchInfo.dataParameterBuffers.size(); i++) {
        if (kernelInfo.patchInfo.dataParameterBuffers[i]->Type == DATA_PARAMETER_KERNEL_ARGUMENT) {
            curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{DATA_PARAMETER_KERNEL_ARGUMENT, kernelInfo.patchInfo.dataParameterBuffers[i]->DataSize, kernelInfo.patchInfo.dataParameterBuffers[i]->Offset, kernelInfo.patchInfo.dataParameterBuffers[i]->ArgumentNumber});
            tokenMask |= ((uint64_t)1 << DATA_PARAMETER_KERNEL_ARGUMENT);
        }
    }

    for (uint32_t i = 0; i < 3; i++) {
        const uint32_t sizeOfParam = 4;
        if (kernelInfo.workloadInfo.enqueuedLocalWorkSizeOffsets[i] != WorkloadInfo::undefinedOffset) {
            curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{DATA_PARAMETER_ENQUEUED_LOCAL_WORK_SIZE, sizeOfParam, kernelInfo.workloadInfo.enqueuedLocalWorkSizeOffsets[i], i * sizeOfParam});
            tokenMask |= ((uint64_t)1 << DATA_PARAMETER_ENQUEUED_LOCAL_WORK_SIZE);
        }
        if (kernelInfo.workloadInfo.globalWorkOffsetOffsets[i] != WorkloadInfo::undefinedOffset) {
            curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{DATA_PARAMETER_GLOBAL_WORK_OFFSET, sizeOfParam, kernelInfo.workloadInfo.globalWorkOffsetOffsets[i], i * sizeOfParam});
            tokenMask |= ((uint64_t)1 << DATA_PARAMETER_GLOBAL_WORK_OFFSET);
        }
        if (kernelInfo.workloadInfo.globalWorkSizeOffsets[i] != WorkloadInfo::undefinedOffset) {
            curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{DATA_PARAMETER_GLOBAL_WORK_SIZE, sizeOfParam, kernelInfo.workloadInfo.globalWorkSizeOffsets[i], i * sizeOfParam});
            tokenMask |= ((uint64_t)1 << DATA_PARAMETER_GLOBAL_WORK_SIZE);
        }
        if (kernelInfo.workloadInfo.localWorkSizeOffsets[i] != WorkloadInfo::undefinedOffset) {
            curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{DATA_PARAMETER_LOCAL_WORK_SIZE, sizeOfParam, kernelInfo.workloadInfo.localWorkSizeOffsets[i], i * sizeOfParam});
            tokenMask |= ((uint64_t)1 << DATA_PARAMETER_LOCAL_WORK_SIZE);
        }
        if (kernelInfo.workloadInfo.localWorkSizeOffsets2[i] != WorkloadInfo::undefinedOffset) {
            curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{DATA_PARAMETER_LOCAL_WORK_SIZE, sizeOfParam, kernelInfo.workloadInfo.localWorkSizeOffsets2[i], i * sizeOfParam});
            tokenMask |= ((uint64_t)1 << DATA_PARAMETER_LOCAL_WORK_SIZE);
        }
        if (kernelInfo.workloadInfo.numWorkGroupsOffset[i] != WorkloadInfo::undefinedOffset) {
            curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{DATA_PARAMETER_NUM_WORK_GROUPS, sizeOfParam, kernelInfo.workloadInfo.numWorkGroupsOffset[i], i * sizeOfParam});
            tokenMask |= ((uint64_t)1 << DATA_PARAMETER_NUM_WORK_GROUPS);
        }
    }

    if (kernelInfo.workloadInfo.parentEventOffset != WorkloadInfo::undefinedOffset) {
        curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{DATA_PARAMETER_PARENT_EVENT, sizeof(uint32_t), kernelInfo.workloadInfo.parentEventOffset, 0});
        tokenMask |= ((uint64_t)1 << DATA_PARAMETER_PARENT_EVENT);
    }
    if (kernelInfo.workloadInfo.workDimOffset != WorkloadInfo::undefinedOffset) {
        curbeParamsOut.emplace_back(IGIL_KernelCurbeParams{DATA_PARAMETER_WORK_DIMENSIONS, sizeof(uint32_t), kernelInfo.workloadInfo.workDimOffset, 0});
        tokenMask |= ((uint64_t)1 << DATA_PARAMETER_WORK_DIMENSIONS);
    }

    std::sort(curbeParamsOut.begin(), curbeParamsOut.end(), compareFunction);
    tokenMaskOut = tokenMask;
    firstSSHTokenIndex = static_cast<uint32_t>(curbeParamsOut.size() - numArgs);
}

uint32_t Kernel::ReflectionSurfaceHelper::setKernelData(void *reflectionSurface, uint32_t offset,
                                                        std::vector<IGIL_KernelCurbeParams> &curbeParamsIn, uint64_t tokenMaskIn,
                                                        size_t maxConstantBufferSize, size_t samplerCount, const KernelInfo &kernelInfo, const HardwareInfo &hwInfo) {
    uint32_t offsetToEnd = 0;
    IGIL_KernelData *kernelData = reinterpret_cast<IGIL_KernelData *>(ptrOffset(reflectionSurface, offset));
    size_t samplerHeapSize = alignUp(kernelInfo.getSamplerStateArraySize(hwInfo), Sampler::samplerStateArrayAlignment) + kernelInfo.getBorderColorStateSize();

    kernelData->m_numberOfCurbeParams = static_cast<uint32_t>(curbeParamsIn.size()); // number of paramters to patch
    kernelData->m_numberOfCurbeTokens = static_cast<uint32_t>(curbeParamsIn.size() - kernelInfo.kernelArgInfo.size());
    kernelData->m_numberOfSamplerStates = static_cast<uint32_t>(kernelInfo.getSamplerStateArrayCount());
    kernelData->m_SizeOfSamplerHeap = static_cast<uint32_t>(samplerHeapSize);
    kernelData->m_SamplerBorderColorStateOffsetOnDSH = kernelInfo.patchInfo.samplerStateArray ? kernelInfo.patchInfo.samplerStateArray->BorderColorOffset : 0;
    kernelData->m_SamplerStateArrayOffsetOnDSH = kernelInfo.patchInfo.samplerStateArray ? kernelInfo.patchInfo.samplerStateArray->Offset : (uint32_t)-1;
    kernelData->m_sizeOfConstantBuffer = kernelInfo.getConstantBufferSize();
    kernelData->m_PatchTokensMask = tokenMaskIn;
    kernelData->m_ScratchSpacePatchValue = 0;
    kernelData->m_SIMDSize = kernelInfo.patchInfo.executionEnvironment ? kernelInfo.patchInfo.executionEnvironment->LargestCompiledSIMDSize : 0;
    kernelData->m_HasBarriers = kernelInfo.patchInfo.executionEnvironment ? kernelInfo.patchInfo.executionEnvironment->HasBarriers : 0;
    kernelData->m_RequiredWkgSizes[0] = kernelInfo.reqdWorkGroupSize[0] != WorkloadInfo::undefinedOffset ? static_cast<uint32_t>(kernelInfo.reqdWorkGroupSize[0]) : 0;
    kernelData->m_RequiredWkgSizes[1] = kernelInfo.reqdWorkGroupSize[1] != WorkloadInfo::undefinedOffset ? static_cast<uint32_t>(kernelInfo.reqdWorkGroupSize[1]) : 0;
    kernelData->m_RequiredWkgSizes[2] = kernelInfo.reqdWorkGroupSize[2] != WorkloadInfo::undefinedOffset ? static_cast<uint32_t>(kernelInfo.reqdWorkGroupSize[2]) : 0;
    kernelData->m_InilineSLMSize = kernelInfo.workloadInfo.slmStaticSize;

    bool localIdRequired = false;
    if (kernelInfo.patchInfo.threadPayload) {
        if (kernelInfo.patchInfo.threadPayload->LocalIDFlattenedPresent ||
            kernelInfo.patchInfo.threadPayload->LocalIDXPresent ||
            kernelInfo.patchInfo.threadPayload->LocalIDYPresent ||
            kernelInfo.patchInfo.threadPayload->LocalIDZPresent) {
            localIdRequired = true;
        }
        kernelData->m_PayloadSize = PerThreadDataHelper::getThreadPayloadSize(*kernelInfo.patchInfo.threadPayload, kernelData->m_SIMDSize);
    }

    kernelData->m_NeedLocalIDS = localIdRequired ? 1 : 0;
    kernelData->m_DisablePreemption = 0u;

    bool concurrentExecAllowed = true;

    if (kernelInfo.patchInfo.pAllocateStatelessPrivateSurface) {
        if (kernelInfo.patchInfo.pAllocateStatelessPrivateSurface->PerThreadPrivateMemorySize > 0) {
            concurrentExecAllowed = false;
        }
    }
    kernelData->m_CanRunConcurently = concurrentExecAllowed ? 1 : 0;

    if (DebugManager.flags.DisableConcurrentBlockExecution.get()) {
        kernelData->m_CanRunConcurently = false;
    }

    IGIL_KernelCurbeParams *kernelCurbeParams = kernelData->m_data;

    for (uint32_t i = 0; i < curbeParamsIn.size(); i++) {
        kernelCurbeParams[i] = curbeParamsIn[i];
    }

    offsetToEnd = static_cast<uint32_t>(offset +
                                        alignUp(sizeof(IGIL_KernelData) + sizeof(IGIL_KernelCurbeParams) * curbeParamsIn.size(), sizeof(void *)) +
                                        alignUp(samplerHeapSize, sizeof(void *)) +
                                        alignUp(maxConstantBufferSize, sizeof(void *)) +
                                        sizeof(IGIL_SamplerParams) * samplerCount);

    return offsetToEnd;
}

void Kernel::ReflectionSurfaceHelper::setKernelAddressDataBtOffset(void *reflectionSurface, uint32_t blockID, uint32_t btOffset) {

    uint32_t offset = static_cast<uint32_t>(offsetof(IGIL_KernelDataHeader, m_data) + sizeof(IGIL_KernelAddressData) * blockID);
    IGIL_KernelAddressData *kernelAddressData = reinterpret_cast<IGIL_KernelAddressData *>(ptrOffset(reflectionSurface, offset));

    kernelAddressData->m_BTSoffset = btOffset;
}

void Kernel::ReflectionSurfaceHelper::setKernelAddressData(void *reflectionSurface, uint32_t offset, uint32_t kernelDataOffset, uint32_t samplerHeapOffset,
                                                           uint32_t constantBufferOffset, uint32_t samplerParamsOffset,
                                                           uint32_t sshTokensOffset, uint32_t btOffset, const KernelInfo &kernelInfo, const HardwareInfo &hwInfo) {
    IGIL_KernelAddressData *kernelAddressData = reinterpret_cast<IGIL_KernelAddressData *>(ptrOffset(reflectionSurface, offset));

    auto &hwHelper = HwHelper::get(hwInfo.pPlatform->eRenderCoreFamily);

    kernelAddressData->m_KernelDataOffset = kernelDataOffset;
    kernelAddressData->m_SamplerHeapOffset = samplerHeapOffset;
    kernelAddressData->m_SamplerParamsOffset = samplerParamsOffset;
    kernelAddressData->m_ConstantBufferOffset = constantBufferOffset;
    kernelAddressData->m_SSHTokensOffset = sshTokensOffset;
    kernelAddressData->m_BTSoffset = btOffset;
    kernelAddressData->m_BTSize = static_cast<uint32_t>(kernelInfo.patchInfo.bindingTableState ? kernelInfo.patchInfo.bindingTableState->Count * hwHelper.getBindingTableStateSize() : 0);
}

template <>
void Kernel::ReflectionSurfaceHelper::patchBlocksCurbe<false>(void *reflectionSurface, uint32_t blockID,
                                                              uint64_t defaultDeviceQueueCurbeOffset, uint32_t patchSizeDefaultQueue, uint64_t defaultDeviceQueueGpuAddress,
                                                              uint64_t eventPoolCurbeOffset, uint32_t patchSizeEventPool, uint64_t eventPoolGpuAddress,
                                                              uint64_t deviceQueueCurbeOffset, uint32_t patchSizeDeviceQueue, uint64_t deviceQueueGpuAddress,
                                                              uint64_t printfBufferOffset, uint32_t patchSizePrintfBuffer, uint64_t printfBufferGpuAddress,
                                                              uint64_t privateSurfaceOffset, uint32_t privateSurfaceSize, uint64_t privateSurfaceGpuAddress) {

    IGIL_KernelDataHeader *pKernelHeader = reinterpret_cast<IGIL_KernelDataHeader *>(reflectionSurface);

    // Reflection surface must be initialized prior to patching blocks curbe on KRS
    DEBUG_BREAK_IF(blockID >= pKernelHeader->m_numberOfKernels);

    IGIL_KernelAddressData *addressData = pKernelHeader->m_data;
    // const buffer offsets must be set
    DEBUG_BREAK_IF(addressData[blockID].m_ConstantBufferOffset == 0);

    void *pCurbe = ptrOffset(reflectionSurface, addressData[blockID].m_ConstantBufferOffset);

    if (defaultDeviceQueueCurbeOffset != undefinedOffset) {
        auto *patchedPointer = ptrOffset(pCurbe, (size_t)defaultDeviceQueueCurbeOffset);
        patchWithRequiredSize(patchedPointer, patchSizeDefaultQueue, (uintptr_t)defaultDeviceQueueGpuAddress);
    }
    if (eventPoolCurbeOffset != undefinedOffset) {
        auto *patchedPointer = ptrOffset(pCurbe, (size_t)eventPoolCurbeOffset);
        patchWithRequiredSize(patchedPointer, patchSizeEventPool, (uintptr_t)eventPoolGpuAddress);
    }
    if (deviceQueueCurbeOffset != undefinedOffset) {
        auto *patchedPointer = ptrOffset(pCurbe, (size_t)deviceQueueCurbeOffset);
        patchWithRequiredSize(patchedPointer, patchSizeDeviceQueue, (uintptr_t)deviceQueueGpuAddress);
    }
    if (printfBufferOffset != undefinedOffset) {
        auto *patchedPointer = ptrOffset(pCurbe, (size_t)printfBufferOffset);
        patchWithRequiredSize(patchedPointer, patchSizePrintfBuffer, (uintptr_t)printfBufferGpuAddress);
    }

    if (privateSurfaceOffset != undefinedOffset) {
        auto *patchedPointer = ptrOffset(pCurbe, (size_t)privateSurfaceOffset);
        patchWithRequiredSize(patchedPointer, privateSurfaceSize, (uintptr_t)privateSurfaceGpuAddress);
    }
}

void Kernel::ReflectionSurfaceHelper::patchBlocksCurbeWithConstantValues(void *reflectionSurface, uint32_t blockID,
                                                                         uint64_t globalMemoryCurbeOffset, uint32_t globalMemoryPatchSize, uint64_t globalMemoryGpuAddress,
                                                                         uint64_t constantMemoryCurbeOffset, uint32_t constantMemoryPatchSize, uint64_t constantMemoryGpuAddress,
                                                                         uint64_t privateMemoryCurbeOffset, uint32_t privateMemoryPatchSize, uint64_t privateMemoryGpuAddress) {

    IGIL_KernelDataHeader *pKernelHeader = reinterpret_cast<IGIL_KernelDataHeader *>(reflectionSurface);

    // Reflection surface must be initialized prior to patching blocks curbe on KRS
    DEBUG_BREAK_IF(blockID >= pKernelHeader->m_numberOfKernels);

    IGIL_KernelAddressData *addressData = pKernelHeader->m_data;
    // const buffer offsets must be set
    DEBUG_BREAK_IF(addressData[blockID].m_ConstantBufferOffset == 0);

    void *pCurbe = ptrOffset(reflectionSurface, addressData[blockID].m_ConstantBufferOffset);

    if (globalMemoryCurbeOffset != undefinedOffset) {
        auto *patchedPointer = ptrOffset(pCurbe, (size_t)globalMemoryCurbeOffset);
        patchWithRequiredSize(patchedPointer, globalMemoryPatchSize, (uintptr_t)globalMemoryGpuAddress);
    }
    if (constantMemoryCurbeOffset != undefinedOffset) {
        auto *patchedPointer = ptrOffset(pCurbe, (size_t)constantMemoryCurbeOffset);
        patchWithRequiredSize(patchedPointer, constantMemoryPatchSize, (uintptr_t)constantMemoryGpuAddress);
    }
    if (privateMemoryCurbeOffset != undefinedOffset) {
        auto *patchedPointer = ptrOffset(pCurbe, (size_t)privateMemoryCurbeOffset);
        patchWithRequiredSize(patchedPointer, privateMemoryPatchSize, (uintptr_t)privateMemoryGpuAddress);
    }
}

void Kernel::ReflectionSurfaceHelper::setParentImageParams(void *reflectionSurface, std::vector<Kernel::SimpleKernelArgInfo> &parentArguments, const KernelInfo &parentKernelInfo) {
    IGIL_KernelDataHeader *pKernelHeader = reinterpret_cast<IGIL_KernelDataHeader *>(reflectionSurface);
    IGIL_ImageParamters *pImageParameters = reinterpret_cast<IGIL_ImageParamters *>(ptrOffset(pKernelHeader, (size_t)pKernelHeader->m_ParentImageDataOffset));

    uint32_t numArgs = (uint32_t)parentArguments.size();
    for (uint32_t i = 0; i < numArgs; i++) {
        if (parentArguments[i].type == Kernel::kernelArgType::IMAGE_OBJ) {
            const Image *image = const_cast<const Image *>(castToObject<Image>((cl_mem)parentArguments[i].object));
            if (image) {
                pImageParameters->m_ArraySize = (uint32_t)image->getImageDesc().image_array_size;
                pImageParameters->m_Depth = (uint32_t)image->getImageDesc().image_depth;
                pImageParameters->m_Height = (uint32_t)image->getImageDesc().image_height;
                pImageParameters->m_Width = (uint32_t)image->getImageDesc().image_width;
                pImageParameters->m_NumMipLevels = (uint32_t)image->getImageDesc().num_mip_levels;
                pImageParameters->m_NumSamples = (uint32_t)image->getImageDesc().num_samples;

                pImageParameters->m_ChannelDataType = (uint32_t)image->getImageFormat().image_channel_data_type;
                pImageParameters->m_ChannelOrder = (uint32_t)image->getImageFormat().image_channel_data_type;
                pImageParameters->m_ObjectID = (uint32_t)parentKernelInfo.kernelArgInfo[i].offsetHeap;
                pImageParameters++;
            }
        }
    }
}

void Kernel::ReflectionSurfaceHelper::setParentSamplerParams(void *reflectionSurface, std::vector<Kernel::SimpleKernelArgInfo> &parentArguments, const KernelInfo &parentKernelInfo) {
    IGIL_KernelDataHeader *pKernelHeader = reinterpret_cast<IGIL_KernelDataHeader *>(reflectionSurface);
    IGIL_ParentSamplerParams *pParentSamplerParams = reinterpret_cast<IGIL_ParentSamplerParams *>(ptrOffset(pKernelHeader, (size_t)pKernelHeader->m_ParentSamplerParamsOffset));

    uint32_t numArgs = (uint32_t)parentArguments.size();
    for (uint32_t i = 0; i < numArgs; i++) {
        if (parentArguments[i].type == Kernel::kernelArgType::SAMPLER_OBJ) {
            const Sampler *sampler = const_cast<const Sampler *>(castToObject<Sampler>((cl_sampler)parentArguments[i].object));
            if (sampler) {
                pParentSamplerParams->CoordinateSnapRequired = (uint32_t)sampler->getSnapWaValue();
                pParentSamplerParams->m_AddressingMode = (uint32_t)sampler->addressingMode;
                pParentSamplerParams->NormalizedCoords = (uint32_t)sampler->normalizedCoordinates;

                pParentSamplerParams->m_ObjectID = OCLRT_ARG_OFFSET_TO_SAMPLER_OBJECT_ID((uint32_t)parentKernelInfo.kernelArgInfo[i].offsetHeap);
                pParentSamplerParams++;
            }
        }
    }
}

void Kernel::resetSharedObjectsPatchAddresses() {
    for (size_t i = 0; i < getKernelArgsNumber(); i++) {
        auto clMem = (cl_mem)kernelArguments[i].object;
        auto memObj = castToObject<MemObj>(clMem);
        if (memObj && memObj->peekSharingHandler()) {
            setArg((uint32_t)i, sizeof(cl_mem), &clMem);
        }
    }
}

void Kernel::provideInitializationHints() {

    const auto &patchInfo = kernelInfo.patchInfo;
    Context *context = program->getContextPtr();
    if (context == nullptr || !context->isProvidingPerformanceHints())
        return;
    if (privateSurfaceSize) {
        context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, PRIVATE_MEMORY_USAGE_TOO_HIGH,
                                        kernelInfo.name.c_str(), privateSurfaceSize);
    }
    if (patchInfo.mediavfestate) {
        auto scratchSize = patchInfo.mediavfestate->PerThreadScratchSpace;
        scratchSize *= device.getDeviceInfo().computeUnitsUsedForScratch * getKernelInfo().getMaxSimdSize();
        if (scratchSize > 0) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, REGISTER_PRESSURE_TOO_HIGH,
                                            kernelInfo.name.c_str(), scratchSize);
        }
    }
}

void Kernel::patchDefaultDeviceQueue(DeviceQueue *devQueue) {

    const auto &patchInfo = kernelInfo.patchInfo;
    if (patchInfo.pAllocateStatelessDefaultDeviceQueueSurface) {
        if (crossThreadData) {
            auto patchLocation = ptrOffset(reinterpret_cast<uint32_t *>(getCrossThreadData()),
                                           patchInfo.pAllocateStatelessDefaultDeviceQueueSurface->DataParamOffset);

            patchWithRequiredSize(patchLocation, patchInfo.pAllocateStatelessDefaultDeviceQueueSurface->DataParamSize,
                                  static_cast<uintptr_t>(devQueue->getQueueBuffer()->getGpuAddressToPatch()));
        }
        if (requiresSshForBuffers()) {
            auto surfaceState = ptrOffset(reinterpret_cast<uintptr_t *>(getSurfaceStateHeap()),
                                          patchInfo.pAllocateStatelessDefaultDeviceQueueSurface->SurfaceStateHeapOffset);
            Buffer::setSurfaceState(&getContext(), surfaceState, devQueue->getQueueBuffer()->getUnderlyingBufferSize(), (void *)devQueue->getQueueBuffer()->getGpuAddress(), devQueue->getQueueBuffer());
        }
    }
}

void Kernel::patchEventPool(DeviceQueue *devQueue) {

    const auto &patchInfo = kernelInfo.patchInfo;
    if (patchInfo.pAllocateStatelessEventPoolSurface) {
        if (crossThreadData) {
            auto patchLocation = ptrOffset(reinterpret_cast<uint32_t *>(getCrossThreadData()),
                                           patchInfo.pAllocateStatelessEventPoolSurface->DataParamOffset);

            patchWithRequiredSize(patchLocation, patchInfo.pAllocateStatelessEventPoolSurface->DataParamSize,
                                  static_cast<uintptr_t>(devQueue->getEventPoolBuffer()->getGpuAddressToPatch()));
        }

        if (requiresSshForBuffers()) {
            auto surfaceState = ptrOffset(reinterpret_cast<uintptr_t *>(getSurfaceStateHeap()),
                                          patchInfo.pAllocateStatelessEventPoolSurface->SurfaceStateHeapOffset);
            Buffer::setSurfaceState(&getContext(), surfaceState, devQueue->getEventPoolBuffer()->getUnderlyingBufferSize(), (void *)devQueue->getEventPoolBuffer()->getGpuAddress(), devQueue->getEventPoolBuffer());
        }
    }
}

void Kernel::patchBlocksSimdSize() {
    BlockKernelManager *blockManager = program->getBlockKernelManager();

    for (uint32_t i = 0; i < kernelInfo.childrenKernelsIdOffset.size(); i++) {

        DEBUG_BREAK_IF(!(kernelInfo.childrenKernelsIdOffset[i].first < static_cast<uint32_t>(blockManager->getCount())));

        const KernelInfo *blockInfo = blockManager->getBlockKernelInfo(kernelInfo.childrenKernelsIdOffset[i].first);
        uint32_t *simdSize = reinterpret_cast<uint32_t *>(&crossThreadData[kernelInfo.childrenKernelsIdOffset[i].second]);
        *simdSize = blockInfo->getMaxSimdSize();
    }
}

template void Kernel::patchReflectionSurface<false>(DeviceQueue *, PrintfHandler *);

bool Kernel::isPatched() const {
    return patchedArgumentsNum == kernelInfo.argumentsToPatchNum;
}
cl_int Kernel::checkCorrectImageAccessQualifier(cl_uint argIndex,
                                                size_t argSize,
                                                const void *argValue) const {
    if (getKernelInfo().kernelArgInfo[argIndex].isImage) {
        cl_mem mem = *(static_cast<const cl_mem *>(argValue));
        MemObj *pMemObj = nullptr;
        WithCastToInternal(mem, &pMemObj);
        if (pMemObj) {
            cl_kernel_arg_access_qualifier accessQualifier = getKernelInfo().kernelArgInfo[argIndex].accessQualifier;
            cl_mem_flags flags = pMemObj->getFlags();
            if ((accessQualifier == CL_KERNEL_ARG_ACCESS_READ_ONLY && ((flags | CL_MEM_WRITE_ONLY) == flags)) ||
                (accessQualifier == CL_KERNEL_ARG_ACCESS_WRITE_ONLY && ((flags | CL_MEM_READ_ONLY) == flags))) {
                return CL_INVALID_ARG_VALUE;
            }
        } else {
            return CL_INVALID_ARG_VALUE;
        }
    }
    return CL_SUCCESS;
}

void Kernel::resolveArgs() {
    if (!Kernel::isPatched() || !imageTransformer->hasRegisteredImages3d() || !canTransformImages())
        return;
    bool canTransformImageTo2dArray = true;
    for (uint32_t i = 0; i < patchedArgumentsNum; i++) {
        if (kernelInfo.kernelArgInfo.at(i).isSampler) {
            auto clSamplerObj = *(static_cast<const cl_sampler *>(kernelArguments.at(i).value));
            auto sampler = castToObjectOrAbort<Sampler>(clSamplerObj);
            if (sampler->isTransformable()) {
                canTransformImageTo2dArray = true;
            } else {
                canTransformImageTo2dArray = false;
                break;
            }
        }
    }
    if (canTransformImageTo2dArray) {
        imageTransformer->transformImagesTo2dArray(kernelInfo, kernelArguments, getSurfaceStateHeap());
    } else if (imageTransformer->didTransform()) {
        imageTransformer->transformImagesTo3d(kernelInfo, kernelArguments, getSurfaceStateHeap());
    }
}

bool Kernel::canTransformImages() const {
    return device.getHardwareInfo().pPlatform->eRenderCoreFamily >= IGFX_GEN9_CORE;
}
} // namespace OCLRT
