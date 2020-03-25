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
#include "runtime/built_ins/built_ins.h"
#include "runtime/command_stream/preemption.h"
#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/fixtures/preemption_fixture.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_submissions_aggregator.h"

namespace OCLRT {

template <>
void HardwareParse::findCsrBaseAddress<SKLFamily>() {
    typedef typename GEN9::GPGPU_CSR_BASE_ADDRESS GPGPU_CSR_BASE_ADDRESS;
    itorGpgpuCsrBaseAddress = find<GPGPU_CSR_BASE_ADDRESS *>(cmdList.begin(), itorWalker);
    if (itorGpgpuCsrBaseAddress != itorWalker) {
        cmdGpgpuCsrBaseAddress = *itorGpgpuCsrBaseAddress;
    }
}
} // namespace OCLRT

using namespace OCLRT;

using Gen9PreemptionTests = DevicePreemptionTests;
using Gen9PreemptionEnqueueKernelTest = PreemptionEnqueueKernelTest;
using Gen9MidThreadPreemptionEnqueueKernelTest = MidThreadPreemptionEnqueueKernelTest;
using Gen9ThreadGroupPreemptionEnqueueKernelTest = ThreadGroupPreemptionEnqueueKernelTest;

template <>
PreemptionTestHwDetails GetPreemptionTestHwDetails<SKLFamily>() {
    PreemptionTestHwDetails ret;
    ret.modeToRegValueMap[PreemptionMode::ThreadGroup] = DwordBuilder::build(1, true) | DwordBuilder::build(2, true, false);
    ret.modeToRegValueMap[PreemptionMode::MidBatch] = DwordBuilder::build(2, true) | DwordBuilder::build(1, true, false);
    ret.modeToRegValueMap[PreemptionMode::MidThread] = DwordBuilder::build(2, true, false) | DwordBuilder::build(1, true, false);
    ret.defaultRegValue = ret.modeToRegValueMap[PreemptionMode::MidBatch];
    ret.regAddress = 0x2580u;
    return ret;
}

GEN9TEST_F(Gen9PreemptionTests, whenMidThreadPreemptionIsNotAvailableThenDoesNotProgramPreamble) {
    device->setPreemptionMode(PreemptionMode::ThreadGroup);

    size_t requiredSize = PreemptionHelper::getRequiredPreambleSize<FamilyType>(*device);
    EXPECT_EQ(0U, requiredSize);

    LinearStream cmdStream{nullptr, 0};
    PreemptionHelper::programPreamble<FamilyType>(cmdStream, *device, nullptr);
    EXPECT_EQ(0U, cmdStream.getUsed());
}

GEN9TEST_F(Gen9PreemptionTests, whenMidThreadPreemptionIsAvailableThenProgramsPreamble) {
    using GPGPU_CSR_BASE_ADDRESS = typename FamilyType::GPGPU_CSR_BASE_ADDRESS;
    using STATE_SIP = typename FamilyType::STATE_SIP;

    device->setPreemptionMode(PreemptionMode::MidThread);
    executionEnvironment->DisableMidThreadPreemption = 0;

    size_t minCsrSize = device->getHardwareInfo().pSysInfo->CsrSizeInMb * MemoryConstants::megaByte;
    uint64_t minCsrAlignment = 2 * 256 * MemoryConstants::kiloByte;
    MockGraphicsAllocation csrSurface((void *)minCsrAlignment, minCsrSize);

    // verify preamble programming
    size_t requiredPreambleSize = PreemptionHelper::getRequiredPreambleSize<FamilyType>(*device);
    size_t expectedPreambleSize = sizeof(GPGPU_CSR_BASE_ADDRESS) + sizeof(STATE_SIP);
    EXPECT_EQ(expectedPreambleSize, requiredPreambleSize);

    StackVec<char, 8192> preambleStorage(requiredPreambleSize);
    ASSERT_LE(requiredPreambleSize, preambleStorage.size());
    LinearStream preambleCmdStream{preambleStorage.begin(), preambleStorage.size()};
    PreemptionHelper::programPreamble<FamilyType>(preambleCmdStream, *device, &csrSurface);

    HardwareParse hwParsePreamble;
    hwParsePreamble.parseCommands<FamilyType>(preambleCmdStream);

    auto csrBaseAddressCmd = hwParsePreamble.getCommand<GPGPU_CSR_BASE_ADDRESS>();
    ASSERT_NE(nullptr, csrBaseAddressCmd);
    EXPECT_EQ(csrSurface.getGpuAddressToPatch(), csrBaseAddressCmd->getGpgpuCsrBaseAddress());

    auto stateSipCmd = hwParsePreamble.getCommand<STATE_SIP>();
    ASSERT_NE(nullptr, stateSipCmd);
    EXPECT_EQ(BuiltIns::getInstance().getSipKernel(SipKernelType::Csr, *device).getSipAllocation()->getGpuAddressToPatch(), stateSipCmd->getSystemInstructionPointer());
}

