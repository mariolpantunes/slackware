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

#include "aub_command_stream_fixture.h"
#include "runtime/command_stream/aub_command_stream_receiver_hw.h"
#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/helpers/ptr_math.h"
#include "gen_cmd_parse.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "test.h"
#include <cstdint>

using namespace OCLRT;

struct AUBFixture : public AUBCommandStreamFixture,
                    public CommandQueueFixture,
                    public DeviceFixture {

    using AUBCommandStreamFixture::SetUp;
    using CommandQueueFixture::SetUp;

    void SetUp() {
        DeviceFixture::SetUp();
        CommandQueueFixture::SetUp(nullptr, pDevice, 0);
        AUBCommandStreamFixture::SetUp(pCmdQ);
    }

    void TearDown() override {
        AUBCommandStreamFixture::TearDown();
        CommandQueueFixture::TearDown();
        DeviceFixture::TearDown();
    }

    template <typename FamilyType>
    void testNoopIdXcs(EngineType engineType) {
        typedef typename FamilyType::MI_NOOP MI_NOOP;

        auto pCmd = (MI_NOOP *)pCS->getSpace(sizeof(MI_NOOP) * 4);

        uint32_t noopId = 0xbaadd;
        auto noop = MI_NOOP::sInit();
        *pCmd++ = noop;
        *pCmd++ = noop;
        *pCmd++ = noop;
        noop.TheStructure.Common.IdentificationNumberRegisterWriteEnable = true;
        noop.TheStructure.Common.IdentificationNumber = noopId;
        *pCmd++ = noop;

        CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(*pCS, nullptr);
        CommandStreamReceiverHw<FamilyType>::alignToCacheLine(*pCS);
        BatchBuffer batchBuffer{pCS->getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, pCS->getUsed(), pCS};
        ResidencyContainer allocationsForResidency;
        pCommandStreamReceiver->flush(batchBuffer, engineType, &allocationsForResidency);

        auto mmioBase = AUBCommandStreamReceiverHw<FamilyType>::getCsTraits(engineType).mmioBase;
        AUBCommandStreamFixture::expectMMIO<FamilyType>(mmioBase + 0x2094, noopId);
    }
};

typedef Test<AUBFixture> AUBcommandstreamTests;

HWTEST_F(AUBcommandstreamTests, testFlushTwice) {
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(*pCS, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(*pCS);
    BatchBuffer batchBuffer{pCS->getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, pCS->getUsed(), pCS};
    ResidencyContainer allocationsForResidency;
    pCommandStreamReceiver->flush(batchBuffer, EngineType::ENGINE_RCS, &allocationsForResidency);
    BatchBuffer batchBuffer2{pCS->getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, pCS->getUsed(), pCS};
    ResidencyContainer allocationsForResidency2;
    pCommandStreamReceiver->flush(batchBuffer2, EngineType::ENGINE_RCS, &allocationsForResidency);
}

HWTEST_F(AUBcommandstreamTests, testNoopIdRcs) {
    testNoopIdXcs<FamilyType>(EngineType::ENGINE_RCS);
}

HWTEST_F(AUBcommandstreamTests, testNoopIdBcs) {
    testNoopIdXcs<FamilyType>(EngineType::ENGINE_BCS);
}

HWTEST_F(AUBcommandstreamTests, testNoopIdVcs) {
    testNoopIdXcs<FamilyType>(EngineType::ENGINE_VCS);
}

HWTEST_F(AUBcommandstreamTests, testNoopIdVecs) {
    testNoopIdXcs<FamilyType>(EngineType::ENGINE_VECS);
}

TEST_F(AUBcommandstreamTests, makeResident) {
    uint8_t buffer[0x10000];
    size_t size = sizeof(buffer);
    auto &commandStreamReceiver = pDevice->getCommandStreamReceiver();
    auto graphicsAllocation = commandStreamReceiver.createAllocationAndHandleResidency(buffer, size);
    ResidencyContainer allocationsForResidency = {graphicsAllocation};
    commandStreamReceiver.processResidency(&allocationsForResidency);
}

HWTEST_F(AUBcommandstreamTests, expectMemorySingle) {
    uint32_t buffer = 0xdeadbeef;
    size_t size = sizeof(buffer);
    auto &commandStreamReceiver = pDevice->getCommandStreamReceiver();
    auto graphicsAllocation = commandStreamReceiver.createAllocationAndHandleResidency(&buffer, size);
    ResidencyContainer allocationsForResidency = {graphicsAllocation};
    commandStreamReceiver.processResidency(&allocationsForResidency);

    AUBCommandStreamFixture::expectMemory<FamilyType>(&buffer, &buffer, size);
}

HWTEST_F(AUBcommandstreamTests, expectMemoryLarge) {
    size_t sizeBuffer = 0x100001;
    auto buffer = new uint8_t[sizeBuffer];

    for (size_t index = 0; index < sizeBuffer; ++index) {
        buffer[index] = static_cast<uint8_t>(index);
    }

    auto &commandStreamReceiver = pDevice->getCommandStreamReceiver();
    auto graphicsAllocation = commandStreamReceiver.createAllocationAndHandleResidency(buffer, sizeBuffer);
    ResidencyContainer allocationsForResidency = {graphicsAllocation};
    commandStreamReceiver.processResidency(&allocationsForResidency);

    AUBCommandStreamFixture::expectMemory<FamilyType>(buffer, buffer, sizeBuffer);
    delete[] buffer;
}
