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

#include "runtime/command_queue/command_queue.h"

#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_context.h"
#include "test.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

using namespace OCLRT;

struct KmdNotifyTests : public ::testing::Test {
    void SetUp() override {
        device.reset(Device::create<MockDevice>(&localHwInfo));
        cmdQ.reset(new CommandQueue(&context, device.get(), nullptr));
        *device->getTagAddress() = taskCountToWait;
        device->getCommandStreamReceiver().waitForFlushStamp(flushStampToWait);
        overrideKmdNotifyParams(true, 2, true, 1, false, 0);
    }

    void overrideKmdNotifyParams(bool kmdNotifyEnable, int64_t kmdNotifyDelay,
                                 bool quickKmdSleepEnable, int64_t quickKmdSleepDelay,
                                 bool quickKmdSleepEnableForSporadicWaits, int64_t quickKmdSleepDelayForSporadicWaits) {
        auto &properties = localHwInfo.capabilityTable.kmdNotifyProperties;
        properties.enableKmdNotify = kmdNotifyEnable;
        properties.delayKmdNotifyMicroseconds = kmdNotifyDelay;
        properties.enableQuickKmdSleep = quickKmdSleepEnable;
        properties.delayQuickKmdSleepMicroseconds = quickKmdSleepDelay;
        properties.enableQuickKmdSleepForSporadicWaits = quickKmdSleepEnableForSporadicWaits;
        properties.delayQuickKmdSleepForSporadicWaitsMicroseconds = quickKmdSleepDelayForSporadicWaits;
    }

    class MockKmdNotifyHelper : public KmdNotifyHelper {
      public:
        using KmdNotifyHelper::acLineConnected;
        using KmdNotifyHelper::lastWaitForCompletionTimestamp;

        MockKmdNotifyHelper() = delete;
        MockKmdNotifyHelper(const KmdNotifyProperties *newProperties) : KmdNotifyHelper(newProperties){};

        void updateLastWaitForCompletionTimestamp() override {
            KmdNotifyHelper::updateLastWaitForCompletionTimestamp();
            updateLastWaitForCompletionTimestampCalled++;
        }

        void updateAcLineStatus() override {
            KmdNotifyHelper::updateAcLineStatus();
            updateAcLineStatusCalled++;
        }

        uint32_t updateLastWaitForCompletionTimestampCalled = 0u;
        uint32_t updateAcLineStatusCalled = 0u;
    };

    template <typename Family>
    class MockKmdNotifyCsr : public UltCommandStreamReceiver<Family> {
      public:
        MockKmdNotifyCsr(const HardwareInfo &hwInfo) : UltCommandStreamReceiver<Family>(hwInfo) {}
        MOCK_METHOD1(waitForFlushStamp, bool(FlushStamp &flushStampToWait));
        MOCK_METHOD3(waitForCompletionWithTimeout, bool(bool enableTimeout, int64_t timeoutMs, uint32_t taskCountToWait));
    };

    template <typename Family>
    MockKmdNotifyCsr<Family> *createMockCsr() {
        auto csr = new ::testing::NiceMock<MockKmdNotifyCsr<Family>>(device->getHardwareInfo());
        device->resetCommandStreamReceiver(csr);

        mockKmdNotifyHelper = new MockKmdNotifyHelper(&device->getHardwareInfo().capabilityTable.kmdNotifyProperties);
        csr->resetKmdNotifyHelper(mockKmdNotifyHelper);

        return csr;
    }

    MockKmdNotifyHelper *mockKmdNotifyHelper = nullptr;
    HardwareInfo localHwInfo = **platformDevices;
    MockContext context;
    std::unique_ptr<MockDevice> device;
    std::unique_ptr<CommandQueue> cmdQ;
    FlushStamp flushStampToWait = 1000;
    uint32_t taskCountToWait = 5;
};

HWTEST_F(KmdNotifyTests, givenTaskCountWhenWaitUntilCompletionCalledThenAlwaysTryCpuPolling) {
    auto csr = createMockCsr<FamilyType>();

    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, 2, taskCountToWait)).Times(1).WillOnce(::testing::Return(true));

    cmdQ->waitUntilComplete(taskCountToWait, flushStampToWait, false);
}

