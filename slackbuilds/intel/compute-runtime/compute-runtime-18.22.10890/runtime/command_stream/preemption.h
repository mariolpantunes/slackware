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

#pragma once
#include "runtime/command_stream/linear_stream.h"
#include "runtime/helpers/hw_helper.h"

namespace OCLRT {
class Kernel;
class Device;
class GraphicsAllocation;
struct MultiDispatchInfo;

class PreemptionHelper {
  public:
    template <typename CmdFamily>
    using INTERFACE_DESCRIPTOR_DATA = typename CmdFamily::INTERFACE_DESCRIPTOR_DATA;

    static PreemptionMode taskPreemptionMode(Device &device, Kernel *kernel);
    static PreemptionMode taskPreemptionMode(Device &device, const MultiDispatchInfo &multiDispatchInfo);
    static bool allowThreadGroupPreemption(Kernel *kernel, const WorkaroundTable *waTable);
    static bool allowMidThreadPreemption(Kernel *kernel, Device &device);
    static void adjustDefaultPreemptionMode(RuntimeCapabilityTable &deviceCapabilities, bool allowMidThread, bool allowThreadGroup, bool allowMidBatch);

    template <typename GfxFamily>
    static size_t getRequiredPreambleSize(const Device &device);

    template <typename GfxFamily>
    static void programPreamble(LinearStream &preambleCmdStream, Device &device, const GraphicsAllocation *preemptionCsr);

    template <typename GfxFamily>
    static size_t getRequiredCmdStreamSize(PreemptionMode newPreemptionMode, PreemptionMode oldPreemptionMode);

    template <typename GfxFamily>
    static void programCmdStream(LinearStream &cmdStream, PreemptionMode newPreemptionMode, PreemptionMode oldPreemptionMode,
                                 GraphicsAllocation *preemptionCsr, Device &device);

    template <typename GfxFamily>
    static size_t getPreemptionWaCsSize(const Device &device);

    template <typename GfxFamily>
    static void applyPreemptionWaCmdsBegin(LinearStream *pCommandStream, const Device &device);

    template <typename GfxFamily>
    static void applyPreemptionWaCmdsEnd(LinearStream *pCommandStream, const Device &device);

    static PreemptionMode getDefaultPreemptionMode(const HardwareInfo &hwInfo);

    template <typename GfxFamily>
    static void programInterfaceDescriptorDataPreemption(INTERFACE_DESCRIPTOR_DATA<GfxFamily> *idd, PreemptionMode preemptionMode);
};

template <typename GfxFamily>
struct PreemptionConfig {
    static const uint32_t mmioAddress;
    static const uint32_t maskVal;
    static const uint32_t maskShift;
    static const uint32_t mask;

    static const uint32_t threadGroupVal;
    static const uint32_t cmdLevelVal;
    static const uint32_t midThreadVal;
};

} // namespace OCLRT
