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

#include "runtime/os_interface/windows/gdi_interface.h"
#include "runtime/os_interface/windows/wddm/wddm23.h"
#include "unit_tests/mocks/mock_wddm23.h"
#include "unit_tests/os_interface/windows/gdi_dll_fixture.h"
#include "test.h"

using namespace OCLRT;

struct Wddm23Tests : public ::testing::Test, GdiDllFixture {
    void SetUp() override {
        GdiDllFixture::SetUp();
        wddm.reset(static_cast<WddmMock23 *>(Wddm::createWddm(WddmInterfaceVersion::Wddm23)));
    }

    void TearDown() override {
        GdiDllFixture::TearDown();
    }

    std::unique_ptr<WddmMock23> wddm;
};

TEST_F(Wddm23Tests, whenCreateMonitoredFenceCalledThenDoNothing) {
    EXPECT_TRUE(wddm->createMonitoredFence());
    EXPECT_TRUE(nullptr == wddm->getMonitoredFence().cpuAddress);
    EXPECT_EQ(0u, wddm->getMonitoredFence().currentFenceValue);
    EXPECT_EQ(static_cast<D3DKMT_HANDLE>(0), wddm->getMonitoredFence().fenceHandle);
    EXPECT_EQ(static_cast<D3DGPU_VIRTUAL_ADDRESS>(0), wddm->getMonitoredFence().gpuAddress);
    EXPECT_EQ(0u, wddm->getMonitoredFence().lastSubmittedFence);
}

HWTEST_F(Wddm23Tests, whenCreateContextIsCalledThenEnableHwQueues) {
    wddm->init<FamilyType>();
    EXPECT_TRUE(wddm->hwQueuesSupported());
    EXPECT_EQ(1u, getCreateContextDataFcn()->Flags.HwQueueSupported);
}

TEST_F(Wddm23Tests, whenCreateHwQueueIsCalledThenSetAllRequiredFieldsAndMonitoredFence) {
    EXPECT_EQ(0u, wddm->hwQueueHandle);
    wddm->context = 1;

    wddm->createHwQueue();

    EXPECT_EQ(wddm->context, getCreateHwQueueDataFcn()->hHwContext);
    EXPECT_EQ(0u, getCreateHwQueueDataFcn()->PrivateDriverDataSize);
    EXPECT_EQ(nullptr, getCreateHwQueueDataFcn()->pPrivateDriverData);

    EXPECT_TRUE(nullptr != wddm->getMonitoredFence().cpuAddress);
    EXPECT_EQ(1u, wddm->getMonitoredFence().currentFenceValue);
    EXPECT_NE(static_cast<D3DKMT_HANDLE>(0), wddm->getMonitoredFence().fenceHandle);
    EXPECT_NE(static_cast<D3DGPU_VIRTUAL_ADDRESS>(0), wddm->getMonitoredFence().gpuAddress);
    EXPECT_EQ(0u, wddm->getMonitoredFence().lastSubmittedFence);
}

TEST_F(Wddm23Tests, givenPreemptionModeWhenCreateHwQueueCalledThenSetGpuTimeoutIfEnabled) {
    wddm->setPreemptionMode(PreemptionMode::Disabled);
    wddm->createHwQueue();
    EXPECT_EQ(0u, getCreateHwQueueDataFcn()->Flags.DisableGpuTimeout);

    wddm->setPreemptionMode(PreemptionMode::MidBatch);
    wddm->createHwQueue();
    EXPECT_EQ(1u, getCreateHwQueueDataFcn()->Flags.DisableGpuTimeout);
}

HWTEST_F(Wddm23Tests, whenDestroyHwQueueCalledThenPassExistingHandle) {
    wddm->init<FamilyType>();
    wddm->hwQueueHandle = 123;
    wddm->destroyHwQueue();
    EXPECT_EQ(wddm->hwQueueHandle, getDestroyHwQueueDataFcn()->hHwQueue);

    wddm->hwQueueHandle = 0;
    wddm->destroyHwQueue();
    EXPECT_NE(wddm->hwQueueHandle, getDestroyHwQueueDataFcn()->hHwQueue); // gdi not called when 0
}

HWTEST_F(Wddm23Tests, whenObjectIsDestructedThenDestroyHwQueue) {
    wddm->init<FamilyType>();
    D3DKMT_HANDLE hwQueue = 123;
    wddm->hwQueueHandle = hwQueue;
    wddm.reset(nullptr);
    EXPECT_EQ(hwQueue, getDestroyHwQueueDataFcn()->hHwQueue);
}

