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

#include "runtime/context/context.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/device/device.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/memory_manager/deferred_deleter.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/get_info.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include <algorithm>

namespace OCLRT {

MemObj::MemObj(Context *context,
               cl_mem_object_type memObjectType,
               cl_mem_flags flags,
               size_t size,
               void *memoryStorage,
               void *hostPtr,
               GraphicsAllocation *gfxAllocation,
               bool zeroCopy,
               bool isHostPtrSVM,
               bool isObjectRedescribed)
    : context(context), memObjectType(memObjectType), flags(flags), size(size),
      memoryStorage(memoryStorage), hostPtr(hostPtr),
      isZeroCopy(zeroCopy), isHostPtrSVM(isHostPtrSVM), isObjectRedescribed(isObjectRedescribed),
      graphicsAllocation(gfxAllocation) {
    completionStamp = {};

    if (context) {
        context->incRefInternal();
        memoryManager = context->getMemoryManager();
    }
}

MemObj::~MemObj() {
    bool needWait = false;
    if (allocatedMapPtr != nullptr) {
        needWait = true;
    }
    if (mapOperationsHandler.size() > 0 && !getCpuAddressForMapping()) {
        needWait = true;
    }
    if (!destructorCallbacks.empty()) {
        needWait = true;
    }

    if (memoryManager) {
        if (peekSharingHandler()) {
            peekSharingHandler()->releaseReusedGraphicsAllocation();
        }
        if (graphicsAllocation && !associatedMemObject && !isObjectRedescribed && !isHostPtrSVM && graphicsAllocation->peekReuseCount() == 0) {
            bool doAsyncDestrucions = DebugManager.flags.EnableAsyncDestroyAllocations.get();
            if (!doAsyncDestrucions) {
                needWait = true;
            }
            if (needWait && graphicsAllocation->taskCount != ObjectNotUsed) {
                waitForCsrCompletion();
            }
            destroyGraphicsAllocation(graphicsAllocation, doAsyncDestrucions);
            graphicsAllocation = nullptr;
        }

        releaseAllocatedMapPtr();
        if (mcsAllocation) {
            destroyGraphicsAllocation(mcsAllocation, false);
        }

        if (associatedMemObject) {
            if (associatedMemObject->getGraphicsAllocation() != this->getGraphicsAllocation()) {
                destroyGraphicsAllocation(graphicsAllocation, false);
            }
            associatedMemObject->decRefInternal();
        }
    }
    if (!destructorCallbacks.empty()) {
        for (auto iter = destructorCallbacks.rbegin(); iter != destructorCallbacks.rend(); iter++) {
            (*iter)->invoke(this);
            delete *iter;
        }
    }

    if (context) {
        context->decRefInternal();
    }
}

void MemObj::DestructorCallback::invoke(cl_mem memObj) {
    this->funcNotify(memObj, userData);
}

cl_int MemObj::getMemObjectInfo(cl_mem_info paramName,
                                size_t paramValueSize,
                                void *paramValue,
                                size_t *paramValueSizeRet) {
    cl_int retVal;
    size_t srcParamSize = 0;
    void *srcParam = nullptr;
    cl_bool usesSVMPointer;
    cl_uint refCnt = 0;
    cl_uint mapCount = 0;
    cl_mem clAssociatedMemObject = static_cast<cl_mem>(this->associatedMemObject);
    cl_context ctx = nullptr;

    switch (paramName) {
    case CL_MEM_TYPE:
        srcParamSize = sizeof(memObjectType);
        srcParam = &memObjectType;
        break;

    case CL_MEM_FLAGS:
        srcParamSize = sizeof(flags);
        srcParam = &flags;
        break;

    case CL_MEM_SIZE:
        srcParamSize = sizeof(size);
        srcParam = &size;
        break;

    case CL_MEM_HOST_PTR:
        srcParamSize = sizeof(hostPtr);
        srcParam = &hostPtr;
        break;

    case CL_MEM_CONTEXT:
        srcParamSize = sizeof(context);
        ctx = context;
        srcParam = &ctx;
        break;

    case CL_MEM_USES_SVM_POINTER:
        usesSVMPointer = isHostPtrSVM && !!(flags & CL_MEM_USE_HOST_PTR);
        srcParamSize = sizeof(cl_bool);
        srcParam = &usesSVMPointer;
        break;

    case CL_MEM_OFFSET:
        srcParamSize = sizeof(offset);
        srcParam = &offset;
        break;

    case CL_MEM_ASSOCIATED_MEMOBJECT:
        srcParamSize = sizeof(clAssociatedMemObject);
        srcParam = &clAssociatedMemObject;
        break;

    case CL_MEM_MAP_COUNT:
        srcParamSize = sizeof(mapCount);
        mapCount = static_cast<cl_uint>(mapOperationsHandler.size());
        srcParam = &mapCount;
        break;

    case CL_MEM_REFERENCE_COUNT:
        refCnt = static_cast<cl_uint>(this->getReference());
        srcParamSize = sizeof(refCnt);
        srcParam = &refCnt;
        break;

    default:
        getOsSpecificMemObjectInfo(paramName, &srcParamSize, &srcParam);
        break;
    }

    retVal = ::getInfo(paramValue, paramValueSize, srcParam, srcParamSize);

    if (paramValueSizeRet) {
        *paramValueSizeRet = srcParamSize;
    }

    return retVal;
}

cl_int MemObj::setDestructorCallback(void(CL_CALLBACK *funcNotify)(cl_mem, void *),
                                     void *userData) {
    auto cb = new DestructorCallback(funcNotify, userData);

    std::unique_lock<std::mutex> theLock(mtx);
    destructorCallbacks.push_back(cb);
    return CL_SUCCESS;
}

void *MemObj::getCpuAddress() const {
    return memoryStorage;
}

void *MemObj::getHostPtr() const {
    return hostPtr;
}

size_t MemObj::getSize() const {
    return size;
}

void MemObj::setCompletionStamp(CompletionStamp completionStamp, Device *pDevice, CommandQueue *pCmdQ) {
    this->completionStamp = completionStamp;
    device = pDevice;
    cmdQueuePtr = pCmdQ;
}

CompletionStamp MemObj::getCompletionStamp() const {
    return completionStamp;
}

void MemObj::setAllocatedMapPtr(void *allocatedMapPtr) {
    this->allocatedMapPtr = allocatedMapPtr;
}

cl_mem_flags MemObj::getFlags() const {
    return flags;
}

bool MemObj::isMemObjZeroCopy() const {
    return isZeroCopy;
}

bool MemObj::isMemObjWithHostPtrSVM() const {
    return isHostPtrSVM;
}

GraphicsAllocation *MemObj::getGraphicsAllocation() {
    return graphicsAllocation;
}

bool MemObj::readMemObjFlagsInvalid() {
    if (this->getFlags() & (CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_NO_ACCESS)) {
        return true;
    }

    return false;
}

bool MemObj::writeMemObjFlagsInvalid() {
    if (this->getFlags() & (CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS)) {
        return true;
    }

    return false;
}

bool MemObj::mapMemObjFlagsInvalid(cl_map_flags mapFlags) {
    if ((this->getFlags() & (CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS) && (mapFlags & CL_MAP_WRITE)) ||
        (this->getFlags() & (CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_NO_ACCESS) && (mapFlags & CL_MAP_READ))) {
        return true;
    }

    return false;
}

void MemObj::setHostPtrMinSize(size_t size) {
    hostPtrMinSize = size;
}

void *MemObj::getCpuAddressForMapping() {
    void *ptrToReturn = nullptr;
    if ((this->flags & CL_MEM_USE_HOST_PTR)) {
        ptrToReturn = this->hostPtr;
    } else {
        ptrToReturn = this->memoryStorage;
    }
    return ptrToReturn;
}
void *MemObj::getCpuAddressForMemoryTransfer() {
    void *ptrToReturn = nullptr;
    if ((this->flags & CL_MEM_USE_HOST_PTR) && this->isMemObjZeroCopy()) {
        ptrToReturn = this->hostPtr;
    } else {
        ptrToReturn = this->memoryStorage;
    }
    return ptrToReturn;
}
void MemObj::releaseAllocatedMapPtr() {
    if (allocatedMapPtr) {
        DEBUG_BREAK_IF((flags & CL_MEM_USE_HOST_PTR));
        memoryManager->freeSystemMemory(allocatedMapPtr);
    }
    allocatedMapPtr = nullptr;
}

void MemObj::waitForCsrCompletion() {
    if (memoryManager->device && graphicsAllocation) {
        memoryManager->device->getCommandStreamReceiver().waitForCompletionWithTimeout(false, TimeoutControls::maxTimeout, graphicsAllocation->taskCount);
    }
}

void MemObj::destroyGraphicsAllocation(GraphicsAllocation *allocation, bool asyncDestroy) {
    if (asyncDestroy && memoryManager->device && allocation->taskCount != ObjectNotUsed) {
        auto currentTag = *memoryManager->device->getTagAddress();
        if (currentTag < allocation->taskCount) {
            memoryManager->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), TEMPORARY_ALLOCATION);
            return;
        }
    }
    memoryManager->freeGraphicsMemory(allocation);
}