HWTEST_F(KmdNotifyTests, givenTaskCountAndKmdNotifyDisabledWhenWaitUntilCompletionCalledThenTryCpuPollingWithoutTimeout) {
    overrideKmdNotifyParams(false, 0, false, 0, false, 0);
    auto csr = createMockCsr<FamilyType>();

    EXPECT_CALL(*csr, waitForCompletionWithTimeout(false, 0, taskCountToWait)).Times(1).WillOnce(::testing::Return(true));
    EXPECT_CALL(*csr, waitForFlushStamp(::testing::_)).Times(0);

    cmdQ->waitUntilComplete(taskCountToWait, flushStampToWait, false);
}

HWTEST_F(KmdNotifyTests, givenNotReadyTaskCountWhenWaitUntilCompletionCalledThenTryCpuPollingAndKmdWait) {
    auto csr = createMockCsr<FamilyType>();
    *device->getTagAddress() = taskCountToWait - 1;

    ::testing::InSequence is;
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, 2, taskCountToWait)).Times(1).WillOnce(::testing::Return(false));
    EXPECT_CALL(*csr, waitForFlushStamp(flushStampToWait)).Times(1).WillOnce(::testing::Return(true));
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(false, 0, taskCountToWait)).Times(1).WillOnce(::testing::Return(false));

    //we have unrecoverable for this case, this will throw.
    EXPECT_THROW(cmdQ->waitUntilComplete(taskCountToWait, flushStampToWait, false), std::exception);
}

HWTEST_F(KmdNotifyTests, givenReadyTaskCountWhenWaitUntilCompletionCalledThenTryCpuPollingAndDontCallKmdWait) {
    auto csr = createMockCsr<FamilyType>();

    ::testing::InSequence is;
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, 2, taskCountToWait)).Times(1).WillOnce(::testing::Return(true));
    EXPECT_CALL(*csr, waitForFlushStamp(::testing::_)).Times(0);

    cmdQ->waitUntilComplete(taskCountToWait, flushStampToWait, false);
}

HWTEST_F(KmdNotifyTests, givenDefaultArgumentWhenWaitUntilCompleteIsCalledThenDisableQuickKmdSleep) {
    auto csr = createMockCsr<FamilyType>();
    auto expectedTimeout = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds;

    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, expectedTimeout, taskCountToWait)).Times(1).WillOnce(::testing::Return(true));

    cmdQ->waitUntilComplete(taskCountToWait, flushStampToWait, false);
}

HWTEST_F(KmdNotifyTests, givenEnabledQuickSleepWhenWaitUntilCompleteIsCalledThenChangeDelayValue) {
    auto csr = createMockCsr<FamilyType>();
    auto expectedTimeout = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds;

    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, expectedTimeout, taskCountToWait)).Times(1).WillOnce(::testing::Return(true));

    cmdQ->waitUntilComplete(taskCountToWait, flushStampToWait, true);
}

HWTEST_F(KmdNotifyTests, givenDisabledQuickSleepWhenWaitUntilCompleteWithQuickSleepRequestIsCalledThenUseBaseDelayValue) {
    overrideKmdNotifyParams(true, 1, false, 0, false, 0);
    auto csr = createMockCsr<FamilyType>();
    auto expectedTimeout = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds;

    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, expectedTimeout, taskCountToWait)).Times(1).WillOnce(::testing::Return(true));

    cmdQ->waitUntilComplete(taskCountToWait, flushStampToWait, true);
}

HWTEST_F(KmdNotifyTests, givenNotReadyTaskCountWhenPollForCompletionCalledThenTimeout) {
    *device->getTagAddress() = taskCountToWait - 1;
    auto success = device->getCommandStreamReceiver().waitForCompletionWithTimeout(true, 1, taskCountToWait);
    EXPECT_FALSE(success);
}

HWTEST_F(KmdNotifyTests, givenZeroFlushStampWhenWaitIsCalledThenDisableTimeout) {
    auto csr = createMockCsr<FamilyType>();

    EXPECT_TRUE(device->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableKmdNotify);
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(false, ::testing::_, taskCountToWait)).Times(1).WillOnce(::testing::Return(true));
    EXPECT_CALL(*csr, waitForFlushStamp(::testing::_)).Times(0);

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 0, false);
}

HWTEST_F(KmdNotifyTests, givenNonQuickSleepRequestWhenItsSporadicWaitThenOverrideQuickSleepRequest) {
    overrideKmdNotifyParams(true, 3, true, 2, true, 0);
    auto csr = createMockCsr<FamilyType>();

    auto expectedDelay = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds;
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(::testing::_, expectedDelay, ::testing::_)).Times(1).WillOnce(::testing::Return(true));

    auto now = std::chrono::high_resolution_clock::now();
    mockKmdNotifyHelper->lastWaitForCompletionTimestamp = now - std::chrono::hours(24);
    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, false);
}

