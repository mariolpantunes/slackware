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

#include "unit_tests/helpers/hw_helper_tests.h"
#include "runtime/helpers/options.h"

void HwHelperTest::SetUp() {
    memcpy(&testPlatform, platformDevices[0]->pPlatform, sizeof(testPlatform));
    memcpy(&testFtrTable, platformDevices[0]->pSkuTable, sizeof(testFtrTable));
    memcpy(&testWaTable, platformDevices[0]->pWaTable, sizeof(testWaTable));
    memcpy(&testSysInfo, platformDevices[0]->pSysInfo, sizeof(testSysInfo));
    hwInfo.capabilityTable = platformDevices[0]->capabilityTable;
    hwInfo.pPlatform = &testPlatform;
    hwInfo.pSkuTable = &testFtrTable;
    hwInfo.pSysInfo = &testSysInfo;
    hwInfo.pWaTable = &testWaTable;
}
void HwHelperTest::TearDown() {
}

HWTEST_F(HwHelperTest, getReturnsValidHwHelperHw) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_NE(nullptr, &helper);
}

HWTEST_F(HwHelperTest, getBindingTableStateSurfaceStatePointerReturnsCorrectPointer) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    BINDING_TABLE_STATE bindingTableState[4];

    bindingTableState[2].getRawData(0) = 0x00123456;

    auto &helper = HwHelper::get(renderCoreFamily);

    auto pointer = helper.getBindingTableStateSurfaceStatePointer(bindingTableState, 2);
    EXPECT_EQ(0x00123456u, pointer);
}

HWTEST_F(HwHelperTest, getBindingTableStateSizeReturnsCorrectSize) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;

    auto &helper = HwHelper::get(renderCoreFamily);

    auto pointer = helper.getBindingTableStateSize();
    EXPECT_EQ(sizeof(BINDING_TABLE_STATE), pointer);
}

HWTEST_F(HwHelperTest, getBindingTableStateAlignementReturnsCorrectSize) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_NE(0u, helper.getBindingTableStateAlignement());
}

HWTEST_F(HwHelperTest, getInterfaceDescriptorDataSizeReturnsCorrectSize) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    auto &helper = HwHelper::get(renderCoreFamily);

    EXPECT_EQ(sizeof(INTERFACE_DESCRIPTOR_DATA), helper.getInterfaceDescriptorDataSize());
}

HWTEST_F(HwHelperTest, givenDebuggingInactiveWhenSipKernelTypeIsQueriedThenCsrTypeIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_NE(nullptr, &helper);

    auto sipType = helper.getSipKernelType(false);
    EXPECT_EQ(SipKernelType::Csr, sipType);
}

TEST(DwordBuilderTest, setNonMaskedBits) {
    uint32_t dword = 0;

    // expect non-masked bit 2
    uint32_t expectedDword = (1 << 2);
    dword = DwordBuilder::build(2, false, true, 0); // set 2nd bit
    EXPECT_EQ(expectedDword, dword);

    // expect non-masked bits 2 and 3
    expectedDword |= (1 << 3);
    dword = DwordBuilder::build(3, false, true, dword); // set 3rd bit with init value
    EXPECT_EQ(expectedDword, dword);
}

TEST(DwordBuilderTest, setMaskedBits) {
    uint32_t dword = 0;

    // expect masked bit 2
    uint32_t expectedDword = (1 << 2);
    expectedDword |= (1 << (2 + 16));
    dword = DwordBuilder::build(2, true, true, 0); // set 2nd bit (masked)
    EXPECT_EQ(expectedDword, dword);

    // expect masked bits 2 and 3
    expectedDword |= (1 << 3);
    expectedDword |= (1 << (3 + 16));
    dword = DwordBuilder::build(3, true, true, dword); // set 3rd bit (masked) with init value
    EXPECT_EQ(expectedDword, dword);
}

TEST(DwordBuilderTest, setMaskedBitsWithDifferentBitValue) {
    // expect only mask bit
    uint32_t expectedDword = 1 << (2 + 16);
    auto dword = DwordBuilder::build(2, true, false, 0);
    EXPECT_EQ(expectedDword, dword);

    // expect masked bits 3
    expectedDword = (1 << 3);
    expectedDword |= (1 << (3 + 16));
    dword = DwordBuilder::build(3, true, true, 0);
    EXPECT_EQ(expectedDword, dword);
}

using LriHelperTests = ::testing::Test;

HWTEST_F(LriHelperTests, givenAddressAndOffsetWhenHelperIsUsedThenProgramCmdStream) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    std::unique_ptr<uint8_t> buffer(new uint8_t[128]);

    LinearStream stream(buffer.get(), 128);
    uint32_t address = 0x8888;
    uint32_t data = 0x1234;

    auto expectedLri = MI_LOAD_REGISTER_IMM::sInit();
    expectedLri.setRegisterOffset(address);
    expectedLri.setDataDword(data);

    auto lri = LriHelper<FamilyType>::program(&stream, address, data);

    EXPECT_EQ(sizeof(MI_LOAD_REGISTER_IMM), stream.getUsed());
    EXPECT_EQ(lri, stream.getCpuBase());
    EXPECT_TRUE(memcmp(lri, &expectedLri, sizeof(MI_LOAD_REGISTER_IMM)) == 0);
}

TEST(HwInfoTest, givenHwInfoWhenIsCoreThenPlatformTypeIsCore) {
    HardwareInfo hwInfo;
    hwInfo.capabilityTable.isCore = true;
    auto platformType = getPlatformType(hwInfo);
    EXPECT_STREQ("core", platformType);
}

TEST(HwInfoTest, givenHwInfoWhenIsNotCoreThenPlatformTypeIsLp) {
    HardwareInfo hwInfo;
    hwInfo.capabilityTable.isCore = false;
    auto platformType = getPlatformType(hwInfo);
    EXPECT_STREQ("lp", platformType);
}
