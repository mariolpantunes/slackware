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
#include "runtime/device_queue/device_queue_hw.h"
#include "runtime/device_queue/device_queue.h"

namespace OCLRT {
template <typename GfxFamily>
class MockDeviceQueueHw : public DeviceQueueHw<GfxFamily> {
    using BaseClass = DeviceQueueHw<GfxFamily>;
    using MI_ATOMIC = typename GfxFamily::MI_ATOMIC;
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    using MI_ARB_CHECK = typename GfxFamily::MI_ARB_CHECK;
    using MEDIA_STATE_FLUSH = typename GfxFamily::MEDIA_STATE_FLUSH;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename GfxFamily::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    using GPGPU_WALKER = typename GfxFamily::GPGPU_WALKER;
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;
    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;

  public:
    using BaseClass::addArbCheckCmdWa;
    using BaseClass::addLriCmd;
    using BaseClass::addLriCmdWa;
    using BaseClass::addMediaStateClearCmds;
    using BaseClass::addMiAtomicCmdWa;
    using BaseClass::addPipeControlCmdWa;
    using BaseClass::addProfilingEndCmds;
    using BaseClass::buildSlbDummyCommands;
    using BaseClass::getCSPrefetchSize;
    using BaseClass::getExecutionModelCleanupSectionSize;
    using BaseClass::getMediaStateClearCmdsSize;
    using BaseClass::getMinimumSlbSize;
    using BaseClass::getProfilingEndCmdsSize;
    using BaseClass::getSlbCS;
    using BaseClass::getWaCommandsSize;
    using BaseClass::offsetDsh;

    bool arbCheckWa;
    bool miAtomicWa;
    bool lriWa;
    bool pipeControlWa;

    struct ExpectedCmds {
        MEDIA_STATE_FLUSH mediaStateFlush;
        MI_ARB_CHECK arbCheck;
        MI_ATOMIC miAtomic;
        MEDIA_INTERFACE_DESCRIPTOR_LOAD mediaIdLoad;
        MI_LOAD_REGISTER_IMM lriTrue;
        MI_LOAD_REGISTER_IMM lriFalse;
        PIPE_CONTROL pipeControl;
        PIPE_CONTROL noopedPipeControl;
        GPGPU_WALKER gpgpuWalker;
        uint8_t *prefetch;
        MI_BATCH_BUFFER_START bbStart;
    } expectedCmds;

    MockDeviceQueueHw(Context *context,
                      Device *device,
                      cl_queue_properties &properties) : BaseClass(context, device, properties) {
        auto slb = this->getSlbBuffer();
        LinearStream *slbCS = getSlbCS();
        slbCS->replaceBuffer(slb->getUnderlyingBuffer(), slb->getUnderlyingBufferSize());
        size_t size = slbCS->getUsed();

        lriWa = false;
        addLriCmdWa(true);
        if (slbCS->getUsed() > size) {
            size = slbCS->getUsed();
            lriWa = true;
        }
        pipeControlWa = false;
        addPipeControlCmdWa();
        if (slbCS->getUsed() > size) {
            size = slbCS->getUsed();
            pipeControlWa = true;
        }
        arbCheckWa = false;
        addArbCheckCmdWa();
        if (slbCS->getUsed() > size) {
            size = slbCS->getUsed();
            arbCheckWa = true;
        }
        miAtomicWa = false;
        addMiAtomicCmdWa(0);
        if (slbCS->getUsed() > size) {
            size = slbCS->getUsed();
            miAtomicWa = true;
        }
        slbCS->replaceBuffer(slb->getUnderlyingBuffer(), slb->getUnderlyingBufferSize()); // reset

        setupExpectedCmds();
    };

    ~MockDeviceQueueHw() {
        if (expectedCmds.prefetch)
            delete expectedCmds.prefetch;
    }

