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

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/command_stream_receiver_with_aub_dump.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/helpers/options.h"
#include "unit_tests/libult/create_command_stream.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "test.h"

using namespace OCLRT;

struct CreateCommandStreamReceiverTest : public ::testing::TestWithParam<CommandStreamReceiverType> {
    void SetUp() override {
        storeInitHWTag = initialHardwareTag;
    }

    void TearDown() override {
        initialHardwareTag = storeInitHWTag;
    }

  protected:
    int storeInitHWTag;
};

HWTEST_P(CreateCommandStreamReceiverTest, givenCreateCommandStreamWhenCsrIsSetToValidTypeThenTheFuntionReturnsCommandStreamReceiver) {
    DebugManagerStateRestore stateRestorer;
    const HardwareInfo hwInfo = *platformDevices[0];
    CommandStreamReceiverType csrType = GetParam();
    MemoryManager *memoryManager = nullptr;

    overrideCommandStreamReceiverCreation = true;
    DebugManager.flags.SetCommandStreamReceiver.set(csrType);
    auto csr = createCommandStream(&hwInfo);
    if (csrType < CommandStreamReceiverType::CSR_TYPES_NUM) {
        EXPECT_NE(nullptr, csr);
        memoryManager = csr->createMemoryManager(false);
        EXPECT_NE(nullptr, memoryManager);
    } else {
        EXPECT_EQ(nullptr, csr);
        EXPECT_EQ(nullptr, memoryManager);
    }
    delete csr;
    delete memoryManager;
}

static CommandStreamReceiverType commandStreamReceiverTypes[] = {
    CSR_HW,
    CSR_AUB,
    CSR_TBX,
    CSR_HW_WITH_AUB,
    CSR_TBX_WITH_AUB,
    CSR_TYPES_NUM};

INSTANTIATE_TEST_CASE_P(
    CreateCommandStreamReceiverTest_Create,
    CreateCommandStreamReceiverTest,
    testing::ValuesIn(commandStreamReceiverTypes));