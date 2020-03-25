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

#include "runtime/context/context.h"
#include "runtime/command_queue/gpgpu_walker.h"
#include "unit_tests/fixtures/device_host_queue_fixture.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_device_queue.h"

using namespace OCLRT;
using namespace DeviceHostQueue;

typedef DeviceQueueHwTest Gen9DeviceQueueSlb;

GEN9TEST_F(Gen9DeviceQueueSlb, expectedAllocationSize) {
    deviceQueue = createQueueObject();
    ASSERT_NE(deviceQueue, nullptr);

    auto expectedSize = getMinimumSlbSize<FamilyType>() +
                        sizeof(typename FamilyType::MI_ARB_CHECK) +
                        sizeof(typename FamilyType::MI_ATOMIC) +
                        sizeof(typename FamilyType::MI_ARB_CHECK) +
                        sizeof(typename FamilyType::PIPE_CONTROL) +
                        sizeof(typename FamilyType::PIPE_CONTROL);
    expectedSize *= 128; //num of enqueues
    expectedSize += sizeof(typename FamilyType::MI_BATCH_BUFFER_START);
    expectedSize = alignUp(expectedSize, MemoryConstants::pageSize);
    expectedSize += MockDeviceQueueHw<FamilyType>::getExecutionModelCleanupSectionSize();
    expectedSize += (4 * MemoryConstants::pageSize);
    expectedSize = alignUp(expectedSize, MemoryConstants::pageSize);

    ASSERT_NE(deviceQueue->getSlbBuffer(), nullptr);
    EXPECT_EQ(deviceQueue->getSlbBuffer()->getUnderlyingBufferSize(), expectedSize);

    delete deviceQueue;
}

GEN9TEST_F(Gen9DeviceQueueSlb, SlbCommandsWa) {
    auto mockDeviceQueueHw = new MockDeviceQueueHw<FamilyType>(pContext, device,
                                                               DeviceHostQueue::deviceQueueProperties::minimumProperties[0]);
    EXPECT_TRUE(mockDeviceQueueHw->arbCheckWa);
    EXPECT_TRUE(mockDeviceQueueHw->pipeControlWa);
    EXPECT_TRUE(mockDeviceQueueHw->miAtomicWa);
    EXPECT_FALSE(mockDeviceQueueHw->lriWa);

    delete mockDeviceQueueHw;
}

GEN9TEST_F(Gen9DeviceQueueSlb, addProfilingEndcmds) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    auto mockDeviceQueueHw = new MockDeviceQueueHw<FamilyType>(pContext, device,
                                                               DeviceHostQueue::deviceQueueProperties::minimumProperties[0]);

    uint64_t timestampAddress = 0x12345678555500;

    mockDeviceQueueHw->addProfilingEndCmds(timestampAddress);

    HardwareParse hwParser;
    auto *slbCS = mockDeviceQueueHw->getSlbCS();

    hwParser.parseCommands<FamilyType>(*slbCS, 0);

    auto pipeControlItor = find<PIPE_CONTROL *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), pipeControlItor);

    PIPE_CONTROL *pipeControl = (PIPE_CONTROL *)*pipeControlItor;

    uint32_t postSyncOp = (uint32_t)PIPE_CONTROL::POST_SYNC_OPERATION_NO_WRITE;
    EXPECT_EQ(postSyncOp, (uint32_t)pipeControl->getPostSyncOperation());

    EXPECT_NE(0u, (uint32_t)pipeControl->getCommandStreamerStallEnable());

    auto storeRegMemItor = find<MI_STORE_REGISTER_MEM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());

    ASSERT_NE(hwParser.cmdList.end(), storeRegMemItor);

    MI_STORE_REGISTER_MEM *pMICmdLow = (MI_STORE_REGISTER_MEM *)*storeRegMemItor;

    EXPECT_EQ(GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, pMICmdLow->getRegisterAddress());
    EXPECT_EQ(timestampAddress, pMICmdLow->getMemoryAddress());

    storeRegMemItor++;
    EXPECT_EQ(hwParser.cmdList.end(), storeRegMemItor);

    delete mockDeviceQueueHw;
}
