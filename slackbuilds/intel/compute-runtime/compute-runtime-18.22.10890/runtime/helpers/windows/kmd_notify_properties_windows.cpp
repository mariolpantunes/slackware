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

#include "runtime/helpers/kmd_notify_properties.h"
#include "runtime/os_interface/windows/sys_calls.h"

using namespace OCLRT;

void KmdNotifyHelper::updateAcLineStatus() {
    SYSTEM_POWER_STATUS systemPowerStatus = {};
    auto powerStatusRetValue = SysCalls::getSystemPowerStatus(&systemPowerStatus);
    if (powerStatusRetValue == 1) {
        acLineConnected = (systemPowerStatus.ACLineStatus == 1);
    }
}

int64_t KmdNotifyHelper::getBaseTimeout(const int64_t &multiplier) const {
    return properties->delayKmdNotifyMicroseconds;
}
