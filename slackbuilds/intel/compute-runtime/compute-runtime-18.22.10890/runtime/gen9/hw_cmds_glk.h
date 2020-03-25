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

#pragma once
#include "runtime/gen9/hw_cmds_base.h"

namespace OCLRT {

struct GLK : public SKLFamily {
    static const PLATFORM platform;
    static const HardwareInfo hwInfo;

    static const uint32_t threadsPerEu = 6;
    static const uint32_t maxEuPerSubslice = 6;
    static const uint32_t maxSlicesSupported = 1;
    static const uint32_t maxSubslicesSupported = 3;

    static const RuntimeCapabilityTable capabilityTable;
    static void (*setupGtSystemInfo)(GT_SYSTEM_INFO *gtSysInfo);
};

class GLK_1x3x6 : public GLK {
  public:
    static void setupGtSystemInfo(GT_SYSTEM_INFO *gtSysInfo);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};

class GLK_1x2x6 : public GLK {
  public:
    static void setupGtSystemInfo(GT_SYSTEM_INFO *gtSysInfo);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};
} // namespace OCLRT