HWTEST_F(KmdNotifyTests, givenNonQuickSleepRequestWhenItsNotSporadicWaitThenOverrideQuickSleepRequest) {
    overrideKmdNotifyParams(true, 3, true, 2, true, 9999999);
    auto csr = createMockCsr<FamilyType>();

    auto expectedDelay = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds;
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(::testing::_, expectedDelay, ::testing::_)).Times(1).WillOnce(::testing::Return(true));

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, false);
}

HWTEST_F(KmdNotifyTests, givenQuickSleepRequestWhenItsSporadicWaitOptimizationIsDisabledThenDontOverrideQuickSleepRequest) {
    overrideKmdNotifyParams(true, 3, true, 2, false, 0);
    auto csr = createMockCsr<FamilyType>();

    auto expectedDelay = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds;
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(::testing::_, expectedDelay, ::testing::_)).Times(1).WillOnce(::testing::Return(true));

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, true);
}

HWTEST_F(KmdNotifyTests, givenTaskCountEqualToHwTagWhenWaitCalledThenDontMultiplyTimeout) {
    auto csr = createMockCsr<FamilyType>();
    *device->getTagAddress() = taskCountToWait;

    auto expectedTimeout = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds;

    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, expectedTimeout, ::testing::_)).Times(1).WillOnce(::testing::Return(true));

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, false);
}

HWTEST_F(KmdNotifyTests, givenTaskCountLowerThanHwTagWhenWaitCalledThenDontMultiplyTimeout) {
    auto csr = createMockCsr<FamilyType>();
    *device->getTagAddress() = taskCountToWait + 5;

    auto expectedTimeout = device->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds;

    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, expectedTimeout, ::testing::_)).Times(1).WillOnce(::testing::Return(true));

    csr->waitForTaskCountWithKmdNotifyFallback(taskCountToWait, 1, false);
}

HWTEST_F(KmdNotifyTests, givenDefaultCommandStreamReceiverWhenWaitCalledThenUpdateWaitTimestamp) {
    overrideKmdNotifyParams(true, 3, true, 2, true, 1);

    auto csr = createMockCsr<FamilyType>();
    EXPECT_NE(0, mockKmdNotifyHelper->lastWaitForCompletionTimestamp.time_since_epoch().count());

    EXPECT_EQ(1u, mockKmdNotifyHelper->updateLastWaitForCompletionTimestampCalled);
    csr->waitForTaskCountWithKmdNotifyFallback(0, 0, false);
    EXPECT_EQ(2u, mockKmdNotifyHelper->updateLastWaitForCompletionTimestampCalled);
}

HWTEST_F(KmdNotifyTests, givenDefaultCommandStreamReceiverWithDisabledSporadicWaitOptimizationWhenWaitCalledThenDontUpdateWaitTimestamp) {
    overrideKmdNotifyParams(true, 3, true, 2, false, 0);

    auto csr = createMockCsr<FamilyType>();
    EXPECT_EQ(0, mockKmdNotifyHelper->lastWaitForCompletionTimestamp.time_since_epoch().count());

    csr->waitForTaskCountWithKmdNotifyFallback(0, 0, false);
    EXPECT_EQ(0u, mockKmdNotifyHelper->updateLastWaitForCompletionTimestampCalled);
}

HWTEST_F(KmdNotifyTests, givenNewHelperWhenItsSetToCsrThenUpdateAcLineStatus) {
    auto helper = new MockKmdNotifyHelper(&(localHwInfo.capabilityTable.kmdNotifyProperties));
    EXPECT_EQ(0u, helper->updateAcLineStatusCalled);

    auto csr = createMockCsr<FamilyType>();
    csr->resetKmdNotifyHelper(helper);
    EXPECT_EQ(1u, helper->updateAcLineStatusCalled);
}

TEST_F(KmdNotifyTests, givenTaskCountDiffLowerThanMinimumToCheckAcLineWhenObtainingTimeoutPropertiesThenDontCheck) {
    localHwInfo.capabilityTable.kmdNotifyProperties.enableKmdNotify = false;
    MockKmdNotifyHelper helper(&(localHwInfo.capabilityTable.kmdNotifyProperties));

    uint32_t hwTag = 9;
    uint32_t taskCountToWait = 10;
    EXPECT_TRUE(taskCountToWait - hwTag < KmdNotifyConstants::minimumTaskCountDiffToCheckAcLine);
    EXPECT_EQ(10u, KmdNotifyConstants::minimumTaskCountDiffToCheckAcLine);

    int64_t timeout = 0;
    helper.obtainTimeoutParams(timeout, false, hwTag, taskCountToWait, 1);

    EXPECT_EQ(0u, helper.updateAcLineStatusCalled);
}

