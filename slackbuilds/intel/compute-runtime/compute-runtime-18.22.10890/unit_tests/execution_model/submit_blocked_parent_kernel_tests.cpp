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

#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/event/hw_timestamps.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/helpers/task_information.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_device_queue.h"

#include "unit_tests/fixtures/execution_model_fixture.h"

#include <memory>

using namespace OCLRT;

class SubmitBlockedParentKernelFixture : public ExecutionModelSchedulerTest,
                                         public testing::Test {
    void SetUp() override {
        ExecutionModelSchedulerTest::SetUp();
    }

    void TearDown() override {
        ExecutionModelSchedulerTest::TearDown();
    }
};

template <typename GfxFamily>
class MockDeviceQueueHwWithCriticalSectionRelease : public DeviceQueueHw<GfxFamily> {
    using BaseClass = DeviceQueueHw<GfxFamily>;

  public:
    MockDeviceQueueHwWithCriticalSectionRelease(Context *context,
                                                Device *device,
                                                cl_queue_properties &properties) : BaseClass(context, device, properties) {}

    bool isEMCriticalSectionFree() override {
        auto igilCmdQueue = reinterpret_cast<IGIL_CommandQueue *>(DeviceQueue::queueBuffer->getUnderlyingBuffer());

        criticalSectioncheckCounter++;

        if (criticalSectioncheckCounter == maxCounter) {
            igilCmdQueue->m_controls.m_CriticalSection = DeviceQueueHw<GfxFamily>::ExecutionModelCriticalSection::Free;
            return true;
        }

        return igilCmdQueue->m_controls.m_CriticalSection == DeviceQueueHw<GfxFamily>::ExecutionModelCriticalSection::Free;
    }

    void setupIndirectState(IndirectHeap &surfaceStateHeap, IndirectHeap &dynamicStateHeap, Kernel *parentKernel, uint32_t parentIDCount) override {
        indirectStateSetup = true;
        return BaseClass::setupIndirectState(surfaceStateHeap, dynamicStateHeap, parentKernel, parentIDCount);
    }
    void addExecutionModelCleanUpSection(Kernel *parentKernel, HwTimeStamps *hwTimeStamp, uint32_t taskCount) override {
        cleanupSectionAdded = true;
        timestampAddedInCleanupSection = hwTimeStamp;
        return BaseClass::addExecutionModelCleanUpSection(parentKernel, hwTimeStamp, taskCount);
    }
    void dispatchScheduler(CommandQueue &cmdQ, SchedulerKernel &scheduler, PreemptionMode preemptionMode, IndirectHeap *ssh, IndirectHeap *dsh) override {
        schedulerDispatched = true;
        return BaseClass::dispatchScheduler(cmdQ, scheduler, preemptionMode, ssh, dsh);
    }

    uint32_t criticalSectioncheckCounter = 0;
    const uint32_t maxCounter = 10;
    bool indirectStateSetup = false;
    bool cleanupSectionAdded = false;
    bool schedulerDispatched = false;
    HwTimeStamps *timestampAddedInCleanupSection = nullptr;
};

