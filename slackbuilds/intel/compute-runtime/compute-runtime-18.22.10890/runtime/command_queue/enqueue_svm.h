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
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_queue/enqueue_common.h"
#include "runtime/event/event.h"
#include <new>

namespace OCLRT {

using SvmFreeClbT = void(CL_CALLBACK *)(cl_command_queue queue,
                                        cl_uint numSvmPointers,
                                        void *svmPointers[],
                                        void *userData);

struct SvmFreeUserData {
    cl_uint numSvmPointers;
    void **svmPointers;
    SvmFreeClbT clb;
    void *userData;
    bool ownsEventDeletion;

    SvmFreeUserData(cl_uint numSvmPointers,
                    void **svmPointers, SvmFreeClbT clb,
                    void *userData,
                    bool ownsEventDeletion)
        : numSvmPointers(numSvmPointers),
          svmPointers(svmPointers),
          clb(clb),
          userData(userData),
          ownsEventDeletion(ownsEventDeletion){};
};

inline void CL_CALLBACK freeSvmEventClb(cl_event event,
                                        cl_int commandExecCallbackType,
                                        void *usrData) {
    auto freeDt = reinterpret_cast<SvmFreeUserData *>(usrData);
    auto eventObject = castToObject<Event>(event);
    if (freeDt->clb == nullptr) {
        auto ctx = eventObject->getContext();
        for (cl_uint i = 0; i < freeDt->numSvmPointers; i++) {
            castToObject<Context>(ctx)->getSVMAllocsManager()->freeSVMAlloc(freeDt->svmPointers[i]);
        }
    } else {
        freeDt->clb(eventObject->getCommandQueue(), freeDt->numSvmPointers,
                    freeDt->svmPointers, freeDt->userData);
    }
    if (freeDt->ownsEventDeletion) {
        castToObject<Event>(event)->release();
    }
    delete freeDt;
}

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueSVMMap(cl_bool blockingMap,
                                                cl_map_flags mapFlags,
                                                void *svmPtr,
                                                size_t size,
                                                cl_uint numEventsInWaitList,
                                                const cl_event *eventWaitList,
                                                cl_event *event) {

    OCLRT::GraphicsAllocation *svmAllocation = context->getSVMAllocsManager()->getSVMAlloc(svmPtr);
    if (svmAllocation == nullptr) {
        return CL_INVALID_VALUE;
    }

    NullSurface s;
    Surface *surfaces[] = {&s};
    cl_uint dimensions = 1;
    if (context->isProvidingPerformanceHints()) {
        context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_ENQUEUE_SVM_MAP_DOESNT_REQUIRE_COPY_DATA, svmPtr);
    }
    enqueueHandler<CL_COMMAND_SVM_MAP>(surfaces,
                                       blockingMap ? true : false,
                                       nullptr,
                                       dimensions,
                                       nullptr,
                                       nullptr,
                                       nullptr,
                                       numEventsInWaitList,
                                       eventWaitList,
                                       event);