TEST_F(KmdNotifyTests, givenTaskCountDiffGreaterThanMinimumToCheckAcLineAndDisabledKmdNotifyWhenObtainingTimeoutPropertiesThenCheck) {
    localHwInfo.capabilityTable.kmdNotifyProperties.enableKmdNotify = false;
    MockKmdNotifyHelper helper(&(localHwInfo.capabilityTable.kmdNotifyProperties));

    uint32_t hwTag = 10;
    uint32_t taskCountToWait = 21;
    EXPECT_TRUE(taskCountToWait - hwTag > KmdNotifyConstants::minimumTaskCountDiffToCheckAcLine);
    EXPECT_EQ(10u, KmdNotifyConstants::minimumTaskCountDiffToCheckAcLine);

    int64_t timeout = 0;
    helper.obtainTimeoutParams(timeout, false, hwTag, taskCountToWait, 1);

    EXPECT_EQ(1u, helper.updateAcLineStatusCalled);
}

TEST_F(KmdNotifyTests, givenTaskCountDiffGreaterThanMinimumToCheckAcLineAndEnabledKmdNotifyWhenObtainingTimeoutPropertiesThenDontCheck) {
    localHwInfo.capabilityTable.kmdNotifyProperties.enableKmdNotify = true;
    MockKmdNotifyHelper helper(&(localHwInfo.capabilityTable.kmdNotifyProperties));

    uint32_t hwTag = 10;
    uint32_t taskCountToWait = 21;
    EXPECT_TRUE(taskCountToWait - hwTag > KmdNotifyConstants::minimumTaskCountDiffToCheckAcLine);
    EXPECT_EQ(10u, KmdNotifyConstants::minimumTaskCountDiffToCheckAcLine);

    int64_t timeout = 0;
    helper.obtainTimeoutParams(timeout, false, hwTag, taskCountToWait, 1);

    EXPECT_EQ(0u, helper.updateAcLineStatusCalled);
}

TEST_F(KmdNotifyTests, givenDisabledKmdNotifyMechanismWhenAcLineIsDisconnectedThenForceEnableTimeout) {
    localHwInfo.capabilityTable.kmdNotifyProperties.enableKmdNotify = false;
    MockKmdNotifyHelper helper(&(localHwInfo.capabilityTable.kmdNotifyProperties));
    helper.acLineConnected = false;

    int64_t timeout = 0;
    bool timeoutEnabled = helper.obtainTimeoutParams(timeout, false, 1, 2, 2);

    EXPECT_TRUE(timeoutEnabled);
    EXPECT_EQ(KmdNotifyConstants::timeoutInMicrosecondsForDisconnectedAcLine, timeout);
    EXPECT_EQ(10000, KmdNotifyConstants::timeoutInMicrosecondsForDisconnectedAcLine);
}

TEST_F(KmdNotifyTests, givenEnabledKmdNotifyMechanismWhenAcLineIsDisconnectedThenDontChangeTimeoutValue) {
    localHwInfo.capabilityTable.kmdNotifyProperties.enableKmdNotify = true;
    localHwInfo.capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds = 5;
    MockKmdNotifyHelper helper(&(localHwInfo.capabilityTable.kmdNotifyProperties));
    helper.acLineConnected = false;

    int64_t timeout = 0;
    bool timeoutEnabled = helper.obtainTimeoutParams(timeout, false, 1, 2, 2);

    EXPECT_TRUE(timeoutEnabled);
    EXPECT_EQ(localHwInfo.capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds, timeout);
}

TEST_F(KmdNotifyTests, givenDisabledKmdNotifyMechanismAndFlushStampIsZeroWhenAcLineIsDisconnectedThenDontForceEnableTimeout) {
    localHwInfo.capabilityTable.kmdNotifyProperties.enableKmdNotify = false;
    MockKmdNotifyHelper helper(&(localHwInfo.capabilityTable.kmdNotifyProperties));
    helper.acLineConnected = false;

    int64_t timeout = 0;
    FlushStamp flushStampToWait = 0;
    bool timeoutEnabled = helper.obtainTimeoutParams(timeout, false, 1, 2, flushStampToWait);

    EXPECT_FALSE(timeoutEnabled);
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