GEN9TEST_F(Gen9ThreadGroupPreemptionEnqueueKernelTest, givenSecondEnqueueWithTheSamePreemptionRequestThenDontReprogramThreadGroupNoWa) {
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
    WhitelistedRegisters regs = {};
    regs.csChicken1_0x2580 = true;
    pDevice->setForceWhitelistedRegs(true, &regs);
    const_cast<WorkaroundTable *>(pDevice->getWaTable())->waModifyVFEStateAfterGPGPUPreemption = false;
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.getMemoryManager()->setForce32BitAllocations(false);
    csr.overrideMediaVFEStateDirty(false);
    auto csrSurface = csr.getPreemptionCsrAllocation();
    EXPECT_EQ(nullptr, csrSurface);
    size_t off[3] = {0, 0, 0};
    size_t gws[3] = {1, 1, 1};

    MockKernelWithInternals mockKernel(*pDevice);

    HardwareParse hwParserCsr;
    HardwareParse hwParserCmdQ;
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    hwParserCsr.parseCommands<FamilyType>(csr.commandStream);
    hwParserCmdQ.parseCommands<FamilyType>(pCmdQ->getCS());
    auto offsetCsr = csr.commandStream.getUsed();
    auto offsetCmdQ = pCmdQ->getCS().getUsed();
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->flush();
    hwParserCsr.parseCommands<FamilyType>(csr.commandStream, offsetCsr);
    hwParserCmdQ.parseCommands<FamilyType>(pCmdQ->getCS(), offsetCmdQ);

    EXPECT_EQ(1U, countMmio<FamilyType>(hwParserCsr.cmdList.begin(), hwParserCsr.cmdList.end(), 0x2580u));
    EXPECT_EQ(0U, countMmio<FamilyType>(hwParserCsr.cmdList.begin(), hwParserCsr.cmdList.end(), 0x2600u));
    EXPECT_EQ(0U, countMmio<FamilyType>(hwParserCmdQ.cmdList.begin(), hwParserCmdQ.cmdList.end(), 0x2580u));
    EXPECT_EQ(0U, countMmio<FamilyType>(hwParserCmdQ.cmdList.begin(), hwParserCmdQ.cmdList.end(), 0x2600u));
}

