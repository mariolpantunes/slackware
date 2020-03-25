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

#include "runtime/command_stream/preemption.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/dispatch_info.h"
#include "unit_tests/fixtures/preemption_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_builtins.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/mocks/mock_kernel.h"

#include "gmock/gmock.h"

using namespace OCLRT;

class ThreadGroupPreemptionTests : public DevicePreemptionTests {
    void SetUp() override {
        dbgRestore.reset(new DebugManagerStateRestore());
        DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::ThreadGroup));
        preemptionMode = PreemptionMode::ThreadGroup;
        DevicePreemptionTests::SetUp();
    }
};

class MidThreadPreemptionTests : public DevicePreemptionTests {
    void SetUp() override {
        dbgRestore.reset(new DebugManagerStateRestore());
        DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread));
        preemptionMode = PreemptionMode::MidThread;
        DevicePreemptionTests::SetUp();
    }
};

TEST_F(ThreadGroupPreemptionTests, disallowByKMD) {
    waTable->waDisablePerCtxtPreemptionGranularityControl = 1;

    EXPECT_FALSE(PreemptionHelper::allowThreadGroupPreemption(kernel.get(), waTable));
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(*device, kernel.get()));
}

TEST_F(ThreadGroupPreemptionTests, disallowByDevice) {
    device->setPreemptionMode(PreemptionMode::MidThread);

    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(kernel.get(), waTable));
    EXPECT_EQ(PreemptionMode::MidThread, PreemptionHelper::taskPreemptionMode(*device, kernel.get()));
}

TEST_F(ThreadGroupPreemptionTests, disallowByReadWriteFencesWA) {
    executionEnvironment->UsesFencesForReadWriteImages = 1u;
    waTable->waDisableLSQCROPERFforOCL = 1;

    EXPECT_FALSE(PreemptionHelper::allowThreadGroupPreemption(kernel.get(), waTable));
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(*device, kernel.get()));
}

TEST_F(ThreadGroupPreemptionTests, disallowBySchedulerKernel) {
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device, true));

    EXPECT_FALSE(PreemptionHelper::allowThreadGroupPreemption(kernel.get(), waTable));
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(*device, kernel.get()));
}

TEST_F(ThreadGroupPreemptionTests, disallowByVmeKernel) {
    kernelInfo->isVmeWorkload = true;
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device));

    EXPECT_FALSE(PreemptionHelper::allowThreadGroupPreemption(kernel.get(), waTable));
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(*device, kernel.get()));
}

TEST_F(ThreadGroupPreemptionTests, simpleAllow) {
    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(kernel.get(), waTable));
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(*device, kernel.get()));
}

TEST_F(ThreadGroupPreemptionTests, allowDefaultModeForNonKernelRequest) {
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(*device, nullptr));
}

TEST_F(ThreadGroupPreemptionTests, givenKernelWithNoEnvironmentPatchSetWhenLSQCWaIsTurnedOnThenThreadGroupPreemptionIsBeingSelected) {
    kernelInfo.get()->patchInfo.executionEnvironment = nullptr;
    waTable->waDisableLSQCROPERFforOCL = 1;
    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(kernel.get(), waTable));
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(*device, kernel.get()));
}

TEST_F(ThreadGroupPreemptionTests, givenKernelWithEnvironmentPatchSetWhenLSQCWaIsTurnedOnThenThreadGroupPreemptionIsBeingSelected) {
    executionEnvironment.get()->UsesFencesForReadWriteImages = 0;
    waTable->waDisableLSQCROPERFforOCL = 1;
    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(kernel.get(), waTable));
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(*device, kernel.get()));
}

TEST_F(ThreadGroupPreemptionTests, givenKernelWithEnvironmentPatchSetWhenLSQCWaIsTurnedOffThenThreadGroupPreemptionIsBeingSelected) {
    executionEnvironment.get()->UsesFencesForReadWriteImages = 1;
    waTable->waDisableLSQCROPERFforOCL = 0;
    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(kernel.get(), waTable));
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(*device, kernel.get()));
}

TEST_F(ThreadGroupPreemptionTests, allowMidBatch) {
    device->setPreemptionMode(PreemptionMode::MidBatch);
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(*device, nullptr));
}

