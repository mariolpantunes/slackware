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
#include "CL/cl.h"
#include "heap_info.h"
#include "kernel_arg_info.h"
#include "patch_info.h"
#include "runtime/helpers/hw_info.h"
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <vector>
#include <string>
#include <unordered_map>
#include <map>

namespace OCLRT {
class BuiltinDispatchInfoBuilder;
class Device;
class Kernel;
struct KernelInfo;
class DispatchInfo;
struct KernelArgumentType;
class GraphicsAllocation;
class MemoryManager;

extern std::unordered_map<std::string, uint32_t> accessQualifierMap;
extern std::unordered_map<std::string, uint32_t> addressQualifierMap;
extern std::map<std::string, size_t> typeSizeMap;

struct WorkloadInfo {
    uint32_t globalWorkOffsetOffsets[3];
    uint32_t globalWorkSizeOffsets[3];
    uint32_t localWorkSizeOffsets[3];
    uint32_t localWorkSizeOffsets2[3];
    uint32_t enqueuedLocalWorkSizeOffsets[3];
    uint32_t numWorkGroupsOffset[3];
    uint32_t maxWorkGroupSizeOffset;
    uint32_t workDimOffset;
    uint32_t slmStaticSize = 0;
    uint32_t simdSizeOffset;
    uint32_t parentEventOffset;
    uint32_t prefferedWkgMultipleOffset;

    static const uint32_t undefinedOffset;
    static const uint32_t invalidParentEvent;

    WorkloadInfo() {
        globalWorkOffsetOffsets[0] = undefinedOffset;
        globalWorkOffsetOffsets[1] = undefinedOffset;
        globalWorkOffsetOffsets[2] = undefinedOffset;
        globalWorkSizeOffsets[0] = undefinedOffset;
        globalWorkSizeOffsets[1] = undefinedOffset;
        globalWorkSizeOffsets[2] = undefinedOffset;
        localWorkSizeOffsets[0] = undefinedOffset;
        localWorkSizeOffsets[1] = undefinedOffset;
        localWorkSizeOffsets[2] = undefinedOffset;
        localWorkSizeOffsets2[0] = undefinedOffset;
        localWorkSizeOffsets2[1] = undefinedOffset;
        localWorkSizeOffsets2[2] = undefinedOffset;
        enqueuedLocalWorkSizeOffsets[0] = undefinedOffset;
        enqueuedLocalWorkSizeOffsets[1] = undefinedOffset;
        enqueuedLocalWorkSizeOffsets[2] = undefinedOffset;
        numWorkGroupsOffset[0] = undefinedOffset;
        numWorkGroupsOffset[1] = undefinedOffset;
        numWorkGroupsOffset[2] = undefinedOffset;
        maxWorkGroupSizeOffset = undefinedOffset;
        workDimOffset = undefinedOffset;
        simdSizeOffset = undefinedOffset;
        parentEventOffset = undefinedOffset;
        prefferedWkgMultipleOffset = undefinedOffset;
    }
};

static const float YTilingRatioValue = 1.3862943611198906188344642429164f;

struct WorkSizeInfo {

    uint32_t maxWorkGroupSize;
    uint32_t minWorkGroupSize;
    bool hasBarriers;
    uint32_t simdSize;
    uint32_t slmTotalSize;
    GFXCORE_FAMILY coreFamily;
    uint32_t numThreadsPerSubSlice;
    uint32_t localMemSize;
    bool imgUsed = false;
    bool yTiledSurfaces = false;
    bool useRatio = false;
    bool useStrictRatio = false;
    float targetRatio = 0;

    WorkSizeInfo(uint32_t maxWorkGroupSize, bool hasBarriers, uint32_t simdSize, uint32_t slmTotalSize, GFXCORE_FAMILY coreFamily, uint32_t numThreadsPerSubSlice, uint32_t localMemSize, bool imgUsed, bool yTiledSurface);
    WorkSizeInfo(const DispatchInfo &dispatchInfo);
    void setIfUseImg(Kernel *pKernel);
    void setMinWorkGroupSize();
    void checkRatio(const size_t workItems[3]);
};

struct DebugData {
    uint32_t vIsaSize = 0;
    uint32_t genIsaSize = 0;
    const char *vIsa = nullptr;
    const char *genIsa = nullptr;
};

struct KernelInfo {
  public:
    static KernelInfo *create();
    KernelInfo() {
        heapInfo = {};
        patchInfo = {};
        workloadInfo = {};
        kernelArgInfo = {};
        kernelNonArgInfo = {};
        childrenKernelsIdOffset = {};
        reqdWorkGroupSize[0] = WorkloadInfo::undefinedOffset;
        reqdWorkGroupSize[1] = WorkloadInfo::undefinedOffset;
        reqdWorkGroupSize[2] = WorkloadInfo::undefinedOffset;
    }

    KernelInfo(const KernelInfo &) = delete;
    KernelInfo &operator=(const KernelInfo &) = delete;

    ~KernelInfo();