GEN9TEST_F(Gen9ThreadGroupPreemptionEnqueueKernelTest, givenSecondEnqueueWithTheSamePreemptionRequestThenDontReprogramThreadGroupWa) {
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
    WhitelistedRegisters regs = {};
    regs.csChicken1_0x2580 = true;
    pDevice->setForceWhitelistedRegs(true, &regs);
    const_cast<WorkaroundTable *>(pDevice->getWaTable())->waModifyVFEStateAfterGPGPUPreemption = true;
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.getMemoryManager()->setForce32BitAllocations(false);
    csr.overrideMediaVFEStateDirty(false);
    auto csrSurface = csr.getPreemptionCsrAllocation();
    EXPECT_EQ(nullptr, csrSurface);
    HardwareParse hwCsrParser;
    HardwareParse hwCmdQParser;
    size_t off[3] = {0, 0, 0};
    size_t gws[3] = {1, 1, 1};

    MockKernelWithInternals mockKernel(*pDevice);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    hwCsrParser.parseCommands<FamilyType>(csr.commandStream);
    hwCsrParser.findHardwareCommands<FamilyType>();
    hwCmdQParser.parseCommands<FamilyType>(pCmdQ->getCS());
    hwCmdQParser.findHardwareCommands<FamilyType>();
    auto offsetCsr = csr.commandStream.getUsed();
    auto offsetCmdQ = pCmdQ->getCS().getUsed();

    bool foundOne = false;
    for (auto it : hwCsrParser.lriList) {
        auto cmd = genCmdCast<typename FamilyType::MI_LOAD_REGISTER_IMM *>(it);
        if (cmd->getRegisterOffset() == 0x2580u) {
            EXPECT_FALSE(foundOne);
            foundOne = true;
        }
    }
    EXPECT_TRUE(foundOne);
    hwCsrParser.cmdList.clear();
    hwCsrParser.lriList.clear();

    int foundWaLri = 0;
    int foundWaLriBegin = 0;
    int foundWaLriEnd = 0;
    for (auto it : hwCmdQParser.lriList) {
        auto cmd = genCmdCast<typename FamilyType::MI_LOAD_REGISTER_IMM *>(it);
        if (cmd->getRegisterOffset() == 0x2600u) {
            foundWaLri++;
            if (cmd->getDataDword() == 0xFFFFFFFF) {
                foundWaLriBegin++;
            }
            if (cmd->getDataDword() == 0x0) {
                foundWaLriEnd++;
            }
        }
    }
    EXPECT_EQ(2, foundWaLri);
    EXPECT_EQ(1, foundWaLriBegin);
    EXPECT_EQ(1, foundWaLriEnd);
    hwCmdQParser.cmdList.clear();
    hwCmdQParser.lriList.clear();

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    hwCsrParser.parseCommands<FamilyType>(csr.commandStream, offsetCsr);
    hwCsrParser.findHardwareCommands<FamilyType>();

    hwCmdQParser.parseCommands<FamilyType>(pCmdQ->getCS(), offsetCmdQ);
    hwCmdQParser.findHardwareCommands<FamilyType>();

    for (auto it : hwCsrParser.lriList) {
        auto cmd = genCmdCast<typename FamilyType::MI_LOAD_REGISTER_IMM *>(it);
        EXPECT_FALSE(cmd->getRegisterOffset() == 0x2580u);
    }

    foundWaLri = 0;
    foundWaLriBegin = 0;
    foundWaLriEnd = 0;
    for (auto it : hwCmdQParser.lriList) {
        auto cmd = genCmdCast<typename FamilyType::MI_LOAD_REGISTER_IMM *>(it);
        if (cmd->getRegisterOffset() == 0x2600u) {
            foundWaLri++;
            if (cmd->getDataDword() == 0xFFFFFFFF) {
                foundWaLriBegin++;
            }
            if (cmd->getDataDword() == 0x0) {
                foundWaLriEnd++;
            }
        }
    }
    EXPECT_EQ(2, foundWaLri);
    EXPECT_EQ(1, foundWaLriBegin);
    EXPECT_EQ(1, foundWaLriEnd);
}

GEN9TEST_F(Gen9PreemptionEnqueueKernelTest, givenValidKernelForPreemptionWhenEnqueueKernelCalledThenPassDevicePreemptionModeThreadGroup) {
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
    WhitelistedRegisters regs = {};
    regs.csChicken1_0x2580 = true;
    pDevice->setForceWhitelistedRegs(true, &regs);
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo());
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pDevice);
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(*pDevice, mockKernel.mockKernel));

    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->flush();

    EXPECT_EQ(1, mockCsr->flushCalledCount);
    EXPECT_EQ(PreemptionMode::ThreadGroup, mockCsr->passedDispatchFlags.preemptionMode);
}

GEN9TEST_F(Gen9PreemptionEnqueueKernelTest, givenValidKernelForPreemptionWhenEnqueueKernelCalledAndBlockedThenPassDevicePreemptionModeThreadGroup) {
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
    WhitelistedRegisters regs = {};
    regs.csChicken1_0x2580 = true;
    pDevice->setForceWhitelistedRegs(true, &regs);
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo());
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pDevice);
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(*pDevice, mockKernel.mockKernel));

    UserEvent userEventObj;
    cl_event userEvent = &userEventObj;
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 1, &userEvent, nullptr);
    pCmdQ->flush();
    EXPECT_EQ(0, mockCsr->flushCalledCount);

    userEventObj.setStatus(CL_COMPLETE);
    pCmdQ->flush();
    EXPECT_EQ(1, mockCsr->flushCalledCount);
    EXPECT_EQ(PreemptionMode::ThreadGroup, mockCsr->passedDispatchFlags.preemptionMode);
}

