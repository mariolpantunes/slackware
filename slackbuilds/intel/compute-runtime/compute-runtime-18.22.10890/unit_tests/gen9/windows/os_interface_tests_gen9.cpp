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

#include "unit_tests/os_interface/windows/os_interface_tests.h"
#include "unit_tests/gen_common/test.h"

typedef OsInterfaceTest OsInterfaceTestSkl;

GEN9TEST_F(OsInterfaceTestSkl, askKmdIfPreemptionRegisterWhitelisted) {
    HardwareInfo *hwInfo = nullptr;
    const HardwareInfo *refHwinfo = *platformDevices;
    size_t numDevices = 0;

    bool success = DeviceFactory::getDevices(&hwInfo, numDevices);
    EXPECT_TRUE(success);

    for (size_t i = 0u; i < numDevices; i++) {
        if (hwInfo[i].pWaTable->waEnablePreemptionGranularityControlByUMD) {
            EXPECT_TRUE(hwInfo[i].capabilityTable.whitelistedRegisters.csChicken1_0x2580);
        } else {
            EXPECT_FALSE(hwInfo[i].capabilityTable.whitelistedRegisters.csChicken1_0x2580);
        }
    }

    DeviceFactory::releaseDevices();
}
