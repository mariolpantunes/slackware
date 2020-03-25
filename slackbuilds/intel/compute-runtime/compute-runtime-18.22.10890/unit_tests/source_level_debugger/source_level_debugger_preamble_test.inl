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

using namespace OCLRT;

template <typename GfxFamily>
void SourceLevelDebuggerPreambleTest<GfxFamily>::givenMidThreadPreemptionAndDebuggingActiveWhenPreambleIsPrograamedThenCorrectSipKernelIsUsedTest() {
    using STATE_SIP = typename GfxFamily::STATE_SIP;
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::create<MockDevice>(nullptr));

    mockDevice->setSourceLevelDebuggerActive(true);
    mockDevice->setPreemptionMode(PreemptionMode::MidThread);
    auto cmdSizePreemptionMidThread = PreemptionHelper::getRequiredPreambleSize<GfxFamily>(*mockDevice);

    StackVec<char, 4096> preambleBuffer;
    preambleBuffer.resize(cmdSizePreemptionMidThread);
    LinearStream preambleStream(&*preambleBuffer.begin(), preambleBuffer.size());

    uintptr_t minCsrAlignment = 2 * 256 * MemoryConstants::kiloByte;
    MockGraphicsAllocation csrSurface(reinterpret_cast<void *>(minCsrAlignment), 1024);

    PreemptionHelper::programPreamble<GfxFamily>(preambleStream, *mockDevice, &csrSurface);

    HardwareParse hwParser;
    hwParser.parseCommands<GfxFamily>(preambleStream);
    auto itorStateSip = find<STATE_SIP *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorStateSip);
    STATE_SIP *stateSipCmd = (STATE_SIP *)*itorStateSip;
    auto sipAddress = stateSipCmd->getSystemInstructionPointer();

    auto sipType = SipKernel::getSipKernelType(mockDevice->getHardwareInfo().pPlatform->eRenderCoreFamily, mockDevice->isSourceLevelDebuggerActive());
    EXPECT_EQ(BuiltIns::getInstance().getSipKernel(sipType, *mockDevice).getSipAllocation()->getGpuAddressToPatch(), sipAddress);
}

template <typename GfxFamily>
void SourceLevelDebuggerPreambleTest<GfxFamily>::givenMidThreadPreemptionAndDisabledDebuggingWhenPreambleIsPrograamedThenCorrectSipKernelIsUsedTest() {
    using STATE_SIP = typename GfxFamily::STATE_SIP;
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::create<MockDevice>(nullptr));

    mockDevice->setSourceLevelDebuggerActive(false);
    mockDevice->setPreemptionMode(PreemptionMode::MidThread);
    auto cmdSizePreemptionMidThread = PreemptionHelper::getRequiredPreambleSize<GfxFamily>(*mockDevice);

    StackVec<char, 4096> preambleBuffer;
    preambleBuffer.resize(cmdSizePreemptionMidThread);
    LinearStream preambleStream(&*preambleBuffer.begin(), preambleBuffer.size());

    uintptr_t minCsrAlignment = 2 * 256 * MemoryConstants::kiloByte;
    MockGraphicsAllocation csrSurface(reinterpret_cast<void *>(minCsrAlignment), 1024);

    PreemptionHelper::programPreamble<GfxFamily>(preambleStream, *mockDevice, &csrSurface);

    HardwareParse hwParser;
    hwParser.parseCommands<GfxFamily>(preambleStream);
    auto itorStateSip = find<STATE_SIP *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorStateSip);
    STATE_SIP *stateSipCmd = (STATE_SIP *)*itorStateSip;
    auto sipAddress = stateSipCmd->getSystemInstructionPointer();

    auto sipType = SipKernel::getSipKernelType(mockDevice->getHardwareInfo().pPlatform->eRenderCoreFamily, mockDevice->isSourceLevelDebuggerActive());
    EXPECT_EQ(BuiltIns::getInstance().getSipKernel(sipType, *mockDevice).getSipAllocation()->getGpuAddressToPatch(), sipAddress);
}