TEST_F(ThreadGroupPreemptionTests, disallowWhenAdjustedDisabled) {
    device->setPreemptionMode(PreemptionMode::Disabled);
    EXPECT_EQ(PreemptionMode::Disabled, PreemptionHelper::taskPreemptionMode(*device, nullptr));
}

TEST_F(ThreadGroupPreemptionTests, returnDefaultDeviceModeForZeroSizedMdi) {
    MultiDispatchInfo multiDispatchInfo;
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(*device, multiDispatchInfo));
}

TEST_F(ThreadGroupPreemptionTests, returnDefaultDeviceModeForValidKernelsInMdi) {
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(*dispatchInfo);
    multiDispatchInfo.push(*dispatchInfo);
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(*device, multiDispatchInfo));
}

TEST_F(ThreadGroupPreemptionTests, disallowDefaultDeviceModeForValidKernelsInMdiAndDisabledPremption) {
    device->setPreemptionMode(PreemptionMode::Disabled);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(*dispatchInfo);
    multiDispatchInfo.push(*dispatchInfo);
    EXPECT_EQ(PreemptionMode::Disabled, PreemptionHelper::taskPreemptionMode(*device, multiDispatchInfo));
}

TEST_F(ThreadGroupPreemptionTests, disallowDefaultDeviceModeWhenAtLeastOneInvalidKernelInMdi) {
    MockKernel schedulerKernel(program.get(), *kernelInfo, *device, true);
    DispatchInfo schedulerDispatchInfo(&schedulerKernel, 1, Vec3<size_t>(1, 1, 1), Vec3<size_t>(1, 1, 1), Vec3<size_t>(0, 0, 0));

    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(*device, &schedulerKernel));

    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(*dispatchInfo);
    multiDispatchInfo.push(schedulerDispatchInfo);
    multiDispatchInfo.push(*dispatchInfo);

    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(*device, multiDispatchInfo));
}

TEST_F(MidThreadPreemptionTests, allowMidThreadPreemption) {
    device->setPreemptionMode(PreemptionMode::MidThread);
    executionEnvironment->DisableMidThreadPreemption = 0;
    EXPECT_TRUE(PreemptionHelper::allowMidThreadPreemption(kernel.get(), *device));
}

TEST_F(MidThreadPreemptionTests, allowMidThreadPreemptionNullKernel) {
    device->setPreemptionMode(PreemptionMode::MidThread);
    EXPECT_TRUE(PreemptionHelper::allowMidThreadPreemption(nullptr, *device));
}

TEST_F(MidThreadPreemptionTests, allowMidThreadPreemptionDeviceSupportPreemptionOnVmeKernel) {
    device->setPreemptionMode(PreemptionMode::MidThread);
    device->getMutableDeviceInfo()->vmeAvcSupportsPreemption = true;
    kernelInfo->isVmeWorkload = true;
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device));
    EXPECT_TRUE(PreemptionHelper::allowMidThreadPreemption(kernel.get(), *device));
}

TEST_F(MidThreadPreemptionTests, disallowMidThreadPreemptionByDevice) {
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    executionEnvironment->DisableMidThreadPreemption = 0;
    EXPECT_FALSE(PreemptionHelper::allowMidThreadPreemption(kernel.get(), *device));
}

TEST_F(MidThreadPreemptionTests, disallowMidThreadPreemptionByKernel) {
    device->setPreemptionMode(PreemptionMode::MidThread);
    executionEnvironment->DisableMidThreadPreemption = 1;
    EXPECT_FALSE(PreemptionHelper::allowMidThreadPreemption(kernel.get(), *device));
}

TEST_F(MidThreadPreemptionTests, disallowMidThreadPreemptionByVmeKernel) {
    device->setPreemptionMode(PreemptionMode::MidThread);
    device->getMutableDeviceInfo()->vmeAvcSupportsPreemption = false;
    kernelInfo->isVmeWorkload = true;
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device));
    EXPECT_FALSE(PreemptionHelper::allowMidThreadPreemption(kernel.get(), *device));
}