HWTEST_F(Wddm23Tests, givenCmdBufferWhenSubmitCalledThenSetAllRequiredFiledsAndUpdateMonitoredFence) {
    wddm->init<FamilyType>();
    uint64_t cmdBufferAddress = 123;
    size_t cmdSize = 456;
    COMMAND_BUFFER_HEADER cmdBufferHeader = {};

    EXPECT_EQ(1u, wddm->getMonitoredFence().currentFenceValue);
    EXPECT_EQ(0u, wddm->getMonitoredFence().lastSubmittedFence);

    wddm->submit(cmdBufferAddress, cmdSize, &cmdBufferHeader);

    EXPECT_EQ(cmdBufferAddress, getSubmitCommandToHwQueueDataFcn()->CommandBuffer);
    EXPECT_EQ(static_cast<UINT>(cmdSize), getSubmitCommandToHwQueueDataFcn()->CommandLength);
    EXPECT_EQ(wddm->hwQueueHandle, getSubmitCommandToHwQueueDataFcn()->hHwQueue);
    EXPECT_EQ(wddm->getMonitoredFence().fenceHandle, getSubmitCommandToHwQueueDataFcn()->HwQueueProgressFenceId);
    EXPECT_EQ(&cmdBufferHeader, getSubmitCommandToHwQueueDataFcn()->pPrivateDriverData);
    EXPECT_EQ(static_cast<UINT>(sizeof(COMMAND_BUFFER_HEADER)), getSubmitCommandToHwQueueDataFcn()->PrivateDriverDataSize);

    EXPECT_EQ(wddm->getMonitoredFence().gpuAddress, cmdBufferHeader.MonitorFenceVA);
    EXPECT_EQ(wddm->getMonitoredFence().lastSubmittedFence, cmdBufferHeader.MonitorFenceValue);
    EXPECT_EQ(2u, wddm->getMonitoredFence().currentFenceValue);
    EXPECT_EQ(1u, wddm->getMonitoredFence().lastSubmittedFence);
}

HWTEST_F(Wddm23Tests, givenCurrentPendingFenceValueGreaterThanPendingFenceValueWhenSubmitCalledThenCallWaitOnGpu) {
    wddm->init<FamilyType>();
    uint64_t cmdBufferAddress = 123;
    size_t cmdSize = 456;
    COMMAND_BUFFER_HEADER cmdBufferHeader = {};

    *wddm->pagingFenceAddress = 1;
    wddm->currentPagingFenceValue = 1;
    wddm->submit(cmdBufferAddress, cmdSize, &cmdBufferHeader);
    EXPECT_EQ(0u, wddm->waitOnGPUCalled);

    wddm->currentPagingFenceValue = 2;
    wddm->submit(cmdBufferAddress, cmdSize, &cmdBufferHeader);
    EXPECT_EQ(1u, wddm->waitOnGPUCalled);
}

HWTEST_F(Wddm23Tests, whenInitCalledThenInitializeNewGdiDDIsAndCallToCreateHwQueue) {
    EXPECT_EQ(nullptr, wddm->gdi->createHwQueue.mFunc);
    EXPECT_EQ(nullptr, wddm->gdi->destroyHwQueue.mFunc);
    EXPECT_EQ(nullptr, wddm->gdi->submitCommandToHwQueue.mFunc);

    EXPECT_TRUE(wddm->init<FamilyType>());
    EXPECT_EQ(1u, wddm->createHwQueueCalled);

    EXPECT_NE(nullptr, wddm->gdi->createHwQueue.mFunc);
    EXPECT_NE(nullptr, wddm->gdi->destroyHwQueue.mFunc);
    EXPECT_NE(nullptr, wddm->gdi->submitCommandToHwQueue.mFunc);
}

HWTEST_F(Wddm23Tests, whenCreateHwQueueFailedThenReturnFalseFromInit) {
    wddm->forceCreateHwQueueFail = true;
    EXPECT_FALSE(wddm->init<FamilyType>());
}

HWTEST_F(Wddm23Tests, givenFailureOnGdiInitializationWhenCreatingHwQueueThenReturnFailure) {
    struct MyMockGdi : public Gdi {
        bool setupHwQueueProcAddresses() override {
            return false;
        }
    };
    wddm->gdi.reset(new MyMockGdi());

    EXPECT_FALSE(wddm->init<FamilyType>());
    EXPECT_EQ(1u, wddm->createHwQueueCalled);
    EXPECT_FALSE(wddm->createHwQueueResult);
}