GEN9TEST_F(Gen9MidThreadPreemptionEnqueueKernelTest, givenSecondEnqueueWithTheSamePreemptionRequestThenDontReprogramMidThreadNoWa) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef typename FamilyType::GPGPU_CSR_BASE_ADDRESS GPGPU_CSR_BASE_ADDRESS;

    WhitelistedRegisters regs = {};
    regs.csChicken1_0x2580 = true;
    pDevice->setForceWhitelistedRegs(true, &regs);
    const_cast<WorkaroundTable *>(pDevice->getWaTable())->waModifyVFEStateAfterGPGPUPreemption = false;

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.getMemoryManager()->setForce32BitAllocations(false);
    csr.overrideMediaVFEStateDirty(false);
    auto csrSurface = csr.getPreemptionCsrAllocation();
    ASSERT_NE(nullptr, csrSurface);
    HardwareParse hwCsrParser;
    HardwareParse hwCmdQParser;
    size_t off[3] = {0, 0, 0};
    size_t gws[3] = {1, 1, 1};

    MockKernelWithInternals mockKernel(*pDevice);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    hwCsrParser.parseCommands<FamilyType>(csr.commandStream);
    hwCsrParser.findHardwareCommands<FamilyType>();
    hwCmdQParser.parseCommands<FamilyType>(pCmdQ->getCS());
    hwCmdQParser.findHardwareCommands<FamilyType>();
    auto offsetCsr = csr.commandStream.getUsed();
    auto offsetCmdQ = pCmdQ->getCS().getUsed();

    bool foundOneLri = false;
    for (auto it : hwCsrParser.lriList) {
        auto cmdLri = genCmdCast<MI_LOAD_REGISTER_IMM *>(it);
        if (cmdLri->getRegisterOffset() == 0x2580u) {
            EXPECT_FALSE(foundOneLri);
            foundOneLri = true;
        }
    }
    EXPECT_TRUE(foundOneLri);

    bool foundWaLri = false;
    for (auto it : hwCmdQParser.lriList) {
        auto cmdLri = genCmdCast<MI_LOAD_REGISTER_IMM *>(it);
        if (cmdLri->getRegisterOffset() == 0x2600u) {
            foundWaLri = true;
        }
    }
    EXPECT_FALSE(foundWaLri);

    hwCsrParser.findCsrBaseAddress<FamilyType>();
    ASSERT_NE(nullptr, hwCsrParser.cmdGpgpuCsrBaseAddress);
    auto cmdCsr = genCmdCast<GPGPU_CSR_BASE_ADDRESS *>(hwCsrParser.cmdGpgpuCsrBaseAddress);
    ASSERT_NE(nullptr, cmdCsr);
    EXPECT_EQ(csrSurface->getGpuAddressToPatch(), cmdCsr->getGpgpuCsrBaseAddress());

    hwCsrParser.cmdList.clear();
    hwCsrParser.lriList.clear();
    hwCsrParser.cmdGpgpuCsrBaseAddress = nullptr;
    hwCmdQParser.cmdList.clear();
    hwCmdQParser.lriList.clear();

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    hwCsrParser.parseCommands<FamilyType>(csr.commandStream, offsetCsr);
    hwCsrParser.findHardwareCommands<FamilyType>();
    hwCmdQParser.parseCommands<FamilyType>(csr.commandStream, offsetCmdQ);
    hwCmdQParser.findHardwareCommands<FamilyType>();

    for (auto it : hwCsrParser.lriList) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(it);
        EXPECT_FALSE(cmd->getRegisterOffset() == 0x2580u);
    }

    hwCsrParser.findCsrBaseAddress<FamilyType>();
    EXPECT_EQ(nullptr, hwCsrParser.cmdGpgpuCsrBaseAddress);

    for (auto it : hwCmdQParser.lriList) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(it);
        EXPECT_FALSE(cmd->getRegisterOffset() == 0x2600u);
    }
}

