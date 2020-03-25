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

#include "runtime/event/event.h"
#include "runtime/memory_manager/surface.h"
#include "unit_tests/fixtures/enqueue_handler_fixture.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_mdi.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"

#include "test.h"

using namespace OCLRT;

HWTEST_F(EnqueueHandlerTest, enqueueHandlerWithKernelCallsProcessEvictionOnCSR) {
    int32_t tag;
    auto csr = new MockCsrBase<FamilyType>(tag);
    pDevice->resetCommandStreamReceiver(csr);

    MockKernelWithInternals mockKernel(*pDevice);
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(context, pDevice, 0));

    size_t gws[] = {1, 1, 1};
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_TRUE(csr->processEvictionCalled);
}

HWTEST_F(EnqueueHandlerTest, enqueueHandlerCallOnEnqueueMarkerDoesntCallProcessEvictionOnCSR) {
    int32_t tag;
    auto csr = new MockCsrBase<FamilyType>(tag);
    pDevice->resetCommandStreamReceiver(csr);

    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(context, pDevice, 0));

    mockCmdQ->enqueueMarkerWithWaitList(
        0,
        nullptr,
        nullptr);

    EXPECT_FALSE(csr->processEvictionCalled);
    EXPECT_EQ(0u, csr->madeResidentGfxAllocations.size());
    EXPECT_EQ(0u, csr->madeNonResidentGfxAllocations.size());
}

HWTEST_F(EnqueueHandlerTest, enqueueHandlerForMarkerOnUnblockedQueueDoesntIncrementTaskLevel) {
    int32_t tag;
    auto csr = new MockCsrBase<FamilyType>(tag);
    pDevice->resetCommandStreamReceiver(csr);

    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(context, pDevice, 0));

    // put queue into initial unblocked state
    mockCmdQ->taskLevel = 0;

    mockCmdQ->enqueueMarkerWithWaitList(
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(0u, mockCmdQ->taskLevel);
}

HWTEST_F(EnqueueHandlerTest, enqueueHandlerForMarkerOnBlockedQueueShouldNotIncrementTaskLevel) {
    int32_t tag;
    auto csr = new MockCsrBase<FamilyType>(tag);
    pDevice->resetCommandStreamReceiver(csr);

    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(context, pDevice, 0));

    // put queue into initial blocked state
    mockCmdQ->taskLevel = Event::eventNotReady;

    mockCmdQ->enqueueMarkerWithWaitList(
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(Event::eventNotReady, mockCmdQ->taskLevel);
}

HWTEST_F(EnqueueHandlerTest, enqueueBlockedWithoutReturnEventCreatesVirtualEventAndIncremetsCommandQueueInternalRefCount) {

    MockKernelWithInternals kernelInternals(*pDevice, context);

    Kernel *kernel = kernelInternals.mockKernel;

    MockMultiDispatchInfo multiDispatchInfo(kernel);

    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pDevice, 0);

    // put queue into initial blocked state
    mockCmdQ->taskLevel = Event::eventNotReady;

    auto initialRefCountInternal = mockCmdQ->getRefInternalCount();

    bool blocking = false;
    mockCmdQ->template enqueueHandler<CL_COMMAND_NDRANGE_KERNEL>(nullptr,
                                                                 0,
                                                                 blocking,
                                                                 multiDispatchInfo,
                                                                 0,
                                                                 nullptr,
                                                                 nullptr);

    EXPECT_NE(nullptr, mockCmdQ->virtualEvent);

    auto refCountInternal = mockCmdQ->getRefInternalCount();
    EXPECT_EQ(initialRefCountInternal + 1, refCountInternal);

    mockCmdQ->virtualEvent->setStatus(CL_COMPLETE);
    mockCmdQ->isQueueBlocked();
    mockCmdQ->release();
}

HWTEST_F(EnqueueHandlerTest, enqueueBlockedSetsVirtualEventAsCurrentCmdQVirtualEvent) {

    MockKernelWithInternals kernelInternals(*pDevice, context);

    Kernel *kernel = kernelInternals.mockKernel;

    MockMultiDispatchInfo multiDispatchInfo(kernel);

    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pDevice, 0);

    // put queue into initial blocked state
    mockCmdQ->taskLevel = Event::eventNotReady;

    bool blocking = false;
    mockCmdQ->template enqueueHandler<CL_COMMAND_NDRANGE_KERNEL>(nullptr,
                                                                 0,
                                                                 blocking,
                                                                 multiDispatchInfo,
                                                                 0,
                                                                 nullptr,
                                                                 nullptr);

    ASSERT_NE(nullptr, mockCmdQ->virtualEvent);

    EXPECT_TRUE(mockCmdQ->virtualEvent->isCurrentCmdQVirtualEvent());

    mockCmdQ->virtualEvent->setStatus(CL_COMPLETE);
    mockCmdQ->isQueueBlocked();
    mockCmdQ->release();
}