    MI_ATOMIC getExpectedMiAtomicCmd() {
        auto igilCmdQueue = reinterpret_cast<IGIL_CommandQueue *>(this->queueBuffer->getUnderlyingBuffer());
        auto placeholder = (uint64_t)&igilCmdQueue->m_controls.m_DummyAtomicOperationPlaceholder;

        MI_ATOMIC miAtomic = MI_ATOMIC::sInit();
        miAtomic.setAtomicOpcode(MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_8B_INCREMENT);
        miAtomic.setReturnDataControl(0x1);
        miAtomic.setCsStall(0x1);
        miAtomic.setDataSize(MI_ATOMIC::DATA_SIZE::DATA_SIZE_QWORD);
        miAtomic.setMemoryAddress(static_cast<uint32_t>(placeholder & 0x0000FFFFFFFFULL));
        miAtomic.setMemoryAddressHigh(static_cast<uint32_t>((placeholder >> 32) & 0x0000FFFFFFFFULL));

        return miAtomic;
    };

    MI_LOAD_REGISTER_IMM getExpectedLriCmd(bool arbCheck) {
        MI_LOAD_REGISTER_IMM lri = MI_LOAD_REGISTER_IMM::sInit();
        lri.setRegisterOffset(0x2248); // CTXT_PREMP_DBG offset
        if (arbCheck)
            lri.setDataDword(0x00000100); // set only bit 8 (Preempt On MI_ARB_CHK Only)
        else
            lri.setDataDword(0x0); // default value

        return lri;
    }

    PIPE_CONTROL getExpectedPipeControlCmd() {
        PIPE_CONTROL pc;
        this->initPipeControl(&pc);
        return pc;
    }

    MI_ARB_CHECK getExpectedArbCheckCmd() {
        return MI_ARB_CHECK::sInit();
    }

    void setupExpectedCmds() {
        expectedCmds.mediaStateFlush = MEDIA_STATE_FLUSH::sInit();
        expectedCmds.arbCheck = getExpectedArbCheckCmd();
        expectedCmds.miAtomic = getExpectedMiAtomicCmd();
        expectedCmds.mediaIdLoad = MEDIA_INTERFACE_DESCRIPTOR_LOAD::sInit();
        expectedCmds.mediaIdLoad.setInterfaceDescriptorTotalLength(2048);

        auto dataStartAddress = DeviceQueue::colorCalcStateSize;

        // add shift to second table ( 62 index of first ID table with scheduler )
        dataStartAddress += sizeof(INTERFACE_DESCRIPTOR_DATA) * DeviceQueue::schedulerIDIndex;

        expectedCmds.mediaIdLoad.setInterfaceDescriptorDataStartAddress(dataStartAddress);
        expectedCmds.lriTrue = getExpectedLriCmd(true);
        expectedCmds.lriFalse = getExpectedLriCmd(false);
        expectedCmds.pipeControl = getExpectedPipeControlCmd();
        memset(&expectedCmds.noopedPipeControl, 0x0, sizeof(PIPE_CONTROL));
        expectedCmds.gpgpuWalker = GPGPU_WALKER::sInit();
        expectedCmds.gpgpuWalker.setSimdSize(GPGPU_WALKER::SIMD_SIZE::SIMD_SIZE_SIMD16);
        expectedCmds.gpgpuWalker.setThreadGroupIdXDimension(1);
        expectedCmds.gpgpuWalker.setThreadGroupIdYDimension(1);
        expectedCmds.gpgpuWalker.setThreadGroupIdZDimension(1);
        expectedCmds.gpgpuWalker.setRightExecutionMask(0xFFFFFFFF);
        expectedCmds.gpgpuWalker.setBottomExecutionMask(0xFFFFFFFF);
        expectedCmds.prefetch = new uint8_t[DeviceQueueHw<GfxFamily>::getCSPrefetchSize()];
        memset(expectedCmds.prefetch, 0x0, DeviceQueueHw<GfxFamily>::getCSPrefetchSize());
        expectedCmds.bbStart = MI_BATCH_BUFFER_START::sInit();
        auto slbPtr = reinterpret_cast<uintptr_t>(this->getSlbBuffer()->getUnderlyingBuffer());
        expectedCmds.bbStart.setBatchBufferStartAddressGraphicsaddress472(slbPtr);
    }

    IGIL_CommandQueue *getIgilQueue() {
        auto igilCmdQueue = reinterpret_cast<IGIL_CommandQueue *>(DeviceQueue::queueBuffer->getUnderlyingBuffer());
        return igilCmdQueue;
    }
};
} // namespace OCLRT
