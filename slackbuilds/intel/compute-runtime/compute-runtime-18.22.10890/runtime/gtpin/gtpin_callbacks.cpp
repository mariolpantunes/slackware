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

#include "gtpin_ocl_interface.h"
#include "CL/cl.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/device/device_info.h"
#include "runtime/gtpin/gtpin_defs.h"
#include "runtime/gtpin/gtpin_hw_helper.h"
#include "runtime/gtpin/gtpin_notify.h"
#include "runtime/kernel/kernel.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/memory_manager/surface.h"
#include "runtime/platform/platform.h"
#include "runtime/utilities/spinlock.h"
#include <deque>
#include <vector>

using namespace gtpin;

namespace OCLRT {

extern gtpin::ocl::gtpin_events_t GTPinCallbacks;

igc_init_t *pIgcInfo = nullptr;
std::atomic<int> sequenceCount(1);
CommandQueue *pCmdQueueForFlushTask = nullptr;
std::deque<gtpinkexec_t> kernelExecQueue;
std::atomic_flag kernelExecQueueLock = ATOMIC_FLAG_INIT;

void gtpinNotifyContextCreate(cl_context context) {
    if (isGTPinInitialized) {
        platform_info_t gtpinPlatformInfo;
        auto pPlatform = platform();
        auto pDevice = pPlatform->getDevice(0);
        GFXCORE_FAMILY genFamily = pDevice->getHardwareInfo().pPlatform->eRenderCoreFamily;
        GTPinHwHelper &gtpinHelper = GTPinHwHelper::get(genFamily);
        gtpinPlatformInfo.gen_version = (gtpin::GTPIN_GEN_VERSION)gtpinHelper.getGenVersion();
        gtpinPlatformInfo.device_id = static_cast<uint32_t>(pDevice->getHardwareInfo().pPlatform->usDeviceID);
        (*GTPinCallbacks.onContextCreate)((context_handle_t)context, &gtpinPlatformInfo, &pIgcInfo);
    }
}

void gtpinNotifyContextDestroy(cl_context context) {
    if (isGTPinInitialized) {
        (*GTPinCallbacks.onContextDestroy)((context_handle_t)context);
    }
}

void gtpinNotifyKernelCreate(cl_kernel kernel) {
    if (isGTPinInitialized) {
        auto pKernel = castToObjectOrAbort<Kernel>(kernel);
        size_t gtpinBTI = pKernel->getNumberOfBindingTableStates();
        // Enlarge local copy of SSH by 1 SS
        auto pPlatform = platform();
        auto pDevice = pPlatform->getDevice(0);
        GFXCORE_FAMILY genFamily = pDevice->getHardwareInfo().pPlatform->eRenderCoreFamily;
        GTPinHwHelper &gtpinHelper = GTPinHwHelper::get(genFamily);
        if (!gtpinHelper.addSurfaceState(pKernel)) {
            // Kernel with no SSH or Kernel EM, not supported
            return;
        }
        if (pKernel->isKernelHeapSubstituted()) {
            // ISA for this kernel was already substituted
            return;
        }
        // Notify GT-Pin that new kernel was created
        Context *pContext = &(pKernel->getContext());
        cl_context context = (cl_context)pContext;
        auto &kernelInfo = pKernel->getKernelInfo();
        instrument_params_in_t paramsIn;
        paramsIn.kernel_type = GTPIN_KERNEL_TYPE_CS;
        paramsIn.simd = (GTPIN_SIMD_WIDTH)kernelInfo.getMaxSimdSize();
        paramsIn.orig_kernel_binary = (uint8_t *)pKernel->getKernelHeap();
        paramsIn.orig_kernel_size = static_cast<uint32_t>(pKernel->getKernelHeapSize());
        paramsIn.buffer_type = GTPIN_BUFFER_BINDFULL;
        paramsIn.buffer_desc.BTI = static_cast<uint32_t>(gtpinBTI);
        paramsIn.igc_hash_id = kernelInfo.heapInfo.pKernelHeader->ShaderHashCode;
        paramsIn.kernel_name = (char *)kernelInfo.name.c_str();
        paramsIn.igc_info = nullptr;
        instrument_params_out_t paramsOut = {0};
        (*GTPinCallbacks.onKernelCreate)((context_handle_t)(cl_context)context, &paramsIn, &paramsOut);
        // Substitute ISA of created kernel with instrumented code
        pKernel->substituteKernelHeap(paramsOut.inst_kernel_binary, paramsOut.inst_kernel_size);
        pKernel->setKernelId(paramsOut.kernel_id);
    }
}

void gtpinNotifyKernelSubmit(cl_kernel kernel, void *pCmdQueue) {
    if (isGTPinInitialized) {
        auto pKernel = castToObjectOrAbort<Kernel>(kernel);
        if (pKernel->getSurfaceStateHeapSize() == 0) {
            // Kernel with no SSH, not supported
            return;
        }
        Context *pContext = &(pKernel->getContext());
        cl_context context = (cl_context)pContext;
        uint64_t kernelId = pKernel->getKernelId();
        command_buffer_handle_t commandBuffer = (command_buffer_handle_t)((uintptr_t)(sequenceCount++));
        uint32_t kernelOffset = 0;
        resource_handle_t resource = 0;
        // Notify GT-Pin that abstract "command buffer" was created
        (*GTPinCallbacks.onCommandBufferCreate)((context_handle_t)context, commandBuffer);
        // Notify GT-Pin that kernel was submited for execution
        (*GTPinCallbacks.onKernelSubmit)(commandBuffer, kernelId, &kernelOffset, &resource);
        // Create new record in Kernel Execution Queue describing submited kernel
        gtpinkexec_t kExec;
        kExec.pKernel = pKernel;
        kExec.gtpinResource = (cl_mem)resource;
        kExec.commandBuffer = commandBuffer;
        kExec.pCommandQueue = (CommandQueue *)pCmdQueue;
        SpinLock lock;
        lock.enter(kernelExecQueueLock);
        kernelExecQueue.push_back(kExec);
        lock.leave(kernelExecQueueLock);
        // Patch SSH[gtpinBTI] with GT-Pin resource
        auto pPlatform = platform();
        auto pDevice = pPlatform->getDevice(0);
        GFXCORE_FAMILY genFamily = pDevice->getHardwareInfo().pPlatform->eRenderCoreFamily;
        GTPinHwHelper &gtpinHelper = GTPinHwHelper::get(genFamily);
        size_t gtpinBTI = pKernel->getNumberOfBindingTableStates() - 1;
        void *pSurfaceState = gtpinHelper.getSurfaceState(pKernel, gtpinBTI);
        cl_mem buffer = (cl_mem)resource;
        auto pBuffer = castToObjectOrAbort<Buffer>(buffer);
        pBuffer->setArgStateful(const_cast<void *>(pSurfaceState));
    }
}

void gtpinNotifyPreFlushTask(void *pCmdQueue) {
    if (isGTPinInitialized) {
        pCmdQueueForFlushTask = (CommandQueue *)pCmdQueue;
    }
}

void gtpinNotifyFlushTask(uint32_t flushedTaskCount) {
    if (isGTPinInitialized) {
        SpinLock lock;
        lock.enter(kernelExecQueueLock);
        size_t numElems = kernelExecQueue.size();
        for (size_t n = 0; n < numElems; n++) {
            if ((kernelExecQueue[n].pCommandQueue == pCmdQueueForFlushTask) && !kernelExecQueue[n].isTaskCountValid) {
                // Update record in Kernel Execution Queue with kernel's TC
                kernelExecQueue[n].isTaskCountValid = true;
                kernelExecQueue[n].taskCount = flushedTaskCount;
                break;
            }
        }
        lock.leave(kernelExecQueueLock);
        pCmdQueueForFlushTask = nullptr;
    }
}

void gtpinNotifyTaskCompletion(uint32_t completedTaskCount) {
    if (isGTPinInitialized) {
        SpinLock lock;
        lock.enter(kernelExecQueueLock);
        size_t numElems = kernelExecQueue.size();
        for (size_t n = 0; n < numElems;) {
            if (kernelExecQueue[n].isTaskCountValid && (kernelExecQueue[n].taskCount <= completedTaskCount)) {
                // Notify GT-Pin that execution of "command buffer" was completed
                (*GTPinCallbacks.onCommandBufferComplete)(kernelExecQueue[n].commandBuffer);
                // Remove kernel's record from Kernel Execution Queue
                kernelExecQueue.erase(kernelExecQueue.begin() + n);
                numElems--;
            } else {
                n++;
            }
        }
        lock.leave(kernelExecQueueLock);
    }
}

void gtpinNotifyMakeResident(void *pKernel, void *pCSR) {
    if (isGTPinInitialized) {
        SpinLock lock;
        lock.enter(kernelExecQueueLock);
        size_t numElems = kernelExecQueue.size();
        for (size_t n = 0; n < numElems; n++) {
            if ((kernelExecQueue[n].pKernel == pKernel) && !kernelExecQueue[n].isResourceResident) {
                // It's time for kernel to make resident its GT-Pin resource
                CommandStreamReceiver *pCommandStreamReceiver = reinterpret_cast<CommandStreamReceiver *>(pCSR);
                cl_mem gtpinBuffer = kernelExecQueue[n].gtpinResource;
                auto pBuffer = castToObjectOrAbort<Buffer>(gtpinBuffer);
                GraphicsAllocation *pGfxAlloc = pBuffer->getGraphicsAllocation();
                pCommandStreamReceiver->makeResident(*pGfxAlloc);
                kernelExecQueue[n].isResourceResident = true;
                break;
            }
        }
        lock.leave(kernelExecQueueLock);
    }
}

void gtpinNotifyUpdateResidencyList(void *pKernel, void *pResVec) {
    if (isGTPinInitialized) {
        SpinLock lock;
        lock.enter(kernelExecQueueLock);
        size_t numElems = kernelExecQueue.size();
        for (size_t n = 0; n < numElems; n++) {
            if ((kernelExecQueue[n].pKernel == pKernel) && !kernelExecQueue[n].isResourceResident) {
                // It's time for kernel to update its residency list with its GT-Pin resource
                std::vector<Surface *> *pResidencyVector = (std::vector<Surface *> *)pResVec;
                cl_mem gtpinBuffer = kernelExecQueue[n].gtpinResource;
                auto pBuffer = castToObjectOrAbort<Buffer>(gtpinBuffer);
                GraphicsAllocation *pGfxAlloc = pBuffer->getGraphicsAllocation();
                GeneralSurface *pSurface = new GeneralSurface(pGfxAlloc);
                pResidencyVector->push_back(pSurface);
                kernelExecQueue[n].isResourceResident = true;
                break;
            }
        }
        lock.leave(kernelExecQueueLock);
    }
}

void gtpinNotifyPlatformShutdown() {
    if (isGTPinInitialized) {
        // Clear Kernel Execution Queue
        kernelExecQueue.clear();
    }
}
}