GEN9TEST_F(Gen9MidThreadPreemptionEnqueueKernelTest, givenSecondEnqueueWithTheSamePreemptionRequestThenDontReprogramMidThreadWa) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef typename FamilyType::GPGPU_CSR_BASE_ADDRESS GPGPU_CSR_BASE_ADDRESS;

    WhitelistedRegisters regs = {};
    regs.csChicken1_0x2580 = true;
    pDevice->setForceWhitelistedRegs(true, &regs);
    const_cast<WorkaroundTable *>(pDevice->getWaTable())->waModifyVFEStateAfterGPGPUPreemption = true;

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.getMemoryManager()->setForce32BitAllocations(false);
    csr.overrideMediaVFEStateDirty(false);
    auto csrSurface = csr.getPreemptionCsrAllocation();
    ASSERT_NE(nullptr, csrSurface);
    HardwareParse hwCsrParser;
    HardwareParse hwCmdQParser;
    size_t off[3] = {0, 0, 0};
    size_t gws[3] = {1, 1, 1};

    MockKernelWithInternals mockKernel(*pDevice);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    hwCsrParser.parseCommands<FamilyType>(csr.commandStream);
    hwCsrParser.findHardwareCommands<FamilyType>();
    hwCmdQParser.parseCommands<FamilyType>(pCmdQ->getCS());
    hwCmdQParser.findHardwareCommands<FamilyType>();
    auto offsetCsr = csr.commandStream.getUsed();
    auto offsetCmdQ = pCmdQ->getCS().getUsed();

    bool foundOneLri = false;
    for (auto it : hwCsrParser.lriList) {
        auto cmdLri = genCmdCast<MI_LOAD_REGISTER_IMM *>(it);
        if (cmdLri->getRegisterOffset() == 0x2580u) {
            EXPECT_FALSE(foundOneLri);
            foundOneLri = true;
        }
    }
    EXPECT_TRUE(foundOneLri);

    int foundWaLri = 0;
    int foundWaLriBegin = 0;
    int foundWaLriEnd = 0;
    for (auto it : hwCmdQParser.lriList) {
        auto cmdLri = genCmdCast<MI_LOAD_REGISTER_IMM *>(it);
        if (cmdLri->getRegisterOffset() == 0x2600u) {
            foundWaLri++;
            if (cmdLri->getDataDword() == 0xFFFFFFFF) {
                foundWaLriBegin++;
            }
            if (cmdLri->getDataDword() == 0x0) {
                foundWaLriEnd++;
            }
        }
    }
    EXPECT_EQ(2, foundWaLri);
    EXPECT_EQ(1, foundWaLriBegin);
    EXPECT_EQ(1, foundWaLriEnd);

    hwCsrParser.findCsrBaseAddress<FamilyType>();
    ASSERT_NE(nullptr, hwCsrParser.cmdGpgpuCsrBaseAddress);
    auto cmdCsr = genCmdCast<GPGPU_CSR_BASE_ADDRESS *>(hwCsrParser.cmdGpgpuCsrBaseAddress);
    ASSERT_NE(nullptr, cmdCsr);
    EXPECT_EQ(csrSurface->getGpuAddressToPatch(), cmdCsr->getGpgpuCsrBaseAddress());

    hwCsrParser.cmdList.clear();
    hwCsrParser.lriList.clear();
    hwCsrParser.cmdGpgpuCsrBaseAddress = nullptr;
    hwCmdQParser.cmdList.clear();
    hwCmdQParser.lriList.clear();

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    hwCsrParser.parseCommands<FamilyType>(csr.commandStream, offsetCsr);
    hwCsrParser.findHardwareCommands<FamilyType>();
    hwCmdQParser.parseCommands<FamilyType>(pCmdQ->getCS(), offsetCmdQ);
    hwCmdQParser.findHardwareCommands<FamilyType>();

    for (auto it : hwCsrParser.lriList) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(it);
        EXPECT_FALSE(cmd->getRegisterOffset() == 0x2580u);
    }

    hwCsrParser.findCsrBaseAddress<FamilyType>();
    EXPECT_EQ(nullptr, hwCsrParser.cmdGpgpuCsrBaseAddress);

    foundWaLri = 0;
    foundWaLriBegin = 0;
    foundWaLriEnd = 0;
    for (auto it : hwCmdQParser.lriList) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(it);
        if (cmd->getRegisterOffset() == 0x2600u) {
            foundWaLri++;
            if (cmd->getDataDword() == 0xFFFFFFFF) {
                foundWaLriBegin++;
            }
            if (cmd->getDataDword() == 0x0) {
                foundWaLriEnd++;
            }
        }
    }
    EXPECT_EQ(2, foundWaLri);
    EXPECT_EQ(1, foundWaLriBegin);
    EXPECT_EQ(1, foundWaLriEnd);
}