TEST_F(MidThreadPreemptionTests, taskPreemptionDisallowMidThreadByDevice) {
    executionEnvironment->DisableMidThreadPreemption = 0;
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(*device, kernel.get());
    EXPECT_EQ(PreemptionMode::ThreadGroup, outMode);
}

TEST_F(MidThreadPreemptionTests, taskPreemptionDisallowMidThreadByKernel) {
    executionEnvironment->DisableMidThreadPreemption = 1;
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(*device, kernel.get());
    EXPECT_EQ(PreemptionMode::ThreadGroup, outMode);
}

TEST_F(MidThreadPreemptionTests, taskPreemptionDisallowMidThreadByVmeKernel) {
    kernelInfo->isVmeWorkload = true;
    device->getMutableDeviceInfo()->vmeAvcSupportsPreemption = false;
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device));
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(*device, kernel.get());
    //VME disables mid thread and thread group when device does not support it
    EXPECT_EQ(PreemptionMode::MidBatch, outMode);
}

TEST_F(MidThreadPreemptionTests, taskPreemptionAllow) {
    executionEnvironment->DisableMidThreadPreemption = 0;
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(*device, kernel.get());
    EXPECT_EQ(PreemptionMode::MidThread, outMode);
}

TEST_F(MidThreadPreemptionTests, taskPreemptionAllowDeviceSupportsPreemptionOnVmeKernel) {
    executionEnvironment->DisableMidThreadPreemption = 0;
    kernelInfo->isVmeWorkload = true;
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device));
    device->getMutableDeviceInfo()->vmeAvcSupportsPreemption = true;
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(*device, kernel.get());
    EXPECT_EQ(PreemptionMode::MidThread, outMode);
}

TEST_F(DevicePreemptionTests, setDefaultMidThreadPreemption) {
    RuntimeCapabilityTable devCapabilities = {};

    devCapabilities.defaultPreemptionMode = PreemptionMode::MidThread;

    PreemptionHelper::adjustDefaultPreemptionMode(devCapabilities, true, true, true);
    EXPECT_EQ(PreemptionMode::MidThread, devCapabilities.defaultPreemptionMode);
}

TEST_F(DevicePreemptionTests, setDefaultThreadGroupPreemptionNoMidThreadDefault) {
    RuntimeCapabilityTable devCapabilities = {};

    devCapabilities.defaultPreemptionMode = PreemptionMode::ThreadGroup;

    PreemptionHelper::adjustDefaultPreemptionMode(devCapabilities, true, true, true);
    EXPECT_EQ(PreemptionMode::ThreadGroup, devCapabilities.defaultPreemptionMode);
}

TEST_F(DevicePreemptionTests, setDefaultThreadGroupPreemptionNoMidThreadSupport) {
    RuntimeCapabilityTable devCapabilities = {};

    devCapabilities.defaultPreemptionMode = PreemptionMode::MidThread;

    PreemptionHelper::adjustDefaultPreemptionMode(devCapabilities, false, true, true);
    EXPECT_EQ(PreemptionMode::ThreadGroup, devCapabilities.defaultPreemptionMode);
}

TEST_F(DevicePreemptionTests, setDefaultMidBatchPreemptionNoThreadGroupDefault) {
    RuntimeCapabilityTable devCapabilities = {};

    devCapabilities.defaultPreemptionMode = PreemptionMode::MidBatch;

    PreemptionHelper::adjustDefaultPreemptionMode(devCapabilities, true, true, true);
    EXPECT_EQ(PreemptionMode::MidBatch, devCapabilities.defaultPreemptionMode);
}

TEST_F(DevicePreemptionTests, setDefaultMidBatchPreemptionNoThreadGroupSupport) {
    RuntimeCapabilityTable devCapabilities = {};

    devCapabilities.defaultPreemptionMode = PreemptionMode::MidThread;

    PreemptionHelper::adjustDefaultPreemptionMode(devCapabilities, false, false, true);
    EXPECT_EQ(PreemptionMode::MidBatch, devCapabilities.defaultPreemptionMode);
}

TEST_F(DevicePreemptionTests, setDefaultDisabledPreemptionNoMidBatchDefault) {
    RuntimeCapabilityTable devCapabilities = {};

    devCapabilities.defaultPreemptionMode = PreemptionMode::Disabled;

    PreemptionHelper::adjustDefaultPreemptionMode(devCapabilities, true, true, true);
    EXPECT_EQ(PreemptionMode::Disabled, devCapabilities.defaultPreemptionMode);
}

