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

#include "unit_tests/helpers/gtest_helpers.h"
#include "unit_tests/os_interface/linux/hw_info_config_tests.h"

using namespace OCLRT;
using namespace std;

struct HwInfoConfigTestLinuxCfl : HwInfoConfigTestLinux {
    void SetUp() override {
        HwInfoConfigTestLinux::SetUp();
        drm->StoredDeviceID = ICFL_GT2_DT_DEVICE_F0_ID;
        drm->setGtType(GTTYPE_GT2);
    }
};

CFLTEST_F(HwInfoConfigTestLinuxCfl, configureHwInfo) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->StoredDeviceID, outHwInfo.pPlatform->usDeviceID);
    EXPECT_EQ((unsigned short)drm->StoredDeviceRevID, outHwInfo.pPlatform->usRevId);
    EXPECT_EQ((uint32_t)drm->StoredEUVal, outHwInfo.pSysInfo->EUCount);
    EXPECT_EQ((uint32_t)drm->StoredSSVal, outHwInfo.pSysInfo->SubSliceCount);
    EXPECT_EQ(EngineType::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    EXPECT_EQ(GTTYPE_GT2, outHwInfo.pPlatform->eGTType);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT1);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT1_5);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrGT2);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT3);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT4);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTA);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTC);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTX);

    //constant sysInfo/ftr flags
    EXPECT_EQ(1u, outHwInfo.pSysInfo->VEBoxInfo.Instances.Bits.VEBox0Enabled);
    EXPECT_TRUE(outHwInfo.pSysInfo->VEBoxInfo.IsValid);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrVEBOX);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrGpGpuMidBatchPreempt);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrGpGpuThreadGroupLevelPreempt);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGpGpuMidThreadLevelPreempt);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftr3dMidBatchPreempt);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftr3dObjectLevelPreempt);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrPerCtxtPreemptionGranularityControl);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrPPGTT);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrSVM);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrL3IACoherency);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrIA32eGfxPTEs);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrDisplayYTiling);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrTranslationTable);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrUserModeTranslationTable);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrEnableGuC);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrFbc);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrFbc2AddressTranslation);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrFbcBlitterTracking);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrFbcCpuTracking);

    ReleaseOutHwInfoStructs();

    drm->StoredDeviceID = ICFL_GT1_DT_DEVICE_F0_ID;
    drm->StoredSSVal = 3;
    drm->setGtType(GTTYPE_GT1);
    ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->StoredDeviceID, outHwInfo.pPlatform->usDeviceID);
    EXPECT_EQ((unsigned short)drm->StoredDeviceRevID, outHwInfo.pPlatform->usRevId);
    EXPECT_EQ((uint32_t)drm->StoredEUVal, outHwInfo.pSysInfo->EUCount);
    EXPECT_EQ((uint32_t)drm->StoredSSVal, outHwInfo.pSysInfo->SubSliceCount);
    EXPECT_EQ(1u, outHwInfo.pSysInfo->SliceCount);
    EXPECT_EQ(EngineType::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    EXPECT_EQ(GTTYPE_GT1, outHwInfo.pPlatform->eGTType);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrGT1);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT1_5);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT2);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT3);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT4);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTA);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTC);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTX);

    ReleaseOutHwInfoStructs();

    drm->StoredDeviceID = ICFL_GT3_ULT_DEVICE_F0_ID;
    drm->StoredSSVal = 6;
    drm->setGtType(GTTYPE_GT3);
    ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ((unsigned short)drm->StoredDeviceID, outHwInfo.pPlatform->usDeviceID);
    EXPECT_EQ((unsigned short)drm->StoredDeviceRevID, outHwInfo.pPlatform->usRevId);
    EXPECT_EQ((uint32_t)drm->StoredEUVal, outHwInfo.pSysInfo->EUCount);
    EXPECT_EQ((uint32_t)drm->StoredSSVal, outHwInfo.pSysInfo->SubSliceCount);
    EXPECT_EQ(2u, outHwInfo.pSysInfo->SliceCount);
    EXPECT_EQ(EngineType::ENGINE_RCS, outHwInfo.capabilityTable.defaultEngineType);

    EXPECT_EQ(GTTYPE_GT3, outHwInfo.pPlatform->eGTType);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT1);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT1_5);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT2);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrGT3);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT4);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTA);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTC);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTX);

    auto &outKmdNotifyProperties = outHwInfo.capabilityTable.kmdNotifyProperties;
    EXPECT_TRUE(outKmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(50000, outKmdNotifyProperties.delayKmdNotifyMicroseconds);
    EXPECT_TRUE(outKmdNotifyProperties.enableQuickKmdSleep);
    EXPECT_EQ(5000, outKmdNotifyProperties.delayQuickKmdSleepMicroseconds);
    EXPECT_TRUE(outKmdNotifyProperties.enableQuickKmdSleepForSporadicWaits);
    EXPECT_EQ(200000, outKmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds);
}