    cl_int storeArgInfo(const SPatchKernelArgumentInfo *pkernelArgInfo);
    void storeKernelArgument(const SPatchDataParameterBuffer *pDataParameterKernelArg);
    void storeKernelArgument(const SPatchStatelessGlobalMemoryObjectKernelArgument *pStatelessGlobalKernelArg);
    void storeKernelArgument(const SPatchImageMemoryObjectKernelArgument *pImageMemObjKernelArg);
    void storeKernelArgument(const SPatchGlobalMemoryObjectKernelArgument *pGlobalMemObjKernelArg);
    void storeKernelArgument(const SPatchStatelessConstantMemoryObjectKernelArgument *pStatelessConstMemObjKernelArg);
    void storeKernelArgument(const SPatchStatelessDeviceQueueKernelArgument *pStatelessDeviceQueueKernelArg);
    void storeKernelArgument(const SPatchSamplerKernelArgument *pSamplerKernelArg);
    void storePatchToken(const SPatchAllocateStatelessPrivateSurface *pStatelessPrivateSurfaceArg);
    void storePatchToken(const SPatchAllocateStatelessConstantMemorySurfaceWithInitialization *pStatelessConstantMemorySurfaceWithInitializationArg);
    void storePatchToken(const SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization *pStatelessGlobalMemorySurfaceWithInitializationArg);
    void storePatchToken(const SPatchAllocateStatelessPrintfSurface *pStatelessPrintfSurfaceArg);
    void storePatchToken(const SPatchAllocateStatelessEventPoolSurface *pStatelessEventPoolSurfaceArg);
    void storePatchToken(const SPatchAllocateStatelessDefaultDeviceQueueSurface *pStatelessDefaultDeviceQueueSurfaceArg);
    void storePatchToken(const SPatchString *pStringArg);
    void storePatchToken(const SPatchKernelAttributesInfo *pKernelAttributesInfo);
    void storePatchToken(const SPatchAllocateSystemThreadSurface *pSystemThreadSurface);
    GraphicsAllocation *getGraphicsAllocation() const { return this->kernelAllocation; }
    cl_int resolveKernelInfo();
    void resizeKernelArgInfoAndRegisterParameter(uint32_t argCount) {
        if (kernelArgInfo.size() <= argCount) {
            kernelArgInfo.resize(argCount + 1);
        }
        if (!kernelArgInfo[argCount].needPatch) {
            kernelArgInfo[argCount].needPatch = true;
            argumentsToPatchNum++;
        }
    }

    void storeKernelArgPatchInfo(uint32_t argNum, uint32_t dataSize, uint32_t crossthreadOffset, uint32_t sourceOffset, uint32_t offsetSSH);

    const char *queryPrintfString(uint32_t index) const;

    size_t getSamplerStateArrayCount() const;
    size_t getSamplerStateArraySize(const HardwareInfo &hwInfo) const;
    size_t getBorderColorStateSize() const;
    size_t getBorderColorOffset() const;
    unsigned int getMaxSimdSize() const {
        const auto executionEnvironment = patchInfo.executionEnvironment;
        if (executionEnvironment == nullptr) {
            return 1;
        }

        if (executionEnvironment->CompiledSIMD32) {
            return 32;
        }

        if (executionEnvironment->CompiledSIMD16) {
            return 16;
        }

        return 8;
    }
    bool hasDeviceEnqueue() const {
        return patchInfo.executionEnvironment ? !!patchInfo.executionEnvironment->HasDeviceEnqueue : false;
    }
    bool requiresSubgroupIndependentForwardProgress() const {
        return patchInfo.executionEnvironment ? !!patchInfo.executionEnvironment->SubgroupIndependentForwardProgressRequired : false;
    }
    size_t getMaxRequiredWorkGroupSize(size_t maxWorkGroupSize) const {
        auto requiredWorkGroupSizeX = patchInfo.executionEnvironment->RequiredWorkGroupSizeX;
        auto requiredWorkGroupSizeY = patchInfo.executionEnvironment->RequiredWorkGroupSizeY;
        auto requiredWorkGroupSizeZ = patchInfo.executionEnvironment->RequiredWorkGroupSizeZ;
        size_t maxRequiredWorkGroupSize = requiredWorkGroupSizeX * requiredWorkGroupSizeY * requiredWorkGroupSizeZ;
        if ((maxRequiredWorkGroupSize == 0) || (maxRequiredWorkGroupSize > maxWorkGroupSize)) {
            maxRequiredWorkGroupSize = maxWorkGroupSize;
        }
        return maxRequiredWorkGroupSize;
    }

    uint32_t getConstantBufferSize() const;
    int32_t getArgNumByName(const char *name) const {
        int32_t argNum = 0;
        for (auto &arg : kernelArgInfo) {
            if (arg.name == name) {
                return argNum;
            }
            ++argNum;
        }
        return -1;
    }

    bool createKernelAllocation(MemoryManager *memoryManager);

    std::string name;
    std::string attributes;
    HeapInfo heapInfo;
    PatchInfo patchInfo;
    std::vector<KernelArgInfo> kernelArgInfo;
    std::vector<KernelArgInfo> kernelNonArgInfo;
    WorkloadInfo workloadInfo;
    std::vector<std::pair<uint32_t, uint32_t>> childrenKernelsIdOffset;
    bool usesSsh = false;
    bool requiresSshForBuffers = false;
    bool isValid = false;
    bool isVmeWorkload = false;
    char *crossThreadData = nullptr;
    size_t reqdWorkGroupSize[3];
    size_t requiredSubGroupSize = 0;
    uint32_t gpuPointerSize = 0;
    const BuiltinDispatchInfoBuilder *builtinDispatchBuilder = nullptr;
    uint32_t argumentsToPatchNum = 0;
    uint32_t systemKernelOffset = 0;
    uint64_t kernelId = 0;
    bool isKernelHeapSubstituted = false;
    GraphicsAllocation *kernelAllocation = nullptr;
    DebugData debugData;
};
} // namespace OCLRT