HWTEST_F(ParentKernelCommandQueueFixture, givenLockedEMcritcalSectionWhenParentKernelCommandIsSubmittedThenItWaitsForcriticalSectionReleasement) {
    if (device->getSupportedClVersion() >= 20) {
        cl_queue_properties properties[3] = {0};
        MockParentKernel *parentKernel = MockParentKernel::create(*device);
        MockDeviceQueueHwWithCriticalSectionRelease<FamilyType> mockDevQueue(context, device, properties[0]);
        parentKernel->createReflectionSurface();
        context->setDefaultDeviceQueue(&mockDevQueue);

        mockDevQueue.acquireEMCriticalSection();

        size_t heapSize = 20;
        size_t dshSize = mockDevQueue.getDshBuffer()->getUnderlyingBufferSize();

        IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
        pCmdQ->allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, dshSize, dsh);
        pCmdQ->allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, heapSize, ioh);
        pCmdQ->allocateHeapMemory(IndirectHeap::SURFACE_STATE, heapSize, ssh);

        dsh->getSpace(mockDevQueue.getDshOffset());

        size_t minSizeSSHForEM = KernelCommandsHelper<FamilyType>::template getSizeRequiredForExecutionModel<IndirectHeap::SURFACE_STATE>(*parentKernel);

        KernelOperation *blockedCommandData = new KernelOperation(std::unique_ptr<LinearStream>(new LinearStream()),
                                                                  std::unique_ptr<IndirectHeap>(dsh),
                                                                  std::unique_ptr<IndirectHeap>(ioh),
                                                                  std::unique_ptr<IndirectHeap>(ssh),
                                                                  *pCmdQ->getDevice().getMemoryManager());

        blockedCommandData->surfaceStateHeapSizeEM = minSizeSSHForEM;
        PreemptionMode preemptionMode = device->getPreemptionMode();
        std::vector<Surface *> surfaces;
        auto *cmdComputeKernel = new CommandComputeKernel(*pCmdQ, device->getCommandStreamReceiver(),
                                                          std::unique_ptr<KernelOperation>(blockedCommandData), surfaces, false, false, false, nullptr, preemptionMode, parentKernel, 1);

        cmdComputeKernel->submit(0, false);

        EXPECT_EQ(mockDevQueue.maxCounter, mockDevQueue.criticalSectioncheckCounter);
        delete cmdComputeKernel;
        delete parentKernel;
    }
}

HWTEST_F(ParentKernelCommandQueueFixture, givenParentKernelWhenCommandIsSubmittedThenPassedDshIsUsed) {
    if (device->getSupportedClVersion() >= 20) {
        cl_queue_properties properties[3] = {0};
        MockParentKernel *parentKernel = MockParentKernel::create(*device);
        MockDeviceQueueHwWithCriticalSectionRelease<FamilyType> mockDevQueue(context, device, properties[0]);
        parentKernel->createReflectionSurface();
        context->setDefaultDeviceQueue(&mockDevQueue);

        auto *dshOfDevQueue = mockDevQueue.getIndirectHeap(IndirectHeap::DYNAMIC_STATE);

        size_t heapSize = 20;
        size_t dshSize = mockDevQueue.getDshBuffer()->getUnderlyingBufferSize();

        IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
        pCmdQ->allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, dshSize, dsh);
        pCmdQ->allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, heapSize, ioh);
        pCmdQ->allocateHeapMemory(IndirectHeap::SURFACE_STATE, heapSize, ssh);

        // add initial offset of colorCalState
        dsh->getSpace(DeviceQueue::colorCalcStateSize);

        uint64_t ValueToFillDsh = 5;
        uint64_t *dshVal = (uint64_t *)dsh->getSpace(sizeof(uint64_t));

        // Fill Interface Descriptor Data
        *dshVal = ValueToFillDsh;

        // Move to parent DSH Offset
        size_t alignToOffsetDshSize = mockDevQueue.getDshOffset() - DeviceQueue::colorCalcStateSize - sizeof(uint64_t);
        dsh->getSpace(alignToOffsetDshSize);

        // Fill with pattern
        dshVal = (uint64_t *)dsh->getSpace(sizeof(uint64_t));
        *dshVal = ValueToFillDsh;

        size_t usedDSHBeforeSubmit = dshOfDevQueue->getUsed();

        uint32_t colorCalcSizeDevQueue = DeviceQueue::colorCalcStateSize;
        EXPECT_EQ(colorCalcSizeDevQueue, usedDSHBeforeSubmit);

        KernelOperation *blockedCommandData = new KernelOperation(std::unique_ptr<LinearStream>(new LinearStream()),
                                                                  std::unique_ptr<IndirectHeap>(dsh),
                                                                  std::unique_ptr<IndirectHeap>(ioh),
                                                                  std::unique_ptr<IndirectHeap>(ssh),
                                                                  *pCmdQ->getDevice().getMemoryManager());

        size_t minSizeSSHForEM = KernelCommandsHelper<FamilyType>::template getSizeRequiredForExecutionModel<IndirectHeap::SURFACE_STATE>(*parentKernel);

        blockedCommandData->surfaceStateHeapSizeEM = minSizeSSHForEM;
        PreemptionMode preemptionMode = device->getPreemptionMode();
        std::vector<Surface *> surfaces;
        auto *cmdComputeKernel = new CommandComputeKernel(*pCmdQ, device->getCommandStreamReceiver(),
                                                          std::unique_ptr<KernelOperation>(blockedCommandData), surfaces, false, false, false, nullptr, preemptionMode, parentKernel, 1);

        cmdComputeKernel->submit(0, false);

        //device queue dsh is not changed
        size_t usedDSHAfterSubmit = dshOfDevQueue->getUsed();
        EXPECT_EQ(usedDSHAfterSubmit, usedDSHAfterSubmit);

        delete cmdComputeKernel;
        delete parentKernel;
    }
}