template <typename GfxFamily>
void SourceLevelDebuggerPreambleTest<GfxFamily>::givenPreemptionDisabledAndDebuggingActiveWhenPreambleIsProgrammedThenCorrectSipKernelIsUsedTest() {
    using STATE_SIP = typename GfxFamily::STATE_SIP;
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::create<MockDevice>(nullptr));

    mockDevice->setSourceLevelDebuggerActive(true);
    mockDevice->setPreemptionMode(PreemptionMode::Disabled);
    auto cmdSizePreemptionMidThread = PreemptionHelper::getRequiredPreambleSize<GfxFamily>(*mockDevice);

    StackVec<char, 4096> preambleBuffer;
    preambleBuffer.resize(cmdSizePreemptionMidThread);
    LinearStream preambleStream(&*preambleBuffer.begin(), preambleBuffer.size());

    uintptr_t minCsrAlignment = 2 * 256 * MemoryConstants::kiloByte;
    MockGraphicsAllocation csrSurface(reinterpret_cast<void *>(minCsrAlignment), 1024);

    PreemptionHelper::programPreamble<GfxFamily>(preambleStream, *mockDevice, &csrSurface);

    HardwareParse hwParser;
    hwParser.parseCommands<GfxFamily>(preambleStream);
    auto itorStateSip = find<STATE_SIP *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorStateSip);
    STATE_SIP *stateSipCmd = (STATE_SIP *)*itorStateSip;
    auto sipAddress = stateSipCmd->getSystemInstructionPointer();

    auto sipType = SipKernel::getSipKernelType(mockDevice->getHardwareInfo().pPlatform->eRenderCoreFamily, mockDevice->isSourceLevelDebuggerActive());
    EXPECT_EQ(BuiltIns::getInstance().getSipKernel(sipType, *mockDevice).getSipAllocation()->getGpuAddressToPatch(), sipAddress);
}

template <typename GfxFamily>
void SourceLevelDebuggerPreambleTest<GfxFamily>::givenMidThreadPreemptionAndDebuggingActiveWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturnedTest() {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::create<MockDevice>(nullptr));
    mockDevice->setSourceLevelDebuggerActive(true);
    mockDevice->setPreemptionMode(PreemptionMode::MidThread);
    size_t requiredPreambleSize = PreemptionHelper::getRequiredPreambleSize<GfxFamily>(*mockDevice);
    auto sizeExpected = sizeof(typename GfxFamily::GPGPU_CSR_BASE_ADDRESS) + sizeof(typename GfxFamily::STATE_SIP);
    EXPECT_EQ(sizeExpected, requiredPreambleSize);
}

template <typename GfxFamily>
void SourceLevelDebuggerPreambleTest<GfxFamily>::givenPreemptionDisabledAndDebuggingActiveWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturnedTest() {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::create<MockDevice>(nullptr));
    mockDevice->setSourceLevelDebuggerActive(true);
    mockDevice->setPreemptionMode(PreemptionMode::Disabled);
    size_t requiredPreambleSize = PreemptionHelper::getRequiredPreambleSize<GfxFamily>(*mockDevice);
    auto sizeExpected = sizeof(typename GfxFamily::STATE_SIP);
    EXPECT_EQ(sizeExpected, requiredPreambleSize);
}

template <typename GfxFamily>
void SourceLevelDebuggerPreambleTest<GfxFamily>::givenMidThreadPreemptionAndDisabledDebuggingWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturnedTest() {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::create<MockDevice>(nullptr));
    mockDevice->setSourceLevelDebuggerActive(false);
    mockDevice->setPreemptionMode(PreemptionMode::MidThread);
    size_t requiredPreambleSize = PreemptionHelper::getRequiredPreambleSize<GfxFamily>(*mockDevice);
    auto sizeExpected = sizeof(typename GfxFamily::GPGPU_CSR_BASE_ADDRESS) + sizeof(typename GfxFamily::STATE_SIP);
    EXPECT_EQ(sizeExpected, requiredPreambleSize);
}

template <typename GfxFamily>
void SourceLevelDebuggerPreambleTest<GfxFamily>::givenDisabledPreemptionAndDisabledDebuggingWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturnedTest() {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::create<MockDevice>(nullptr));
    mockDevice->setSourceLevelDebuggerActive(false);
    mockDevice->setPreemptionMode(PreemptionMode::Disabled);
    size_t requiredPreambleSize = PreemptionHelper::getRequiredPreambleSize<GfxFamily>(*mockDevice);
    size_t sizeExpected = 0u;
    EXPECT_EQ(sizeExpected, requiredPreambleSize);
}

template class SourceLevelDebuggerPreambleTest<GfxFamily>;
