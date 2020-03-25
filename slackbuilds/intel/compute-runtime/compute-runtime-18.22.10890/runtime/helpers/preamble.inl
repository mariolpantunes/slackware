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

#include "runtime/helpers/preamble.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/command_stream/preemption.h"
#include "hw_cmds.h"
#include "reg_configs_common.h"
#include "runtime/device/device.h"
#include "runtime/kernel/kernel.h"
#include "runtime/helpers/aligned_memory.h"
#include <cstddef>

namespace OCLRT {

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programThreadArbitration(LinearStream *pCommandStream, uint32_t requiredThreadArbitrationPolicy) {
}

template <typename GfxFamily>
size_t PreambleHelper<GfxFamily>::getThreadArbitrationCommandsSize() {
    return 0;
}

template <typename GfxFamily>
uint32_t PreambleHelper<GfxFamily>::getDefaultThreadArbitrationPolicy() {
    return 0;
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programGenSpecificPreambleWorkArounds(LinearStream *pCommandStream, const HardwareInfo &hwInfo) {
}

template <typename GfxFamily>
size_t PreambleHelper<GfxFamily>::getAdditionalCommandsSize(const Device &device) {
    size_t totalSize = getKernelDebuggingCommandsSize(device.isSourceLevelDebuggerActive());
    return totalSize;
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programVFEState(LinearStream *pCommandStream, const HardwareInfo &hwInfo, int scratchSize, uint64_t scratchAddress) {
    typedef typename GfxFamily::MEDIA_VFE_STATE MEDIA_VFE_STATE;

    addPipeControlBeforeVfeCmd(pCommandStream, &hwInfo);

    auto pMediaVfeState = (MEDIA_VFE_STATE *)pCommandStream->getSpace(sizeof(MEDIA_VFE_STATE));
    *pMediaVfeState = MEDIA_VFE_STATE::sInit();
    pMediaVfeState->setMaximumNumberOfThreads(hwInfo.pSysInfo->ThreadCount);
    pMediaVfeState->setNumberOfUrbEntries(1);
    pMediaVfeState->setUrbEntryAllocationSize(PreambleHelper<GfxFamily>::getUrbEntryAllocationSize());
    pMediaVfeState->setPerThreadScratchSpace(Kernel::getScratchSizeValueToProgramMediaVfeState(scratchSize));
    pMediaVfeState->setStackSize(Kernel::getScratchSizeValueToProgramMediaVfeState(scratchSize));
    uint32_t lowAddress = uint32_t(0xFFFFFFFF & scratchAddress);
    uint32_t highAddress = uint32_t(0xFFFFFFFF & (scratchAddress >> 32));
    pMediaVfeState->setScratchSpaceBasePointer(lowAddress);
    pMediaVfeState->setScratchSpaceBasePointerHigh(highAddress);
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programL3(LinearStream *pCommandStream, uint32_t l3Config) {
    auto pCmd = (MI_LOAD_REGISTER_IMM *)pCommandStream->getSpace(sizeof(MI_LOAD_REGISTER_IMM));
    *pCmd = MI_LOAD_REGISTER_IMM::sInit();

    pCmd->setRegisterOffset(L3CNTLRegisterOffset<GfxFamily>::registerOffset);
    pCmd->setDataDword(l3Config);
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programPreamble(LinearStream *pCommandStream, Device &device, uint32_t l3Config,
                                                uint32_t requiredThreadArbitrationPolicy, GraphicsAllocation *preemptionCsr) {
    programL3(pCommandStream, l3Config);
    programThreadArbitration(pCommandStream, requiredThreadArbitrationPolicy);
    programPreemption(pCommandStream, device, preemptionCsr);
    if (device.isSourceLevelDebuggerActive()) {
        programKernelDebugging(pCommandStream);
    }
    programGenSpecificPreambleWorkArounds(pCommandStream, device.getHardwareInfo());
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programPreemption(LinearStream *pCommandStream, Device &device, GraphicsAllocation *preemptionCsr) {
    PreemptionHelper::programPreamble<GfxFamily>(*pCommandStream, device, preemptionCsr);
}

template <typename GfxFamily>
uint32_t PreambleHelper<GfxFamily>::getUrbEntryAllocationSize() {
    return 0x782;
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programKernelDebugging(LinearStream *pCommandStream) {
    auto pCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(pCommandStream->getSpace(sizeof(MI_LOAD_REGISTER_IMM)));
    *pCmd = MI_LOAD_REGISTER_IMM::sInit();
    pCmd->setRegisterOffset(DebugModeRegisterOffset::registerOffset);
    pCmd->setDataDword(DebugModeRegisterOffset::debugEnabledValue);

    auto pCmd2 = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(pCommandStream->getSpace(sizeof(MI_LOAD_REGISTER_IMM)));
    *pCmd2 = MI_LOAD_REGISTER_IMM::sInit();
    pCmd2->setRegisterOffset(TdDebugControlRegisterOffset::registerOffset);
    pCmd2->setDataDword(TdDebugControlRegisterOffset::debugEnabledValue);
}

template <typename GfxFamily>
size_t PreambleHelper<GfxFamily>::getKernelDebuggingCommandsSize(bool debuggingActive) {
    if (debuggingActive) {
        return 2 * sizeof(MI_LOAD_REGISTER_IMM);
    }
    return 0;
}

} // namespace OCLRT