HWTEST_F(ParentKernelCommandQueueFixture, givenParentKernelWhenCommandIsSubmittedThenIndirectStateAndEMCleanupSectionIsSetup) {
    if (device->getSupportedClVersion() >= 20) {
        cl_queue_properties properties[3] = {0};
        MockParentKernel *parentKernel = MockParentKernel::create(*device);
        MockDeviceQueueHwWithCriticalSectionRelease<FamilyType> mockDevQueue(context, device, properties[0]);
        parentKernel->createReflectionSurface();
        context->setDefaultDeviceQueue(&mockDevQueue);

        size_t heapSize = 20;
        size_t dshSize = mockDevQueue.getDshBuffer()->getUnderlyingBufferSize();

        IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
        pCmdQ->allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, dshSize, dsh);
        pCmdQ->allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, heapSize, ioh);
        pCmdQ->allocateHeapMemory(IndirectHeap::SURFACE_STATE, heapSize, ssh);

        dsh->getSpace(mockDevQueue.getDshOffset());

        KernelOperation *blockedCommandData = new KernelOperation(std::unique_ptr<LinearStream>(new LinearStream()),
                                                                  std::unique_ptr<IndirectHeap>(dsh),
                                                                  std::unique_ptr<IndirectHeap>(ioh),
                                                                  std::unique_ptr<IndirectHeap>(ssh),
                                                                  *pCmdQ->getDevice().getMemoryManager());

        size_t minSizeSSHForEM = KernelCommandsHelper<FamilyType>::template getSizeRequiredForExecutionModel<IndirectHeap::SURFACE_STATE>(*parentKernel);

        blockedCommandData->surfaceStateHeapSizeEM = minSizeSSHForEM;
        PreemptionMode preemptionMode = device->getPreemptionMode();
        std::vector<Surface *> surfaces;
        auto *cmdComputeKernel = new CommandComputeKernel(*pCmdQ, device->getCommandStreamReceiver(),
                                                          std::unique_ptr<KernelOperation>(blockedCommandData), surfaces, false, false, false, nullptr, preemptionMode, parentKernel, 1);

        cmdComputeKernel->submit(0, false);

        EXPECT_TRUE(mockDevQueue.indirectStateSetup);
        EXPECT_TRUE(mockDevQueue.cleanupSectionAdded);

        delete cmdComputeKernel;
        delete parentKernel;
    }
}