bool MemObj::checkIfMemoryTransferIsRequired(size_t offsetInMemObjest, size_t offsetInHostPtr, const void *hostPtr, cl_command_type cmdType) {
    auto bufferStorage = ptrOffset(this->getCpuAddressForMemoryTransfer(), offsetInMemObjest);
    auto hostStorage = ptrOffset(hostPtr, offsetInHostPtr);
    auto isMemTransferNeeded = !((bufferStorage == hostStorage) &&
                                 (cmdType == CL_COMMAND_WRITE_BUFFER || cmdType == CL_COMMAND_READ_BUFFER ||
                                  cmdType == CL_COMMAND_WRITE_BUFFER_RECT || cmdType == CL_COMMAND_READ_BUFFER_RECT ||
                                  cmdType == CL_COMMAND_WRITE_IMAGE || cmdType == CL_COMMAND_READ_IMAGE));
    return isMemTransferNeeded;
}

void *MemObj::getBasePtrForMap() {
    if (getFlags() & CL_MEM_USE_HOST_PTR) {
        return getHostPtr();
    } else {
        TakeOwnershipWrapper<MemObj> memObjOwnership(*this);
        if (!getAllocatedMapPtr()) {
            auto memory = memoryManager->allocateSystemMemory(getSize(), MemoryConstants::pageSize);
            setAllocatedMapPtr(memory);
        }
        return getAllocatedMapPtr();
    }
}

bool MemObj::addMappedPtr(void *ptr, size_t ptrLength, cl_map_flags &mapFlags,
                          MemObjSizeArray &size, MemObjOffsetArray &offset,
                          uint32_t mipLevel) {
    return mapOperationsHandler.add(ptr, ptrLength, mapFlags, size, offset,
                                    mipLevel);
}
} // namespace OCLRT
