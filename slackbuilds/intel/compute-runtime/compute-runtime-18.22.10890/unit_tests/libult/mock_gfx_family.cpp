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

#include "unit_tests/libult/mock_gfx_family.h"
#include "runtime/command_queue/gpgpu_walker.inl"
#include "runtime/command_stream/preemption.inl"
#include "runtime/device_queue/device_queue_hw.h"
#include "runtime/device_queue/device_queue_hw.inl"
#include "runtime/helpers/hw_helper.inl"
#include "runtime/helpers/kernel_commands.inl"
#include "runtime/helpers/preamble.inl"

namespace OCLRT {

bool (*GENX::isSimulationFcn)(unsigned short) = nullptr;

GENX::GPGPU_WALKER GENX::cmdInitGpgpuWalker = GENX::GPGPU_WALKER::sInit();
GENX::INTERFACE_DESCRIPTOR_DATA GENX::cmdInitInterfaceDescriptorData = GENX::INTERFACE_DESCRIPTOR_DATA::sInit();
GENX::MEDIA_STATE_FLUSH GENX::cmdInitMediaStateFlush = GENX::MEDIA_STATE_FLUSH::sInit();
GENX::MEDIA_INTERFACE_DESCRIPTOR_LOAD GENX::cmdInitMediaInterfaceDescriptorLoad = GENX::MEDIA_INTERFACE_DESCRIPTOR_LOAD::sInit();

template <>
size_t HwHelperHw<GENX>::getMaxBarrierRegisterPerSlice() const {
    return 32;
}

template <>
void HwHelperHw<GENX>::setCapabilityCoherencyFlag(const HardwareInfo *pHwInfo, bool &coherencyFlag) {
    PLATFORM *pPlatform = const_cast<PLATFORM *>(pHwInfo->pPlatform);
    if (pPlatform->usDeviceID == 20) {
        coherencyFlag = false;
    } else {
        coherencyFlag = true;
    }
}

template <>
bool HwHelperHw<GENX>::setupPreemptionRegisters(HardwareInfo *pHwInfo, bool enable) {
    pHwInfo->capabilityTable.whitelistedRegisters.csChicken1_0x2580 = enable;
    return enable;
}

struct hw_helper_static_init {
    hw_helper_static_init() {
        hwHelperFactory[IGFX_UNKNOWN_CORE] = &HwHelperHw<GENX>::get();
    }
};

template class HwHelperHw<GENX>;

hw_helper_static_init si;

template class GpgpuWalkerHelper<GENX>;

template <>
bool KernelCommandsHelper<GENX>::isPipeControlWArequired() {
    return false;
}

template struct KernelCommandsHelper<GENX>;

template <>
size_t PreemptionHelper::getRequiredCmdStreamSize<GENX>(PreemptionMode newPreemptionMode, PreemptionMode oldPreemptionMode) {
    return 0;
}

template <>
void PreemptionHelper::programCmdStream<GENX>(LinearStream &cmdStream, PreemptionMode newPreemptionMode, PreemptionMode oldPreemptionMode,
                                              GraphicsAllocation *preemptionCsr, Device &device) {
}

template <>
size_t PreemptionHelper::getRequiredPreambleSize<GENX>(const Device &device) {
    return 0;
}

template <>
void PreemptionHelper::programPreamble<GENX>(LinearStream &preambleCmdStream, Device &device,
                                             const GraphicsAllocation *preemptionCsr) {
}

template <>
size_t PreemptionHelper::getPreemptionWaCsSize<GENX>(const Device &device) {
    return 0;
}

template void PreemptionHelper::programInterfaceDescriptorDataPreemption<GENX>(INTERFACE_DESCRIPTOR_DATA<GENX> *idd, PreemptionMode preemptionMode);

template <>
size_t DeviceQueueHw<GENX>::getWaCommandsSize() {
    return (size_t)0;
}

template <>
void DeviceQueueHw<GENX>::addArbCheckCmdWa() {
}

template <>
void DeviceQueueHw<GENX>::addMiAtomicCmdWa(uint64_t atomicOpPlaceholder) {
}

template <>
void DeviceQueueHw<GENX>::addLriCmdWa(bool setArbCheck) {
}

template <>
void DeviceQueueHw<GENX>::addPipeControlCmdWa(bool isNoopCmd) {
}

template <>
void DeviceQueueHw<GENX>::addProfilingEndCmds(uint64_t timestampAddress) {
}

template class DeviceQueueHw<GENX>;

template <>
void PreambleHelper<GENX>::addPipeControlBeforeVfeCmd(LinearStream *pCommandStream, const HardwareInfo *hwInfo) {
}

template <>
uint32_t PreambleHelper<GENX>::getL3Config(const HardwareInfo &hwInfo, bool useSLM) {
    uint32_t l3Config = 0;
    return l3Config;
}

template <>
void PreambleHelper<GENX>::programPipelineSelect(LinearStream *pCommandStream, bool mediaSamplerRequired) {
}

template <>
struct L3CNTLRegisterOffset<GENX> {
    static const uint32_t registerOffset = 0x7034;
};

template struct PreambleHelper<GENX>;

} // namespace OCLRT
