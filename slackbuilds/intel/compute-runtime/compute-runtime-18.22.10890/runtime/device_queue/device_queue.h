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
#include "runtime/api/cl_types.h"
#include "runtime/helpers/base_object.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/indirect_heap/indirect_heap.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/execution_model/device_enqueue.h"

namespace OCLRT {
class CommandQueue;
class Context;
class Device;
class Kernel;
class Event;
struct MultiDispatchInfo;
class SchedulerKernel;
struct HwTimeStamps;

template <>
struct OpenCLObjectMapper<_device_queue> {
    typedef class DeviceQueue DerivedType;
};

class DeviceQueue : public BaseObject<_device_queue> {
  public:
    static const cl_ulong objectMagic = 0x1734547890087154LL;

    DeviceQueue() {
        for (uint32_t i = 0; i < IndirectHeap::NUM_TYPES; i++) {
            heaps[i] = nullptr;
        }
        offsetDsh = 0;
    }
    DeviceQueue(Context *context, Device *device, cl_queue_properties &properties);
    ~DeviceQueue() override;

    Device &getDevice() { return *device; }
    Context &getContext() { return *context; }
    cl_uint getQueueSize() { return queueSize; }
    cl_command_queue_properties getCommandQueueProperties() const { return commandQueueProperties; }
    GraphicsAllocation *getQueueBuffer() { return queueBuffer; }
    GraphicsAllocation *getEventPoolBuffer() { return eventPoolBuffer; }
    GraphicsAllocation *getSlbBuffer() { return slbBuffer; }
    GraphicsAllocation *getStackBuffer() { return stackBuffer; }
    GraphicsAllocation *getQueueStorageBuffer() { return queueStorageBuffer; }
    GraphicsAllocation *getDshBuffer() { return dshBuffer; }
    GraphicsAllocation *getDebugQueue() { return debugQueue; }

    bool isProfilingEnabled() {
        return !!(commandQueueProperties & CL_QUEUE_PROFILING_ENABLE);
    }

    static DeviceQueue *create(Context *context,
                               Device *device,
                               const cl_queue_properties &properties,
                               cl_int &errcodeRet);

    cl_int getCommandQueueInfo(cl_command_queue_info paramName,
                               size_t paramValueSize, void *paramValue,
                               size_t *paramValueSizeRet);

    void setupExecutionModelDispatch(IndirectHeap &surfaceStateHeap, IndirectHeap &dynamicStateHeap, Kernel *parentKernel, uint32_t parentCount, uint32_t taskCount, HwTimeStamps *hwTimeStamp);

    virtual void setupIndirectState(IndirectHeap &surfaceStateHeap, IndirectHeap &dynamicStateHeap, Kernel *parentKernel, uint32_t parentIDCount);
    virtual void addExecutionModelCleanUpSection(Kernel *parentKernel, HwTimeStamps *hwTimeStamp, uint32_t taskCount);

    MOCKABLE_VIRTUAL bool isEMCriticalSectionFree() {
        auto igilCmdQueue = reinterpret_cast<IGIL_CommandQueue *>(queueBuffer->getUnderlyingBuffer());
        auto igilCriticalSection = const_cast<volatile uint *>(&igilCmdQueue->m_controls.m_CriticalSection);
        return *igilCriticalSection == ExecutionModelCriticalSection::Free;
    }

    virtual void resetDeviceQueue();
    virtual void dispatchScheduler(CommandQueue &cmdQ, SchedulerKernel &scheduler, PreemptionMode preemptionMode, IndirectHeap *ssh, IndirectHeap *dsh);
    virtual IndirectHeap *getIndirectHeap(IndirectHeap::Type type);

    void acquireEMCriticalSection() {
        if (DebugManager.flags.EnableNullHardware.get()) {
            return;
        }
        auto igilCmdQueue = reinterpret_cast<IGIL_CommandQueue *>(queueBuffer->getUnderlyingBuffer());
        igilCmdQueue->m_controls.m_CriticalSection = ExecutionModelCriticalSection::Taken;
    }

    uint32_t getDshOffset() const {
        return offsetDsh;
    }

    enum ExecutionModelCriticalSection {
        Free = 0,
        Taken = 1
    };

    static const uint32_t numberOfIDTables = 2;
    static const uint32_t interfaceDescriptorEntries = 64;
    static const uint32_t colorCalcStateSize = 192;
    static const uint32_t schedulerIDIndex = 62;
    static const uint32_t numberOfDeviceEnqueues = 128;

  protected:
    void allocateResources();
    void initDeviceQueue();

    Context *context = nullptr;
    Device *device = nullptr;
    cl_command_queue_properties commandQueueProperties = 0;
    cl_uint queueSize = 0;

    GraphicsAllocation *queueBuffer = nullptr;
    GraphicsAllocation *eventPoolBuffer = nullptr;
    GraphicsAllocation *slbBuffer = nullptr;
    GraphicsAllocation *stackBuffer = nullptr;
    GraphicsAllocation *queueStorageBuffer = nullptr;
    GraphicsAllocation *dshBuffer = nullptr;
    GraphicsAllocation *debugQueue = nullptr;

    DebugDataBuffer *debugData = nullptr;

    IndirectHeap *heaps[IndirectHeap::NUM_TYPES];
    uint32_t offsetDsh;
};

typedef DeviceQueue *(*DeviceQueueCreateFunc)(
    Context *context, Device *device, cl_queue_properties &properties);
} // namespace OCLRT
