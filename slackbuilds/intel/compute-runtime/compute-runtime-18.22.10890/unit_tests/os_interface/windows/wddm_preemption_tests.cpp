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
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/os_interface/windows/wddm_fixture.h"
#include "test.h"

using namespace OCLRT;

class WddmPreemptionTests : public Test<WddmFixtureWithMockGdiDll> {
  public:
    void SetUp() override {
        WddmFixtureWithMockGdiDll::SetUp();
        const HardwareInfo hwInfo = *platformDevices[0];
        memcpy(&hwInfoTest, &hwInfo, sizeof(hwInfoTest));
        dbgRestorer = new DebugManagerStateRestore();
    }

    void TearDown() override {
        delete dbgRestorer;
        WddmFixtureWithMockGdiDll::TearDown();
    }

    template <typename GfxFamily>
    void createAndInitWddm(unsigned int forceReturnPreemptionRegKeyValue) {
        wddm.reset(static_cast<WddmMock *>(Wddm::createWddm(WddmInterfaceVersion::Wddm20)));
        auto regReader = new RegistryReaderMock();
        wddm->registryReader.reset(regReader);
        regReader->forceRetValue = forceReturnPreemptionRegKeyValue;
        PreemptionMode preemptionMode = PreemptionHelper::getDefaultPreemptionMode(hwInfoTest);
        wddm->setPreemptionMode(preemptionMode);
        wddm->init<GfxFamily>();
    }

    DebugManagerStateRestore *dbgRestorer = nullptr;
    HardwareInfo hwInfoTest;
};

HWTEST_F(WddmPreemptionTests, givenDevicePreemptionEnabledDebugFlagDontForceWhenPreemptionRegKeySetThenSetGpuTimeoutFlagOn) {
    DebugManager.flags.ForcePreemptionMode.set(-1); // dont force
    hwInfoTest.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    unsigned int expectedVal = 1u;
    createAndInitWddm<FamilyType>(1u);
    EXPECT_EQ(expectedVal, getMockCreateDeviceParamsFcn().Flags.DisableGpuTimeout);
    EXPECT_EQ(expectedVal, getCreateContextDataFcn()->Flags.DisableGpuTimeout);
}

HWTEST_F(WddmPreemptionTests, givenDevicePreemptionDisabledDebugFlagDontForceWhenPreemptionRegKeySetThenSetGpuTimeoutFlagOff) {
    DebugManager.flags.ForcePreemptionMode.set(-1); // dont force
    hwInfoTest.capabilityTable.defaultPreemptionMode = PreemptionMode::Disabled;
    unsigned int expectedVal = 0u;
    createAndInitWddm<FamilyType>(1u);
    EXPECT_EQ(expectedVal, getMockCreateDeviceParamsFcn().Flags.DisableGpuTimeout);
    EXPECT_EQ(expectedVal, getCreateContextDataFcn()->Flags.DisableGpuTimeout);
}

HWTEST_F(WddmPreemptionTests, givenDevicePreemptionEnabledDebugFlagDontForceWhenPreemptionRegKeyNotSetThenSetGpuTimeoutFlagOff) {
    DebugManager.flags.ForcePreemptionMode.set(-1); // dont force
    hwInfoTest.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    unsigned int expectedVal = 0u;
    createAndInitWddm<FamilyType>(0u);
    EXPECT_EQ(expectedVal, getMockCreateDeviceParamsFcn().Flags.DisableGpuTimeout);
    EXPECT_EQ(expectedVal, getCreateContextDataFcn()->Flags.DisableGpuTimeout);
}

HWTEST_F(WddmPreemptionTests, givenDevicePreemptionDisabledDebugFlagDontForceWhenPreemptionRegKeyNotSetThenSetGpuTimeoutFlagOff) {
    DebugManager.flags.ForcePreemptionMode.set(-1); // dont force
    hwInfoTest.capabilityTable.defaultPreemptionMode = PreemptionMode::Disabled;
    unsigned int expectedVal = 0u;
    createAndInitWddm<FamilyType>(0u);
    EXPECT_EQ(expectedVal, getMockCreateDeviceParamsFcn().Flags.DisableGpuTimeout);
    EXPECT_EQ(expectedVal, getCreateContextDataFcn()->Flags.DisableGpuTimeout);
}

HWTEST_F(WddmPreemptionTests, givenDevicePreemptionDisabledDebugFlagForcePreemptionWhenPreemptionRegKeySetThenSetGpuTimeoutFlagOn) {
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread)); // force preemption
    hwInfoTest.capabilityTable.defaultPreemptionMode = PreemptionMode::Disabled;
    unsigned int expectedVal = 1u;
    createAndInitWddm<FamilyType>(1u);
    EXPECT_EQ(expectedVal, getMockCreateDeviceParamsFcn().Flags.DisableGpuTimeout);
    EXPECT_EQ(expectedVal, getCreateContextDataFcn()->Flags.DisableGpuTimeout);
}

HWTEST_F(WddmPreemptionTests, givenDevicePreemptionDisabledDebugFlagForcePreemptionWhenPreemptionRegKeyNotSetThenSetGpuTimeoutFlagOff) {
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread)); // force preemption
    hwInfoTest.capabilityTable.defaultPreemptionMode = PreemptionMode::Disabled;
    unsigned int expectedVal = 0u;
    createAndInitWddm<FamilyType>(0u);
    EXPECT_EQ(expectedVal, getMockCreateDeviceParamsFcn().Flags.DisableGpuTimeout);
    EXPECT_EQ(expectedVal, getCreateContextDataFcn()->Flags.DisableGpuTimeout);
}