HWTEST_F(ParentKernelCommandQueueFixture, givenBlockedParentKernelWithProfilingWhenCommandIsSubmittedThenEMCleanupSectionsSetsCompleteTimestamp) {
    if (device->getSupportedClVersion() >= 20) {
        cl_queue_properties properties[3] = {0};
        MockParentKernel *parentKernel = MockParentKernel::create(*device);
        MockDeviceQueueHwWithCriticalSectionRelease<FamilyType> mockDevQueue(context, device, properties[0]);
        parentKernel->createReflectionSurface();
        context->setDefaultDeviceQueue(&mockDevQueue);

        size_t heapSize = 20;
        size_t dshSize = mockDevQueue.getDshBuffer()->getUnderlyingBufferSize();
        IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
        pCmdQ->allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, dshSize, dsh);
        pCmdQ->allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, heapSize, ioh);
        pCmdQ->allocateHeapMemory(IndirectHeap::SURFACE_STATE, heapSize, ssh);
        dsh->getSpace(mockDevQueue.getDshOffset());

        KernelOperation *blockedCommandData = new KernelOperation(std::unique_ptr<LinearStream>(new LinearStream()),
                                                                  std::unique_ptr<IndirectHeap>(dsh),
                                                                  std::unique_ptr<IndirectHeap>(ioh),
                                                                  std::unique_ptr<IndirectHeap>(ssh),
                                                                  *pCmdQ->getDevice().getMemoryManager());

        size_t minSizeSSHForEM = KernelCommandsHelper<FamilyType>::template getSizeRequiredForExecutionModel<IndirectHeap::SURFACE_STATE>(*parentKernel);

        blockedCommandData->surfaceStateHeapSizeEM = minSizeSSHForEM;
        PreemptionMode preemptionMode = device->getPreemptionMode();
        std::vector<Surface *> surfaces;
        auto *cmdComputeKernel = new CommandComputeKernel(*pCmdQ, device->getCommandStreamReceiver(),
                                                          std::unique_ptr<KernelOperation>(blockedCommandData), surfaces, false, false, false, nullptr, preemptionMode, parentKernel, 1);

        HwTimeStamps timestamp;

        cmdComputeKernel->timestamp = &timestamp;
        cmdComputeKernel->submit(0, false);

        EXPECT_TRUE(mockDevQueue.cleanupSectionAdded);
        EXPECT_EQ(mockDevQueue.timestampAddedInCleanupSection, &timestamp);

        delete cmdComputeKernel;
        delete parentKernel;
    }
}

