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

#include "runtime/mem_obj/mem_obj.h"
#include "tbx_command_stream_fixture.h"
#include "runtime/command_stream/tbx_command_stream_receiver_hw.h"
#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/helpers/ptr_math.h"
#include "gen_cmd_parse.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "test.h"
#include <cstdint>

using namespace OCLRT;
namespace Os {
extern const char *tbxLibName;
}

struct TbxFixture : public TbxCommandStreamFixture,
                    public DeviceFixture {

    using TbxCommandStreamFixture::SetUp;

    void SetUp() {
        DeviceFixture::SetUp();
        TbxCommandStreamFixture::SetUp(pDevice);
    }

    void TearDown() override {
        TbxCommandStreamFixture::TearDown();
        DeviceFixture::TearDown();
    }
};

typedef Test<TbxFixture> TbxCommandStreamTests;

TEST_F(TbxCommandStreamTests, DISABLED_testFactory) {
}

HWTEST_F(TbxCommandStreamTests, DISABLED_testTbxMemoryManager) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    TbxMemoryManager *getMM = tbxCsr->getMemoryManager();
    EXPECT_NE(nullptr, getMM);
    EXPECT_EQ(1 * GB, getMM->getSystemSharedMemory());
}

TEST_F(TbxCommandStreamTests, DISABLED_makeResident) {
    uint8_t buffer[0x10000];
    size_t size = sizeof(buffer);

    GraphicsAllocation *graphicsAllocation = mmTbx->allocateGraphicsMemory(size, buffer);
    pCommandStreamReceiver->makeResident(*graphicsAllocation);
    pCommandStreamReceiver->makeNonResident(*graphicsAllocation);
    mmTbx->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(TbxCommandStreamTests, DISABLED_makeResidentOnZeroSizedBufferShouldDoNothing) {
    GraphicsAllocation graphicsAllocation(nullptr, 0);

    pCommandStreamReceiver->makeResident(graphicsAllocation);
    pCommandStreamReceiver->makeNonResident(graphicsAllocation);
}

TEST_F(TbxCommandStreamTests, DISABLED_flush) {
    char buffer[4096];
    memset(buffer, 0, 4096);
    LinearStream cs(buffer, 4096);
    size_t startOffset = 0;
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), startOffset, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    pCommandStreamReceiver->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr);
}

HWTEST_F(TbxCommandStreamTests, DISABLED_flushUntilTailRCSLargerThanSizeRCS) {
    char buffer[4096];
    memset(buffer, 0, 4096);
    LinearStream cs(buffer, 4096);
    size_t startOffset = 0;
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    auto &engineInfo = tbxCsr->engineInfoTable[EngineType::ENGINE_RCS];

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), startOffset, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    pCommandStreamReceiver->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr);
    auto size = engineInfo.sizeRCS;
    engineInfo.sizeRCS = 64;
    pCommandStreamReceiver->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr);
    pCommandStreamReceiver->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr);
    pCommandStreamReceiver->flush(batchBuffer, EngineType::ENGINE_RCS, nullptr);
    engineInfo.sizeRCS = size;
}

HWTEST_F(TbxCommandStreamTests, DISABLED_getCsTraits) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    tbxCsr->getCsTraits(EngineType::ENGINE_RCS);
    tbxCsr->getCsTraits(EngineType::ENGINE_BCS);
    tbxCsr->getCsTraits(EngineType::ENGINE_VCS);
    tbxCsr->getCsTraits(EngineType::ENGINE_VECS);
}

#if defined(__linux__)
TEST(TbxCommandStreamReceiverTest, DISABLED_createShouldReturnFunctionPointer) {
    TbxCommandStreamReceiver tbx;
    const HardwareInfo *hwInfo = platformDevices[0];
    CommandStreamReceiver *csr = tbx.create(*hwInfo, false);
    EXPECT_NE(nullptr, csr);
    delete csr;
}

namespace OCLRT {
TEST(TbxCommandStreamReceiverTest, createShouldReturnNullptrForEmptyEntryInFactory) {
    extern TbxCommandStreamReceiverCreateFunc tbxCommandStreamReceiverFactory[IGFX_MAX_PRODUCT];

    TbxCommandStreamReceiver tbx;
    const HardwareInfo *hwInfo = platformDevices[0];
    GFXCORE_FAMILY family = hwInfo->pPlatform->eRenderCoreFamily;
    auto pCreate = tbxCommandStreamReceiverFactory[family];

    tbxCommandStreamReceiverFactory[family] = nullptr;
    CommandStreamReceiver *csr = tbx.create(*hwInfo, false);
    EXPECT_EQ(nullptr, csr);

    tbxCommandStreamReceiverFactory[family] = pCreate;
}
} // namespace OCLRT
#endif