CFLTEST_F(HwInfoConfigTestLinuxCfl, negativeUnknownDevId) {
    drm->StoredDeviceID = 0;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);
}

CFLTEST_F(HwInfoConfigTestLinuxCfl, negativeFailedIoctlDevId) {
    drm->StoredRetValForDeviceID = -2;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-2, ret);
}

CFLTEST_F(HwInfoConfigTestLinuxCfl, negativeFailedIoctlDevRevId) {
    drm->StoredRetValForDeviceRevID = -3;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-3, ret);
}

CFLTEST_F(HwInfoConfigTestLinuxCfl, negativeFailedIoctlEuCount) {
    drm->StoredRetValForEUVal = -4;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-4, ret);
}

CFLTEST_F(HwInfoConfigTestLinuxCfl, negativeFailedIoctlSsCount) {
    drm->StoredRetValForSSVal = -5;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-5, ret);
}

CFLTEST_F(HwInfoConfigTestLinuxCfl, configureHwInfoWaFlags) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    drm->StoredDeviceRevID = 0;
    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, outHwInfo.pWaTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads);

    ReleaseOutHwInfoStructs();
}

CFLTEST_F(HwInfoConfigTestLinuxCfl, configureHwInfoEdram) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    int ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ_VAL(0u, outHwInfo.pSysInfo->EdramSizeInKb);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrEDram);
    ReleaseOutHwInfoStructs();

    drm->StoredDeviceID = ICFL_GT3_ULT_28W_DEVICE_F0_ID;
    drm->setGtType(GTTYPE_GT3);
    ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ_VAL((64u * 1024u), outHwInfo.pSysInfo->EdramSizeInKb);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrEDram);
    ReleaseOutHwInfoStructs();

    drm->StoredDeviceID = ICFL_GT3_ULT_15W_DEVICE_F0_ID;
    ret = hwInfoConfig->configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ_VAL((64u * 1024u), outHwInfo.pSysInfo->EdramSizeInKb);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrEDram);
}

CFLTEST_F(HwInfoConfigTestLinuxCfl, whenCallAdjustPlatformThenDoNothing) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    hwInfoConfig->adjustPlatformForProductFamily(&testHwInfo);

    int ret = memcmp(testHwInfo.pPlatform, pInHwInfo->pPlatform, sizeof(PLATFORM));
    EXPECT_EQ(0, ret);
}

template <typename T>
class CflHwInfoTests : public ::testing::Test {
};
typedef ::testing::Types<CFL_1x2x6, CFL_1x3x6, CFL_1x3x8, CFL_2x3x8, CFL_3x3x8> cflTestTypes;
TYPED_TEST_CASE(CflHwInfoTests, cflTestTypes);
TYPED_TEST(CflHwInfoTests, gtSetupIsCorrect) {
    GT_SYSTEM_INFO gtSystemInfo;
    memset(&gtSystemInfo, 0, sizeof(gtSystemInfo));
    TypeParam::setupGtSystemInfo(&gtSystemInfo);
    EXPECT_GT(gtSystemInfo.EUCount, 0u);
    EXPECT_GT(gtSystemInfo.ThreadCount, 0u);
    EXPECT_GT(gtSystemInfo.SliceCount, 0u);
    EXPECT_GT(gtSystemInfo.SubSliceCount, 0u);
    EXPECT_GT_VAL(gtSystemInfo.L3CacheSizeInKb, 0u);
    EXPECT_EQ(gtSystemInfo.CsrSizeInMb, 8u);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
}