TEST_F(DevicePreemptionTests, setDefaultDisabledPreemptionNoMidBatchSupport) {
    RuntimeCapabilityTable devCapabilities = {};

    devCapabilities.defaultPreemptionMode = PreemptionMode::MidThread;

    PreemptionHelper::adjustDefaultPreemptionMode(devCapabilities, false, false, false);
    EXPECT_EQ(PreemptionMode::Disabled, devCapabilities.defaultPreemptionMode);
}

struct PreemptionHwTest : ::testing::Test, ::testing::WithParamInterface<PreemptionMode> {
};

HWTEST_P(PreemptionHwTest, getRequiredCmdStreamSizeReturns0WhenPreemptionModeIsNotChanging) {
    PreemptionMode mode = GetParam();
    size_t requiredSize = PreemptionHelper::getRequiredCmdStreamSize<FamilyType>(mode, mode);
    EXPECT_EQ(0U, requiredSize);

    StackVec<char, 4096> buffer(requiredSize);
    LinearStream cmdStream(buffer.begin(), buffer.size());

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::create<MockDevice>(nullptr));
    {
        MockBuiltins tmpBuiltins;
        tmpBuiltins.overrideSipKernel(std::unique_ptr<OCLRT::SipKernel>(new OCLRT::SipKernel{SipKernelType::Csr, getSipProgramWithCustomBinary()}));
        tmpBuiltins.overrideGlobalBuiltins();
        PreemptionHelper::programCmdStream<FamilyType>(cmdStream, mode, mode,
                                                       nullptr, *mockDevice);
    }
    EXPECT_EQ(0U, cmdStream.getUsed());
}

HWTEST_P(PreemptionHwTest, getRequiredCmdStreamSizeReturnsSizeOfMiLoadRegisterImmWhenPreemptionModeIsChanging) {
    PreemptionMode mode = GetParam();
    PreemptionMode differentPreemptionMode = static_cast<PreemptionMode>(0);

    if (false == GetPreemptionTestHwDetails<FamilyType>().supportsPreemptionProgramming()) {
        EXPECT_EQ(0U, PreemptionHelper::getRequiredCmdStreamSize<FamilyType>(mode, differentPreemptionMode));
        return;
    }

    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    size_t requiredSize = PreemptionHelper::getRequiredCmdStreamSize<FamilyType>(mode, differentPreemptionMode);
    EXPECT_EQ(sizeof(MI_LOAD_REGISTER_IMM), requiredSize);

    StackVec<char, 4096> buffer(requiredSize);
    LinearStream cmdStream(buffer.begin(), buffer.size());

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::create<MockDevice>(nullptr));

    size_t minCsrSize = mockDevice->getHardwareInfo().pSysInfo->CsrSizeInMb * MemoryConstants::megaByte;
    uint64_t minCsrAlignment = 2 * 256 * MemoryConstants::kiloByte;
    MockGraphicsAllocation csrSurface((void *)minCsrAlignment, minCsrSize);

    PreemptionHelper::programCmdStream<FamilyType>(cmdStream, mode, differentPreemptionMode,
                                                   nullptr, *mockDevice);
    EXPECT_EQ(requiredSize, cmdStream.getUsed());
}