HWTEST_F(ParentKernelCommandQueueFixture, givenParentKernelWhenCommandIsSubmittedThenSchedulerIsDispatched) {
    if (device->getSupportedClVersion() >= 20) {
        cl_queue_properties properties[3] = {0};
        MockParentKernel *parentKernel = MockParentKernel::create(*device);
        MockDeviceQueueHwWithCriticalSectionRelease<FamilyType> mockDevQueue(context, device, properties[0]);
        parentKernel->createReflectionSurface();
        context->setDefaultDeviceQueue(&mockDevQueue);

        size_t heapSize = 20;
        size_t dshSize = mockDevQueue.getDshBuffer()->getUnderlyingBufferSize();

        IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
        pCmdQ->allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, dshSize, dsh);
        pCmdQ->allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, heapSize, ioh);
        pCmdQ->allocateHeapMemory(IndirectHeap::SURFACE_STATE, heapSize, ssh);
        dsh->getSpace(mockDevQueue.getDshOffset());

        KernelOperation *blockedCommandData = new KernelOperation(std::unique_ptr<LinearStream>(new LinearStream()),
                                                                  std::unique_ptr<IndirectHeap>(dsh),
                                                                  std::unique_ptr<IndirectHeap>(ioh),
                                                                  std::unique_ptr<IndirectHeap>(ssh),
                                                                  *pCmdQ->getDevice().getMemoryManager());

        size_t minSizeSSHForEM = KernelCommandsHelper<FamilyType>::template getSizeRequiredForExecutionModel<IndirectHeap::SURFACE_STATE>(*parentKernel);

        blockedCommandData->surfaceStateHeapSizeEM = minSizeSSHForEM;
        PreemptionMode preemptionMode = device->getPreemptionMode();
        std::vector<Surface *> surfaces;
        auto *cmdComputeKernel = new CommandComputeKernel(*pCmdQ, device->getCommandStreamReceiver(),
                                                          std::unique_ptr<KernelOperation>(blockedCommandData), surfaces, false, false, false, nullptr, preemptionMode, parentKernel, 1);

        cmdComputeKernel->submit(0, false);

        EXPECT_TRUE(mockDevQueue.schedulerDispatched);

        delete cmdComputeKernel;
        delete parentKernel;
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, ParentKernelCommandQueueFixture, givenUsedCommandQueueHeapshenParentKernelIsSubmittedThenQueueHeapsAreNotUsed) {
    if (device->getSupportedClVersion() >= 20) {
        cl_queue_properties properties[3] = {0};
        MockParentKernel *parentKernel = MockParentKernel::create(*device);
        MockDeviceQueueHw<FamilyType> mockDevQueue(context, device, properties[0]);
        parentKernel->createReflectionSurface();
        context->setDefaultDeviceQueue(&mockDevQueue);

        MockCommandQueue cmdQ(context, device, properties);

        size_t minSizeSSHForEM = KernelCommandsHelper<FamilyType>::template getSizeRequiredForExecutionModel<IndirectHeap::SURFACE_STATE>(*parentKernel);

        size_t heapSize = 20;

        size_t dshSize = mockDevQueue.getDshBuffer()->getUnderlyingBufferSize();
        IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
        pCmdQ->allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, dshSize, dsh);
        pCmdQ->allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, heapSize, ioh);
        pCmdQ->allocateHeapMemory(IndirectHeap::SURFACE_STATE, heapSize, ssh);

        dsh->getSpace(mockDevQueue.getDshOffset());

        auto &queueSsh = cmdQ.getIndirectHeap(IndirectHeap::SURFACE_STATE, 100);
        auto &queueDsh = cmdQ.getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 100);
        auto &queueIoh = cmdQ.getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 100);

        size_t usedSize = 4u;

        queueSsh.getSpace(usedSize);
        queueDsh.getSpace(usedSize);
        queueIoh.getSpace(usedSize);

        KernelOperation *blockedCommandData = new KernelOperation(std::unique_ptr<LinearStream>(new LinearStream()),
                                                                  std::unique_ptr<IndirectHeap>(dsh),
                                                                  std::unique_ptr<IndirectHeap>(ioh),
                                                                  std::unique_ptr<IndirectHeap>(ssh),
                                                                  *pCmdQ->getDevice().getMemoryManager());

        blockedCommandData->surfaceStateHeapSizeEM = minSizeSSHForEM;
        PreemptionMode preemptionMode = device->getPreemptionMode();
        std::vector<Surface *> surfaces;
        auto *cmdComputeKernel = new CommandComputeKernel(cmdQ, device->getCommandStreamReceiver(),
                                                          std::unique_ptr<KernelOperation>(blockedCommandData), surfaces, false, false, false, nullptr, preemptionMode, parentKernel, 1);

        cmdComputeKernel->submit(0, false);

        EXPECT_FALSE(cmdQ.releaseIndirectHeapCalled);
        EXPECT_EQ(usedSize, queueDsh.getUsed());
        EXPECT_EQ(usedSize, queueIoh.getUsed());
        EXPECT_EQ(usedSize, queueSsh.getUsed());

        delete cmdComputeKernel;
        delete parentKernel;
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, ParentKernelCommandQueueFixture, givenNotUsedSSHWhenParentKernelIsSubmittedThenExistingSSHIsUsed) {
    if (device->getSupportedClVersion() >= 20) {
        cl_queue_properties properties[3] = {0};
        MockParentKernel *parentKernel = MockParentKernel::create(*device);
        MockDeviceQueueHw<FamilyType> mockDevQueue(context, device, properties[0]);
        parentKernel->createReflectionSurface();
        context->setDefaultDeviceQueue(&mockDevQueue);

        size_t minSizeSSHForEM = KernelCommandsHelper<FamilyType>::template getSizeRequiredForExecutionModel<IndirectHeap::SURFACE_STATE>(*parentKernel);

        size_t heapSize = 20;

        size_t dshSize = mockDevQueue.getDshBuffer()->getUnderlyingBufferSize();
        size_t sshSize = 1000;
        IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
        pCmdQ->allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, dshSize, dsh);
        pCmdQ->allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, heapSize, ioh);
        pCmdQ->allocateHeapMemory(IndirectHeap::SURFACE_STATE, sshSize, ssh);
        dsh->getSpace(mockDevQueue.getDshOffset());

        EXPECT_EQ(0u, ssh->getUsed());

        pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, sshSize);

        void *sshBuffer = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u).getCpuBase();

        KernelOperation *blockedCommandData = new KernelOperation(std::unique_ptr<LinearStream>(new LinearStream()),
                                                                  std::unique_ptr<IndirectHeap>(dsh),
                                                                  std::unique_ptr<IndirectHeap>(ioh),
                                                                  std::unique_ptr<IndirectHeap>(ssh),
                                                                  *pCmdQ->getDevice().getMemoryManager());

        blockedCommandData->surfaceStateHeapSizeEM = minSizeSSHForEM;
        PreemptionMode preemptionMode = device->getPreemptionMode();
        std::vector<Surface *> surfaces;
        auto *cmdComputeKernel = new CommandComputeKernel(*pCmdQ, device->getCommandStreamReceiver(),
                                                          std::unique_ptr<KernelOperation>(blockedCommandData), surfaces, false, false, false, nullptr, preemptionMode, parentKernel, 1);

        cmdComputeKernel->submit(0, false);

        void *newSshBuffer = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u).getCpuBase();

        EXPECT_EQ(sshBuffer, newSshBuffer);

        delete cmdComputeKernel;
        delete parentKernel;
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, ParentKernelCommandQueueFixture, givenBlockedCommandQueueWhenDispatchWalkerIsCalledThenHeapsHaveProperSizes) {
    if (device->getSupportedClVersion() >= 20) {
        cl_queue_properties properties[3] = {0};
        std::unique_ptr<MockParentKernel> parentKernel(MockParentKernel::create(*device));

        MockDeviceQueueHw<FamilyType> mockDevQueue(context, device, properties[0]);
        parentKernel->createReflectionSurface();
        context->setDefaultDeviceQueue(&mockDevQueue);

        KernelOperation *blockedCommandsData = nullptr;
        const size_t globalOffsets[3] = {0, 0, 0};
        const size_t workItems[3] = {1, 1, 1};

        GpgpuWalkerHelper<FamilyType>::dispatchWalker(*pCmdQ,
                                                      *parentKernel,
                                                      1,
                                                      globalOffsets,
                                                      workItems,
                                                      nullptr,
                                                      0,
                                                      nullptr,
                                                      &blockedCommandsData,
                                                      nullptr,
                                                      nullptr,
                                                      device->getPreemptionMode(),
                                                      true);

        EXPECT_NE(nullptr, blockedCommandsData);
        EXPECT_EQ(blockedCommandsData->dsh->getMaxAvailableSpace(), mockDevQueue.getDshBuffer()->getUnderlyingBufferSize());
        EXPECT_EQ(blockedCommandsData->dsh, blockedCommandsData->ioh);

        EXPECT_NE(nullptr, blockedCommandsData->dsh->getGraphicsAllocation());
        EXPECT_NE(nullptr, blockedCommandsData->ioh->getGraphicsAllocation());
        EXPECT_NE(nullptr, blockedCommandsData->ssh->getGraphicsAllocation());
        EXPECT_EQ(blockedCommandsData->dsh->getGraphicsAllocation(), blockedCommandsData->ioh->getGraphicsAllocation());

        delete blockedCommandsData;
    }
}
