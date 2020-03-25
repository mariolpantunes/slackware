/*
 * Copyright (c) 2017, Intel Corporation
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

#include "runtime/device/device.h"
#include "unit_tests/fixtures/platform_fixture.h"

#include "test.h"
#include "gtest/gtest.h"

using namespace OCLRT;
using namespace std;

struct HwInfoConfigTest : public ::testing::Test,
                          public PlatformFixture {
    void SetUp() override;
    void TearDown() override;
    void ReleaseOutHwInfoStructs();

    HardwareInfo *pInHwInfo;
    HardwareInfo outHwInfo;

    RuntimeCapabilityTable originalCapTable;

    const PLATFORM *pOldPlatform;
    PLATFORM testPlatform;

    const FeatureTable *pOldSkuTable;
    FeatureTable testSkuTable;

    const WorkaroundTable *pOldWaTable;
    WorkaroundTable testWaTable;

    const GT_SYSTEM_INFO *pOldSysInfo;
    GT_SYSTEM_INFO testSysInfo;
};
