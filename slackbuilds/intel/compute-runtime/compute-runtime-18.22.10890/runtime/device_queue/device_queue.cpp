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

#include "runtime/device_queue/device_queue.h"
#include "runtime/device_queue/device_queue_hw.h"
#include "runtime/context/context.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/queue_helpers.h"
#include "runtime/memory_manager/memory_manager.h"

namespace OCLRT {
DeviceQueueCreateFunc deviceQueueFactory[IGFX_MAX_CORE] = {};

DeviceQueue::DeviceQueue(Context *context,
                         Device *device,
                         cl_queue_properties &properties) : DeviceQueue() {
    this->context = context;
    this->device = device;
    auto &caps = device->getDeviceInfo();

    if (context) {
        context->incRefInternal();
    }

    commandQueueProperties = getCmdQueueProperties<cl_command_queue_properties>(&properties, CL_QUEUE_PROPERTIES);
    queueSize = getCmdQueueProperties<cl_uint>(&properties, CL_QUEUE_SIZE);

    if (queueSize == 0)
        queueSize = caps.queueOnDevicePreferredSize;

    allocateResources();
    initDeviceQueue();
}

DeviceQueue *DeviceQueue::create(Context *context, Device *device,
                                 const cl_queue_properties &properties,
                                 cl_int &errcodeRet) {
    errcodeRet = CL_SUCCESS;
    DeviceQueue *deviceQueue = context->getDefaultDeviceQueue();

    auto isDefaultDeviceQueue = getCmdQueueProperties<cl_command_queue_properties>(&properties) &
                                static_cast<cl_command_queue_properties>(CL_QUEUE_ON_DEVICE_DEFAULT);
    if (isDefaultDeviceQueue && deviceQueue) {
        deviceQueue->retain();
        return deviceQueue;
    }

    auto funcCreate = deviceQueueFactory[device->getRenderCoreFamily()];
    DEBUG_BREAK_IF(nullptr == funcCreate);
    deviceQueue = funcCreate(context, device, const_cast<cl_queue_properties &>(properties));

    context->setDefaultDeviceQueue(deviceQueue);

    return deviceQueue;
}

DeviceQueue::~DeviceQueue() {

    for (uint32_t i = 0; i < IndirectHeap::NUM_TYPES; i++) {
        if (heaps[i])
            delete heaps[i];
    }

    if (queueBuffer)
        device->getMemoryManager()->freeGraphicsMemory(queueBuffer);
    if (eventPoolBuffer)
        device->getMemoryManager()->freeGraphicsMemory(eventPoolBuffer);
    if (slbBuffer)
        device->getMemoryManager()->freeGraphicsMemory(slbBuffer);
    if (stackBuffer)
        device->getMemoryManager()->freeGraphicsMemory(stackBuffer);
    if (queueStorageBuffer)
        device->getMemoryManager()->freeGraphicsMemory(queueStorageBuffer);
    if (dshBuffer)
        device->getMemoryManager()->freeGraphicsMemory(dshBuffer);
    if (debugQueue)
        device->getMemoryManager()->freeGraphicsMemory(debugQueue);
    if (context) {
        context->setDefaultDeviceQueue(nullptr);
        context->decRefInternal();
    }
}

cl_int DeviceQueue::getCommandQueueInfo(cl_command_queue_info paramName,
                                        size_t paramValueSize, void *paramValue,
                                        size_t *paramValueSizeRet) {
    return getQueueInfo<DeviceQueue>(this, paramName, paramValueSize, paramValue, paramValueSizeRet);
}

void DeviceQueue::allocateResources() {
    auto &caps = device->getDeviceInfo();

    uint32_t alignedQueueSize = alignUp(queueSize, MemoryConstants::pageSize);
    queueBuffer = device->getMemoryManager()->allocateGraphicsMemory(alignedQueueSize);

    auto eventPoolBufferSize = static_cast<size_t>(caps.maxOnDeviceEvents) * sizeof(IGIL_DeviceEvent) + sizeof(IGIL_EventPool);
    eventPoolBufferSize = alignUp(eventPoolBufferSize, MemoryConstants::pageSize);
    eventPoolBuffer = device->getMemoryManager()->allocateGraphicsMemory(eventPoolBufferSize);

    auto maxEnqueue = static_cast<size_t>(alignedQueueSize) / sizeof(IGIL_CommandHeader);
    auto expectedStackSize = maxEnqueue * sizeof(uint32_t) * 3; // 3 full loads of commands
    expectedStackSize = alignUp(expectedStackSize, MemoryConstants::pageSize);
    stackBuffer = device->getMemoryManager()->allocateGraphicsMemory(expectedStackSize);
    memset(stackBuffer->getUnderlyingBuffer(), 0, stackBuffer->getUnderlyingBufferSize());

    auto queueStorageSize = alignedQueueSize * 2; // place for 2 full loads of queue_t
    queueStorageSize = alignUp(queueStorageSize, MemoryConstants::pageSize);
    queueStorageBuffer = device->getMemoryManager()->allocateGraphicsMemory(queueStorageSize);
    memset(queueStorageBuffer->getUnderlyingBuffer(), 0, queueStorageBuffer->getUnderlyingBufferSize());

    auto &hwHelper = HwHelper::get(device->getHardwareInfo().pPlatform->eRenderCoreFamily);
    const size_t IDTSize = numberOfIDTables * interfaceDescriptorEntries * hwHelper.getInterfaceDescriptorDataSize();

    // Additional padding of PAGE_SIZE for PageFaults just after DSH to satisfy hw requirements
    auto dshSize = (PARALLEL_SCHEDULER_HW_GROUPS + 2) * MAX_DSH_SIZE_PER_ENQUEUE * 8 + IDTSize + colorCalcStateSize + 4096;
    dshSize = alignUp(dshSize, MemoryConstants::pageSize);
    dshBuffer = device->getMemoryManager()->allocateGraphicsMemory(dshSize);

    debugQueue = device->getMemoryManager()->allocateGraphicsMemory(4096);
    debugData = (DebugDataBuffer *)debugQueue->getUnderlyingBuffer();
    memset(debugQueue->getUnderlyingBuffer(), 0, debugQueue->getUnderlyingBufferSize());
}

void DeviceQueue::initDeviceQueue() {
    auto igilCmdQueue = reinterpret_cast<IGIL_CommandQueue *>(queueBuffer->getUnderlyingBuffer());
    auto &caps = device->getDeviceInfo();

    memset(queueBuffer->getUnderlyingBuffer(), 0x0, queueBuffer->getUnderlyingBufferSize());
    igilCmdQueue->m_controls.m_SLBENDoffsetInBytes = -1;
    igilCmdQueue->m_head = IGIL_DEVICE_QUEUE_HEAD_INIT;
    igilCmdQueue->m_size = static_cast<uint32_t>(queueBuffer->getUnderlyingBufferSize() - sizeof(IGIL_CommandQueue));
    igilCmdQueue->m_magic = IGIL_MAGIC_NUMBER;

    auto igilEventPool = reinterpret_cast<IGIL_EventPool *>(eventPoolBuffer->getUnderlyingBuffer());
    memset(eventPoolBuffer->getUnderlyingBuffer(), 0x0, eventPoolBuffer->getUnderlyingBufferSize());
    igilEventPool->m_size = caps.maxOnDeviceEvents;
}

void DeviceQueue::setupExecutionModelDispatch(IndirectHeap &surfaceStateHeap, IndirectHeap &dynamicStateHeap, Kernel *parentKernel, uint32_t parentCount, uint32_t taskCount, HwTimeStamps *hwTimeStamp) {
    setupIndirectState(surfaceStateHeap, dynamicStateHeap, parentKernel, parentCount);
    addExecutionModelCleanUpSection(parentKernel, hwTimeStamp, taskCount);
}

void DeviceQueue::setupIndirectState(IndirectHeap &surfaceStateHeap, IndirectHeap &dynamicStateHeap, Kernel *parentKernel, uint32_t parentIDCount) {
    return;
}

void DeviceQueue::addExecutionModelCleanUpSection(Kernel *parentKernel, HwTimeStamps *hwTimeStamp, uint32_t taskCount) {
    return;
}

void DeviceQueue::resetDeviceQueue() {
    return;
}

void DeviceQueue::dispatchScheduler(CommandQueue &cmdQ, SchedulerKernel &scheduler, PreemptionMode preemptionMode, IndirectHeap *ssh, IndirectHeap *dsh) {
    return;
}

IndirectHeap *DeviceQueue::getIndirectHeap(IndirectHeap::Type type) {
    return nullptr;
}
} // namespace OCLRT