HWTEST_F(EnqueueHandlerTest, enqueueBlockedUnsetsCurrentCmdQVirtualEventForPreviousVirtualEvent) {

    UserEvent userEvent;
    cl_event clUserEvent = &userEvent;

    MockKernelWithInternals kernelInternals(*pDevice, context);

    Kernel *kernel = kernelInternals.mockKernel;

    MockMultiDispatchInfo multiDispatchInfo(kernel);

    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pDevice, 0);

    // put queue into initial blocked state with userEvent

    bool blocking = false;
    mockCmdQ->template enqueueHandler<CL_COMMAND_NDRANGE_KERNEL>(nullptr,
                                                                 0,
                                                                 blocking,
                                                                 multiDispatchInfo,
                                                                 1,
                                                                 &clUserEvent,
                                                                 nullptr);

    ASSERT_NE(nullptr, mockCmdQ->virtualEvent);
    Event *previousVirtualEvent = mockCmdQ->virtualEvent;

    mockCmdQ->template enqueueHandler<CL_COMMAND_NDRANGE_KERNEL>(nullptr,
                                                                 0,
                                                                 blocking,
                                                                 multiDispatchInfo,
                                                                 0,
                                                                 nullptr,
                                                                 nullptr);

    EXPECT_FALSE(previousVirtualEvent->isCurrentCmdQVirtualEvent());
    EXPECT_TRUE(mockCmdQ->virtualEvent->isCurrentCmdQVirtualEvent());

    mockCmdQ->release();
}

HWTEST_F(EnqueueHandlerTest, enqueueWithOutputEventRegistersEvent) {
    MockKernelWithInternals kernelInternals(*pDevice, context);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(kernel);
    cl_event outputEvent = nullptr;

    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pDevice, 0);

    bool blocking = false;
    mockCmdQ->template enqueueHandler<CL_COMMAND_NDRANGE_KERNEL>(nullptr,
                                                                 0,
                                                                 blocking,
                                                                 multiDispatchInfo,
                                                                 0,
                                                                 nullptr,
                                                                 &outputEvent);
    ASSERT_NE(nullptr, outputEvent);
    Event *event = castToObjectOrAbort<Event>(outputEvent);
    ASSERT_NE(nullptr, event);

    event->release();
    mockCmdQ->release();
}

HWTEST_F(EnqueueHandlerTest, givenEnqueueHandlerWhenAddPatchInfoCommentsForAUBDumpIsNotSetThenPatchInfoDataIsNotTransferredToCSR) {
    auto csr = new MockCsrHw2<FamilyType>(*platformDevices[0]);
    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(csr->getMemoryManager());
    csr->overwriteFlatBatchBufferHelper(mockHelper);
    pDevice->resetCommandStreamReceiver(csr);

    MockKernelWithInternals mockKernel(*pDevice);
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(context, pDevice, 0));

    size_t gws[] = {1, 1, 1};

    PatchInfoData patchInfoData = {0xaaaaaaaa, 0, PatchInfoAllocationType::KernelArg, 0xbbbbbbbb, 0, PatchInfoAllocationType::IndirectObjectHeap};
    mockKernel.mockKernel->getPatchInfoDataList().push_back(patchInfoData);

    EXPECT_CALL(*mockHelper, setPatchInfoData(::testing::_)).Times(0);
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
}

HWTEST_F(EnqueueHandlerTest, givenEnqueueHandlerWhenAddPatchInfoCommentsForAUBDumpIsSetThenPatchInfoDataIsTransferredToCSR) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);

    auto csr = new MockCsrHw2<FamilyType>(*platformDevices[0]);
    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(csr->getMemoryManager());
    csr->overwriteFlatBatchBufferHelper(mockHelper);
    pDevice->resetCommandStreamReceiver(csr);

    MockKernelWithInternals mockKernel(*pDevice);
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(context, pDevice, 0));

    size_t gws[] = {1, 1, 1};

    PatchInfoData patchInfoData = {0xaaaaaaaa, 0, PatchInfoAllocationType::KernelArg, 0xbbbbbbbb, 0, PatchInfoAllocationType::IndirectObjectHeap};
    mockKernel.mockKernel->getPatchInfoDataList().push_back(patchInfoData);

    EXPECT_CALL(*mockHelper, setPatchInfoData(::testing::_)).Times(8);
    EXPECT_CALL(*mockHelper, registerCommandChunk(::testing::_)).Times(1);
    EXPECT_CALL(*mockHelper, registerBatchBufferStartAddress(::testing::_, ::testing::_)).Times(1);
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
}

HWTEST_F(EnqueueHandlerTest, givenExternallySynchronizedParentEventWhenRequestingEnqueueWithoutGpuSubmissionThenTaskCountIsNotInherited) {
    struct ExternallySynchEvent : Event {
        ExternallySynchEvent() : Event(nullptr, CL_COMMAND_MARKER, 0, 0) {
            transitionExecutionStatus(CL_COMPLETE);
            this->updateTaskCount(7);
        }
        bool isExternallySynchronized() const override {
            return true;
        }
    };
    ExternallySynchEvent synchEvent;
    cl_event inEv = &synchEvent;
    cl_event outEv = nullptr;

    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pDevice, 0);
    bool blocking = false;
    MultiDispatchInfo emptyDispatchInfo;
    mockCmdQ->template enqueueHandler<CL_COMMAND_MARKER>(nullptr,
                                                         0,
                                                         blocking,
                                                         emptyDispatchInfo,
                                                         1U,
                                                         &inEv,
                                                         &outEv);
    Event *ouputEvent = castToObject<Event>(outEv);
    ASSERT_NE(nullptr, ouputEvent);
    EXPECT_EQ(0U, ouputEvent->peekTaskCount());

    ouputEvent->release();
    mockCmdQ->release();
}