GEN9TEST_F(Gen9PreemptionEnqueueKernelTest, givenDisabledPreemptionWhenEnqueueKernelCalledThenPassDisabledPreemptionMode) {
    pDevice->setPreemptionMode(PreemptionMode::Disabled);
    WhitelistedRegisters regs = {};
    pDevice->setForceWhitelistedRegs(true, &regs);
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo());
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pDevice);
    EXPECT_EQ(PreemptionMode::Disabled, PreemptionHelper::taskPreemptionMode(*pDevice, mockKernel.mockKernel));

    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->flush();

    EXPECT_EQ(1, mockCsr->flushCalledCount);
    EXPECT_EQ(PreemptionMode::Disabled, mockCsr->passedDispatchFlags.preemptionMode);
}

GEN9TEST_F(Gen9PreemptionTests, getPreemptionWaCsSizeMidBatch) {
    size_t expectedSize = 0;
    device->setPreemptionMode(PreemptionMode::MidBatch);
    size_t size = PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device);
    EXPECT_EQ(expectedSize, size);
}

GEN9TEST_F(Gen9PreemptionTests, getPreemptionWaCsSizeThreadGroupNoWa) {
    size_t expectedSize = 0;
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    const_cast<WorkaroundTable *>(device->getWaTable())->waModifyVFEStateAfterGPGPUPreemption = false;
    size_t size = PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device);
    EXPECT_EQ(expectedSize, size);
}

GEN9TEST_F(Gen9PreemptionTests, getPreemptionWaCsSizeThreadGroupWa) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    size_t expectedSize = 2 * sizeof(MI_LOAD_REGISTER_IMM);
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    const_cast<WorkaroundTable *>(device->getWaTable())->waModifyVFEStateAfterGPGPUPreemption = true;
    size_t size = PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device);
    EXPECT_EQ(expectedSize, size);
}

GEN9TEST_F(Gen9PreemptionTests, getPreemptionWaCsSizeMidThreadNoWa) {
    size_t expectedSize = 0;
    device->setPreemptionMode(PreemptionMode::MidThread);
    const_cast<WorkaroundTable *>(device->getWaTable())->waModifyVFEStateAfterGPGPUPreemption = false;
    size_t size = PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device);
    EXPECT_EQ(expectedSize, size);
}

GEN9TEST_F(Gen9PreemptionTests, getPreemptionWaCsSizeMidThreadWa) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    size_t expectedSize = 2 * sizeof(MI_LOAD_REGISTER_IMM);
    device->setPreemptionMode(PreemptionMode::MidThread);
    const_cast<WorkaroundTable *>(device->getWaTable())->waModifyVFEStateAfterGPGPUPreemption = true;
    size_t size = PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device);
    EXPECT_EQ(expectedSize, size);
}

GEN9TEST_F(Gen9PreemptionTests, givenInterfaceDescriptorDataWhenAnyPreemptionModeThenNoChange) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    INTERFACE_DESCRIPTOR_DATA idd;
    INTERFACE_DESCRIPTOR_DATA iddArg;
    int ret;

    idd = FamilyType::cmdInitInterfaceDescriptorData;
    iddArg = FamilyType::cmdInitInterfaceDescriptorData;

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::Disabled);
    ret = memcmp(&idd, &iddArg, sizeof(INTERFACE_DESCRIPTOR_DATA));
    EXPECT_EQ(0, ret);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::MidBatch);
    ret = memcmp(&idd, &iddArg, sizeof(INTERFACE_DESCRIPTOR_DATA));
    EXPECT_EQ(0, ret);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::ThreadGroup);
    ret = memcmp(&idd, &iddArg, sizeof(INTERFACE_DESCRIPTOR_DATA));
    EXPECT_EQ(0, ret);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::MidThread);
    ret = memcmp(&idd, &iddArg, sizeof(INTERFACE_DESCRIPTOR_DATA));
    EXPECT_EQ(0, ret);
}