    return CL_SUCCESS;
}

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueSVMUnmap(void *svmPtr,
                                                  cl_uint numEventsInWaitList,
                                                  const cl_event *eventWaitList,
                                                  cl_event *event) {

    OCLRT::GraphicsAllocation *svmAllocation = context->getSVMAllocsManager()->getSVMAlloc(svmPtr);
    if (svmAllocation == nullptr) {
        return CL_INVALID_VALUE;
    }

    NullSurface s;
    Surface *surfaces[] = {&s};
    cl_uint dimensions = 1;
    enqueueHandler<CL_COMMAND_SVM_UNMAP>(surfaces,
                                         false,
                                         nullptr,
                                         dimensions,
                                         nullptr,
                                         nullptr,
                                         nullptr,
                                         numEventsInWaitList,
                                         eventWaitList,
                                         event);

    return CL_SUCCESS;
}

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueSVMFree(cl_uint numSvmPointers,
                                                 void *svmPointers[],
                                                 SvmFreeClbT clb,
                                                 void *userData,
                                                 cl_uint numEventsInWaitList,
                                                 const cl_event *eventWaitList,
                                                 cl_event *retEvent) {
    cl_event event = nullptr;
    bool ownsEventDeletion = false;
    if (retEvent == nullptr) {
        ownsEventDeletion = true;
        retEvent = &event;
    }

    SvmFreeUserData *pFreeData = new SvmFreeUserData(numSvmPointers,
                                                     svmPointers,
                                                     clb,
                                                     userData,
                                                     ownsEventDeletion);

    NullSurface s;
    Surface *surfaces[] = {&s};
    cl_uint dimensions = 1;
    enqueueHandler<CL_COMMAND_SVM_FREE>(surfaces,
                                        false,
                                        nullptr,
                                        dimensions,
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        numEventsInWaitList,
                                        eventWaitList,
                                        retEvent);

    auto eventObject = castToObject<Event>(*retEvent);
    eventObject->addCallback(freeSvmEventClb, CL_COMPLETE, pFreeData);

    return CL_SUCCESS;
}

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueSVMMemcpy(cl_bool blockingCopy,
                                                   void *dstPtr,
                                                   const void *srcPtr,
                                                   size_t size,
                                                   cl_uint numEventsInWaitList,
                                                   const cl_event *eventWaitList,
                                                   cl_event *event) {

    GraphicsAllocation *pDstSvmAlloc = context->getSVMAllocsManager()->getSVMAlloc(dstPtr);
    GraphicsAllocation *pSrcSvmAlloc = context->getSVMAllocsManager()->getSVMAlloc(srcPtr);
    if ((pDstSvmAlloc == nullptr) || (pSrcSvmAlloc == nullptr)) {
        return CL_INVALID_VALUE;
    }

    MultiDispatchInfo dispatchInfo;

    auto &builder = BuiltIns::getInstance().getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer,
                                                                          this->getContext(), this->getDevice());

    builder.takeOwnership(this->context);

    BuiltinDispatchInfoBuilder::BuiltinOpParams operationParams;
    operationParams.srcPtr = const_cast<void *>(srcPtr);
    operationParams.dstPtr = dstPtr;
    operationParams.srcSvmAlloc = pSrcSvmAlloc;
    operationParams.dstSvmAlloc = pDstSvmAlloc;
    operationParams.srcOffset = {0, 0, 0};
    operationParams.dstOffset = {0, 0, 0};
    operationParams.size = {size, 0, 0};
    builder.buildDispatchInfos(dispatchInfo, operationParams);

    GeneralSurface s1(pSrcSvmAlloc), s2(pDstSvmAlloc);
    Surface *surfaces[] = {&s1, &s2};

    enqueueHandler<CL_COMMAND_SVM_MEMCPY>(
        surfaces,
        blockingCopy ? true : false,
        dispatchInfo,
        numEventsInWaitList,
        eventWaitList,
        event);

    builder.releaseOwnership();

    return CL_SUCCESS;
}

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueSVMMemFill(void *svmPtr,
                                                    const void *pattern,
                                                    size_t patternSize,
                                                    size_t size,
                                                    cl_uint numEventsInWaitList,
                                                    const cl_event *eventWaitList,
                                                    cl_event *event) {

    OCLRT::GraphicsAllocation *pSvmAlloc = context->getSVMAllocsManager()->getSVMAlloc(svmPtr);
    if (pSvmAlloc == nullptr) {
        return CL_INVALID_VALUE;
    }

    auto memoryManager = getDevice().getMemoryManager();
    DEBUG_BREAK_IF(nullptr == memoryManager);

    TakeOwnershipWrapper<Device> deviceOwnership(getDevice());
    auto patternAllocation = memoryManager->obtainReusableAllocation(patternSize, false).release();
    deviceOwnership.unlock();

    if (!patternAllocation) {
        patternAllocation = memoryManager->allocateGraphicsMemory(patternSize, MemoryConstants::preferredAlignment);
    }

    patternAllocation->setAllocationType(GraphicsAllocation::ALLOCATION_TYPE_FILL_PATTERN);

    if (patternSize == 1) {
        int patternInt = (uint32_t)((*(uint8_t *)pattern << 24) | (*(uint8_t *)pattern << 16) | (*(uint8_t *)pattern << 8) | *(uint8_t *)pattern);
        memcpy_s(patternAllocation->getUnderlyingBuffer(), sizeof(int), &patternInt, sizeof(int));
    } else if (patternSize == 2) {
        int patternInt = (uint32_t)((*(uint16_t *)pattern << 16) | *(uint16_t *)pattern);
        memcpy_s(patternAllocation->getUnderlyingBuffer(), sizeof(int), &patternInt, sizeof(int));
    } else {
        memcpy_s(patternAllocation->getUnderlyingBuffer(), patternSize, pattern, patternSize);
    }

    MultiDispatchInfo dispatchInfo;

    auto &builder = BuiltIns::getInstance().getBuiltinDispatchInfoBuilder(EBuiltInOps::FillBuffer,
                                                                          this->getContext(), this->getDevice());

    builder.takeOwnership(this->context);

    BuiltinDispatchInfoBuilder::BuiltinOpParams operationParams;
    MemObj patternMemObj(this->context, 0, 0, alignUp(patternSize, 4), patternAllocation->getUnderlyingBuffer(),
                         patternAllocation->getUnderlyingBuffer(), patternAllocation, false, false, true);
    operationParams.srcMemObj = &patternMemObj;
    operationParams.dstPtr = svmPtr;
    operationParams.dstSvmAlloc = pSvmAlloc;
    operationParams.dstOffset = {0, 0, 0};
    operationParams.size = {size, 0, 0};
    builder.buildDispatchInfos(dispatchInfo, operationParams);

    GeneralSurface s1(pSvmAlloc);
    GeneralSurface s2(patternAllocation);
    Surface *surfaces[] = {&s1, &s2};

    enqueueHandler<CL_COMMAND_SVM_MEMFILL>(
        surfaces,
        false,
        dispatchInfo,
        numEventsInWaitList,
        eventWaitList,
        event);

    memoryManager->storeAllocation(std::unique_ptr<GraphicsAllocation>(patternAllocation), REUSABLE_ALLOCATION, taskCount);

    builder.releaseOwnership();

    return CL_SUCCESS;
}

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueSVMMigrateMem(cl_uint numSvmPointers,
                                                       const void **svmPointers,
                                                       const size_t *sizes,
                                                       const cl_mem_migration_flags flags,
                                                       cl_uint numEventsInWaitList,
                                                       const cl_event *eventWaitList,
                                                       cl_event *event) {
    NullSurface s;
    Surface *surfaces[] = {&s};
    cl_uint dimensions = 1;

    enqueueHandler<CL_COMMAND_MIGRATE_MEM_OBJECTS>(surfaces,
                                                   false,
                                                   nullptr,
                                                   dimensions,
                                                   nullptr,
                                                   nullptr,
                                                   nullptr,
                                                   numEventsInWaitList,
                                                   eventWaitList,
                                                   event);
    return CL_SUCCESS;
}
}