HWTEST_P(PreemptionHwTest, programCmdStreamAddsProperMiLoadRegisterImmCommandToTheStream) {
    PreemptionMode mode = GetParam();
    PreemptionMode differentPreemptionMode = static_cast<PreemptionMode>(0);
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::create<MockDevice>(nullptr));

    if (false == GetPreemptionTestHwDetails<FamilyType>().supportsPreemptionProgramming()) {
        LinearStream cmdStream(nullptr, 0U);
        PreemptionHelper::programCmdStream<FamilyType>(cmdStream, mode, differentPreemptionMode, nullptr, *mockDevice);
        EXPECT_EQ(0U, cmdStream.getUsed());
        return;
    }

    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    auto hwDetails = GetPreemptionTestHwDetails<FamilyType>();

    uint32_t defaultRegValue = hwDetails.defaultRegValue;

    uint32_t expectedRegValue = defaultRegValue;
    if (hwDetails.modeToRegValueMap.find(mode) != hwDetails.modeToRegValueMap.end()) {
        expectedRegValue = hwDetails.modeToRegValueMap[mode];
    }

    size_t requiredSize = PreemptionHelper::getRequiredCmdStreamSize<FamilyType>(mode, differentPreemptionMode);
    StackVec<char, 4096> buffer(requiredSize);
    LinearStream cmdStream(buffer.begin(), buffer.size());

    size_t minCsrSize = mockDevice->getHardwareInfo().pSysInfo->CsrSizeInMb * MemoryConstants::megaByte;
    uint64_t minCsrAlignment = 2 * 256 * MemoryConstants::kiloByte;
    MockGraphicsAllocation csrSurface((void *)minCsrAlignment, minCsrSize);

    PreemptionHelper::programCmdStream<FamilyType>(cmdStream, mode, differentPreemptionMode,
                                                   &csrSurface, *mockDevice);

    HardwareParse cmdParser;
    cmdParser.parseCommands<FamilyType>(cmdStream);
    const uint32_t regAddress = hwDetails.regAddress;
    MI_LOAD_REGISTER_IMM *cmd = findMmioCmd<FamilyType>(cmdParser.cmdList.begin(), cmdParser.cmdList.end(), regAddress);
    ASSERT_NE(nullptr, cmd);
    EXPECT_EQ(expectedRegValue, cmd->getDataDword());
}

INSTANTIATE_TEST_CASE_P(
    CreateParametrizedPreemptionHwTest,
    PreemptionHwTest,
    ::testing::Values(PreemptionMode::Disabled, PreemptionMode::MidBatch, PreemptionMode::ThreadGroup, PreemptionMode::MidThread));

HWTEST_F(MidThreadPreemptionTests, createCsrSurfaceNoWa) {
    const WorkaroundTable *waTable = platformDevices[0]->pWaTable;
    WorkaroundTable tmpWaTable;
    tmpWaTable.waCSRUncachable = false;
    const_cast<HardwareInfo *>(platformDevices[0])->pWaTable = &tmpWaTable;

    std::unique_ptr<MockDevice> mockDevice(Device::create<OCLRT::MockDevice>(platformDevices[0]));
    ASSERT_NE(nullptr, mockDevice.get());

    auto &csr = mockDevice->getUltCommandStreamReceiver<FamilyType>();
    MemoryAllocation *csrSurface = static_cast<MemoryAllocation *>(csr.getPreemptionCsrAllocation());
    ASSERT_NE(nullptr, csrSurface);
    EXPECT_FALSE(csrSurface->uncacheable);

    GraphicsAllocation *devCsrSurface = mockDevice->getPreemptionAllocation();
    EXPECT_EQ(csrSurface, devCsrSurface);

    const_cast<HardwareInfo *>(platformDevices[0])->pWaTable = waTable;
}

HWTEST_F(MidThreadPreemptionTests, createCsrSurfaceWa) {
    const WorkaroundTable *waTable = platformDevices[0]->pWaTable;
    WorkaroundTable tmpWaTable;
    tmpWaTable.waCSRUncachable = true;
    const_cast<HardwareInfo *>(platformDevices[0])->pWaTable = &tmpWaTable;

    std::unique_ptr<MockDevice> mockDevice(Device::create<OCLRT::MockDevice>(platformDevices[0]));
    ASSERT_NE(nullptr, mockDevice.get());

    auto &csr = mockDevice->getUltCommandStreamReceiver<FamilyType>();
    MemoryAllocation *csrSurface = static_cast<MemoryAllocation *>(csr.getPreemptionCsrAllocation());
    ASSERT_NE(nullptr, csrSurface);
    EXPECT_TRUE(csrSurface->uncacheable);

    GraphicsAllocation *devCsrSurface = mockDevice->getPreemptionAllocation();
    EXPECT_EQ(csrSurface, devCsrSurface);

    const_cast<HardwareInfo *>(platformDevices[0])->pWaTable = waTable;
}