TEST(TbxCommandStreamReceiverTest, givenTbxCommandStreamReceiverWhenItIsCreatedWithWrongGfxCoreFamilyThenNullPointerShouldBeReturned) {
    HardwareInfo hwInfo = *platformDevices[0];
    GFXCORE_FAMILY family = hwInfo.pPlatform->eRenderCoreFamily;

    const_cast<PLATFORM *>(hwInfo.pPlatform)->eRenderCoreFamily = GFXCORE_FAMILY_FORCE_ULONG; // wrong gfx core family

    CommandStreamReceiver *csr = TbxCommandStreamReceiver::create(hwInfo, false);
    EXPECT_EQ(nullptr, csr);

    const_cast<PLATFORM *>(hwInfo.pPlatform)->eRenderCoreFamily = family;
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenMakeResidentIsCalledForGraphicsAllocationThenItShouldPushAllocationForResidencyToMemoryManager) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    TbxMemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemory(4096, MemoryConstants::pageSize);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_EQ(0u, memoryManager->getResidencyAllocations().size());

    tbxCsr->makeResident(*graphicsAllocation);

    EXPECT_EQ(1u, memoryManager->getResidencyAllocations().size());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenMakeResidentHasAlreadyBeenCalledForGraphicsAllocationThenItShouldNotPushAllocationForResidencyAgainToMemoryManager) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    TbxMemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemory(4096, MemoryConstants::pageSize);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_EQ(0u, memoryManager->getResidencyAllocations().size());

    tbxCsr->makeResident(*graphicsAllocation);

    EXPECT_EQ(1u, memoryManager->getResidencyAllocations().size());

    tbxCsr->makeResident(*graphicsAllocation);

    EXPECT_EQ(1u, memoryManager->getResidencyAllocations().size());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenWriteMemoryIsCalledForGraphicsAllocationWithNonZeroSizeThenItShouldReturnTrue) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    TbxMemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemory(4096, MemoryConstants::pageSize);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_TRUE(tbxCsr->writeMemory(*graphicsAllocation));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenWriteMemoryIsCalledForGraphicsAllocationWithZeroSizeThenItShouldReturnFalse) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    GraphicsAllocation graphicsAllocation((void *)0x1234, 0);

    EXPECT_FALSE(tbxCsr->writeMemory(graphicsAllocation));
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenProcessResidencyIsCalledWithoutAllocationsForResidencyThenItShouldProcessAllocationsFromMemoryManager) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    TbxMemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemory(4096, MemoryConstants::pageSize);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_EQ(ObjectNotResident, graphicsAllocation->residencyTaskCount);

    memoryManager->pushAllocationForResidency(graphicsAllocation);
    tbxCsr->processResidency(nullptr);

    EXPECT_NE(ObjectNotResident, graphicsAllocation->residencyTaskCount);
    EXPECT_EQ((int)tbxCsr->peekTaskCount() + 1, graphicsAllocation->residencyTaskCount);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenProcessResidencyIsCalledWithAllocationsForResidencyThenItShouldProcessGivenAllocations) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    TbxMemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemory(4096, MemoryConstants::pageSize);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_EQ(ObjectNotResident, graphicsAllocation->residencyTaskCount);

    ResidencyContainer allocationsForResidency = {graphicsAllocation};
    tbxCsr->processResidency(&allocationsForResidency);

    EXPECT_NE(ObjectNotResident, graphicsAllocation->residencyTaskCount);
    EXPECT_EQ((int)tbxCsr->peekTaskCount() + 1, graphicsAllocation->residencyTaskCount);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenFlushIsCalledThenItShouldProcessAllocationsForResidency) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    TbxMemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    ASSERT_NE(nullptr, graphicsAllocation);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096, MemoryConstants::pageSize);
    ASSERT_NE(nullptr, commandBuffer);

    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {graphicsAllocation};

    EXPECT_EQ(ObjectNotResident, graphicsAllocation->residencyTaskCount);

    tbxCsr->flush(batchBuffer, engineType, &allocationsForResidency);

    EXPECT_NE(ObjectNotResident, graphicsAllocation->residencyTaskCount);
    EXPECT_EQ((int)tbxCsr->peekTaskCount() + 1, graphicsAllocation->residencyTaskCount);

    memoryManager->freeGraphicsMemory(commandBuffer);
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST(TbxMemoryManagerTest, givenTbxMemoryManagerWhenItIsQueriedForSystemSharedMemoryThen1GBIsReturned) {
    TbxMemoryManager memoryManager;
    EXPECT_EQ(1 * GB, memoryManager.getSystemSharedMemory());
}