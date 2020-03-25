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

#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "reg_configs_common.h"
#include "runtime/memory_manager/memory_manager.h"
#include "unit_tests/command_queue/enqueue_write_image_fixture.h"
#include "unit_tests/gen_common/gen_commands_common_validation.h"
#include "unit_tests/mocks/mock_builtin_dispatch_info_builder.h"
#include "test.h"

using namespace OCLRT;

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueWriteImageTest, gpgpuWalker) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    enqueueWriteImage<FamilyType>();

    auto *cmd = reinterpret_cast<GPGPU_WALKER *>(cmdWalker);
    ASSERT_NE(nullptr, cmd);

    // Verify GPGPU_WALKER parameters
    EXPECT_NE(0u, cmd->getThreadGroupIdXDimension());
    EXPECT_NE(0u, cmd->getThreadGroupIdYDimension());
    EXPECT_NE(0u, cmd->getThreadGroupIdZDimension());
    EXPECT_NE(0u, cmd->getRightExecutionMask());
    EXPECT_NE(0u, cmd->getBottomExecutionMask());
    EXPECT_EQ(GPGPU_WALKER::SIMD_SIZE_SIMD32, cmd->getSimdSize());
    EXPECT_NE(0u, cmd->getIndirectDataLength());
    EXPECT_FALSE(cmd->getIndirectParameterEnable());

    // Compute the SIMD lane mask
    size_t simd =
        cmd->getSimdSize() == GPGPU_WALKER::SIMD_SIZE_SIMD32 ? 32 : cmd->getSimdSize() == GPGPU_WALKER::SIMD_SIZE_SIMD16 ? 16 : 8;
    uint64_t simdMask = (1ull << simd) - 1;

    // Mask off lanes based on the execution masks
    auto laneMaskRight = cmd->getRightExecutionMask() & simdMask;
    auto lanesPerThreadX = 0;
    while (laneMaskRight) {
        lanesPerThreadX += laneMaskRight & 1;
        laneMaskRight >>= 1;
    }
}

HWTEST_F(EnqueueWriteImageTest, alignsToCSR_Blocking) {
    //this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;
    auto oldCsrTaskLevel = csr.peekTaskLevel();

    EnqueueWriteImageHelper<>::enqueueWriteImage(pCmdQ, dstImage, CL_TRUE);
    EXPECT_EQ(csr.peekTaskCount(), pCmdQ->taskCount);
    EXPECT_EQ(oldCsrTaskLevel, pCmdQ->taskLevel);
}

HWTEST_F(EnqueueWriteImageTest, alignsToCSR_NonBlocking) {
    //this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;

    EnqueueWriteImageHelper<>::enqueueWriteImage(pCmdQ, dstImage, CL_FALSE);
    EXPECT_EQ(csr.peekTaskCount(), pCmdQ->taskCount);
    EXPECT_EQ(csr.peekTaskLevel(), pCmdQ->taskLevel + 1);
}

HWTEST_F(EnqueueWriteImageTest, bumpsTaskLevel) {
    auto taskLevelBefore = pCmdQ->taskLevel;

    EnqueueWriteImageHelper<>::enqueueWriteImage(pCmdQ, dstImage, EnqueueWriteImageTraits::blocking);
    EXPECT_GT(pCmdQ->taskLevel, taskLevelBefore);
}

HWTEST_F(EnqueueWriteImageTest, addsCommands) {
    auto usedCmdBufferBefore = pCS->getUsed();

    EnqueueWriteImageHelper<>::enqueueWriteImage(pCmdQ, dstImage, EnqueueWriteImageTraits::blocking);
    EXPECT_NE(usedCmdBufferBefore, pCS->getUsed());
}

HWTEST_F(EnqueueWriteImageTest, addsIndirectData) {
    auto dshBefore = pDSH->getUsed();
    auto iohBefore = pIOH->getUsed();
    auto sshBefore = pSSH->getUsed();

    EnqueueWriteImageHelper<>::enqueueWriteImage(pCmdQ, dstImage, EnqueueWriteImageTraits::blocking);
    EXPECT_NE(dshBefore, pDSH->getUsed());
    EXPECT_NE(iohBefore, pIOH->getUsed());
    EXPECT_NE(sshBefore, pSSH->getUsed());
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueWriteImageTest, loadRegisterImmediateL3CNTLREG) {
    enqueueWriteImage<FamilyType>();
    validateL3Programming<FamilyType>(cmdList, itorWalker);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueWriteImageTest, WhenEnqueueIsDoneThenStateBaseAddressIsProperlyProgrammed) {
    enqueueWriteImage<FamilyType>();
    validateStateBaseAddress<FamilyType>(this->pDevice->getCommandStreamReceiver().getMemoryManager()->getInternalHeapBaseAddress(),
                                         pDSH, pIOH, pSSH, itorPipelineSelect, itorWalker, cmdList, 0llu);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueWriteImageTest, mediaInterfaceDescriptorLoad) {
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueWriteImage<FamilyType>();

    // All state should be programmed before walker
    auto cmd = reinterpret_cast<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmdMediaInterfaceDescriptorLoad);
    ASSERT_NE(nullptr, cmd);

    // Verify we have a valid length -- multiple of INTERFACE_DESCRIPTOR_DATAs
    EXPECT_EQ(0u, cmd->getInterfaceDescriptorTotalLength() % sizeof(INTERFACE_DESCRIPTOR_DATA));

    // Validate the start address
    size_t alignmentStartAddress = 64 * sizeof(uint8_t);
    EXPECT_EQ(0u, cmd->getInterfaceDescriptorDataStartAddress() % alignmentStartAddress);

    // Validate the length
    EXPECT_NE(0u, cmd->getInterfaceDescriptorTotalLength());
    size_t alignmentTotalLength = 32 * sizeof(uint8_t);
    EXPECT_EQ(0u, cmd->getInterfaceDescriptorTotalLength() % alignmentTotalLength);

    // Generically validate this command
    FamilyType::PARSE::template validateCommand<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmdList.begin(), itorMediaInterfaceDescriptorLoad);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueWriteImageTest, interfaceDescriptorData) {
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueWriteImage<FamilyType>();

    // Extract the interfaceDescriptorData
    auto cmdSBA = (STATE_BASE_ADDRESS *)cmdStateBaseAddress;
    auto &interfaceDescriptorData = *(INTERFACE_DESCRIPTOR_DATA *)cmdInterfaceDescriptorData;

    // Validate the kernel start pointer.  Technically, a kernel can start at address 0 but let's force a value.
    auto kernelStartPointer = ((uint64_t)interfaceDescriptorData.getKernelStartPointerHigh() << 32) + interfaceDescriptorData.getKernelStartPointer();
    EXPECT_LE(kernelStartPointer, cmdSBA->getInstructionBufferSize() * MemoryConstants::pageSize);

    // EnqueueWriteImage uses a byte copy.  Need to convert to bytes.
    auto localWorkSize = 2 * 2 * sizeof(float);
    auto simd = 32;
    auto threadsPerThreadGroup = (localWorkSize + simd - 1) / simd;
    EXPECT_EQ(threadsPerThreadGroup, interfaceDescriptorData.getNumberOfThreadsInGpgpuThreadGroup());
    EXPECT_NE(0u, interfaceDescriptorData.getCrossThreadConstantDataReadLength());
    EXPECT_NE(0u, interfaceDescriptorData.getConstantIndirectUrbEntryReadLength());

    // We shouldn't have these pointers the same.
    EXPECT_NE(kernelStartPointer, interfaceDescriptorData.getBindingTablePointer());
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueWriteImageTest, surfaceState) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    enqueueWriteImage<FamilyType>();

    // BufferToImage kernel uses BTI=1 for destSurface
    uint32_t bindingTableIndex = 1;
    const auto &surfaceState = getSurfaceState<FamilyType>(bindingTableIndex);

    // EnqueueWriteImage uses  multi-byte copies depending on per-pixel-size-in-bytes
    const auto &imageDesc = dstImage->getImageDesc();
    EXPECT_EQ(imageDesc.image_width, surfaceState.getWidth());
    EXPECT_EQ(imageDesc.image_height, surfaceState.getHeight());
    EXPECT_NE(0u, surfaceState.getSurfacePitch());
    EXPECT_NE(0u, surfaceState.getSurfaceType());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_UINT, surfaceState.getSurfaceFormat());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_4, surfaceState.getSurfaceHorizontalAlignment());
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_4, surfaceState.getSurfaceVerticalAlignment());
    EXPECT_EQ(reinterpret_cast<uint64_t>(dstImage->getCpuAddress()), surfaceState.getSurfaceBaseAddress());
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueWriteImageTest, pipelineSelect) {
    enqueueWriteImage<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueWriteImageTest, mediaVFEState) {
    typedef typename FamilyType::MEDIA_VFE_STATE MEDIA_VFE_STATE;

    enqueueWriteImage<FamilyType>();

    auto *cmd = (MEDIA_VFE_STATE *)cmdMediaVfeState;

    // Verify we have a valid length
    EXPECT_EQ(pDevice->getHardwareInfo().pSysInfo->ThreadCount, cmd->getMaximumNumberOfThreads());
    EXPECT_NE(0u, cmd->getNumberOfUrbEntries());
    EXPECT_NE(0u, cmd->getUrbEntryAllocationSize());

    // Generically validate this command
    FamilyType::PARSE::template validateCommand<MEDIA_VFE_STATE *>(cmdList.begin(), itorMediaVfeState);
}

HWTEST_F(EnqueueWriteImageTest, GivenImage1DarrayWhenReadWriteImageIsCalledThenHostPtrSizeIsCalculatedProperly) {
    auto dstImage2 = Image1dArrayHelper<>::create(context);
    auto imageDesc = dstImage2->getImageDesc();
    auto imageSize = imageDesc.image_width * imageDesc.image_array_size * 4;
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_array_size, 1};

    EnqueueWriteImageHelper<>::enqueueWriteImage(pCmdQ, dstImage2, CL_FALSE, origin, region);

    auto memoryManager = pCmdQ->getDevice().getMemoryManager();

    auto temporaryAllocation1 = memoryManager->graphicsAllocations.peekHead();
    ASSERT_NE(nullptr, temporaryAllocation1);

    EXPECT_EQ(temporaryAllocation1->getUnderlyingBufferSize(), imageSize);

    EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ, dstImage, CL_FALSE, origin, region);
    auto temporaryAllocation2 = temporaryAllocation1->next;
    ASSERT_NE(nullptr, temporaryAllocation2);
    EXPECT_EQ(temporaryAllocation1->getUnderlyingBufferSize(), imageSize);

    delete dstImage2;
}

HWTEST_F(EnqueueWriteImageTest, GivenImage2DarrayWhenReadWriteImageIsCalledThenHostPtrSizeIsCalculatedProperly) {
    auto dstImage2 = Image2dArrayHelper<>::create(context);
    auto imageDesc = dstImage2->getImageDesc();
    auto imageSize = imageDesc.image_width * imageDesc.image_height * imageDesc.image_array_size * 4;
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_height, imageDesc.image_array_size};

    EnqueueWriteImageHelper<>::enqueueWriteImage(pCmdQ, dstImage2, CL_FALSE, origin, region);

    auto memoryManager = pCmdQ->getDevice().getMemoryManager();

    auto temporaryAllocation1 = memoryManager->graphicsAllocations.peekHead();
    ASSERT_NE(nullptr, temporaryAllocation1);

    EXPECT_EQ(temporaryAllocation1->getUnderlyingBufferSize(), imageSize);

    EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ, dstImage, CL_FALSE, origin, region);
    auto temporaryAllocation2 = temporaryAllocation1->next;
    ASSERT_NE(nullptr, temporaryAllocation2);
    EXPECT_EQ(temporaryAllocation1->getUnderlyingBufferSize(), imageSize);

    delete dstImage2;
}

HWTEST_F(EnqueueWriteImageTest, GivenImage1DAndImageShareTheSameStorageWithHostPtrWhenReadWriteImageIsCalledThenImageIsNotWritten) {
    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<Image> dstImage2(Image1dHelper<>::create(context));
    auto imageDesc = dstImage2->getImageDesc();
    std::unique_ptr<CommandQueue> pCmdOOQ(createCommandQueue(pDevice, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE));
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, 1, 1};
    void *ptr = dstImage2->getCpuAddressForMemoryTransfer();

    size_t rowPitch = dstImage2->getHostPtrRowPitch();
    size_t slicePitch = dstImage2->getHostPtrSlicePitch();
    retVal = pCmdOOQ->enqueueWriteImage(dstImage2.get(),
                                        CL_FALSE,
                                        origin,
                                        region,
                                        rowPitch,
                                        slicePitch,
                                        ptr,
                                        0,
                                        nullptr,
                                        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdOOQ->taskLevel, 0u);
}

HWTEST_F(EnqueueWriteImageTest, GivenImage2DAndImageShareTheSameStorageWithHostPtrWhenReadWriteImageIsCalledThenImageIsNotWritten) {
    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<Image> dstImage2(Image2dHelper<>::create(context));
    auto imageDesc = dstImage2->getImageDesc();
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_height, 1};
    void *ptr = dstImage2->getCpuAddressForMemoryTransfer();

    size_t rowPitch = dstImage2->getHostPtrRowPitch();
    size_t slicePitch = dstImage2->getHostPtrSlicePitch();
    retVal = pCmdQ->enqueueWriteImage(dstImage2.get(),
                                      CL_FALSE,
                                      origin,
                                      region,
                                      rowPitch,
                                      slicePitch,
                                      ptr,
                                      0,
                                      nullptr,
                                      nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdQ->taskLevel, 0u);
}

HWTEST_F(EnqueueWriteImageTest, GivenImage3DAndImageShareTheSameStorageWithHostPtrWhenReadWriteImageIsCalledThenImageIsNotWritten) {
    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<Image> dstImage2(Image3dHelper<>::create(context));
    auto imageDesc = dstImage2->getImageDesc();
    std::unique_ptr<CommandQueue> pCmdOOQ(createCommandQueue(pDevice, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE));
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_height, imageDesc.image_depth};
    void *ptr = dstImage2->getCpuAddressForMemoryTransfer();

    size_t rowPitch = dstImage2->getHostPtrRowPitch();
    size_t slicePitch = dstImage2->getHostPtrSlicePitch();
    retVal = pCmdOOQ->enqueueWriteImage(dstImage2.get(),
                                        CL_FALSE,
                                        origin,
                                        region,
                                        rowPitch,
                                        slicePitch,
                                        ptr,
                                        0,
                                        nullptr,
                                        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdOOQ->taskLevel, 0u);
}

HWTEST_F(EnqueueWriteImageTest, GivenImage1DArrayAndImageShareTheSameStorageWithHostPtrWhenReadWriteImageIsCalledThenImageIsNotWritten) {
    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<Image> dstImage2(Image1dArrayHelper<>::create(context));
    auto imageDesc = dstImage2->getImageDesc();
    size_t origin[] = {imageDesc.image_width / 2, imageDesc.image_array_size / 2, 0};
    size_t region[] = {imageDesc.image_width - (imageDesc.image_width / 2), imageDesc.image_array_size - (imageDesc.image_array_size / 2), 1};
    void *ptr = dstImage2->getCpuAddressForMemoryTransfer();
    auto bytesPerPixel = 4;
    size_t rowPitch = dstImage2->getHostPtrRowPitch();
    size_t slicePitch = dstImage2->getHostPtrSlicePitch();
    auto pOffset = origin[2] * rowPitch + origin[1] * slicePitch + origin[0] * bytesPerPixel;
    void *ptrStorage = ptrOffset(ptr, pOffset);
    retVal = pCmdQ->enqueueWriteImage(dstImage2.get(),
                                      CL_FALSE,
                                      origin,
                                      region,
                                      rowPitch,
                                      slicePitch,
                                      ptrStorage,
                                      0,
                                      nullptr,
                                      nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdQ->taskLevel, 0u);
}

HWTEST_F(EnqueueWriteImageTest, GivenImage2DArrayAndImageShareTheSameStorageWithHostPtrWhenReadWriteImageIsCalledThenImageIsNotWritten) {
    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<Image> dstImage2(Image2dArrayHelper<>::create(context));
    auto imageDesc = dstImage2->getImageDesc();
    std::unique_ptr<CommandQueue> pCmdOOQ(createCommandQueue(pDevice, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE));
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_height, imageDesc.image_array_size};
    void *ptr = dstImage2->getCpuAddressForMemoryTransfer();
    size_t rowPitch = dstImage2->getHostPtrRowPitch();
    size_t slicePitch = dstImage2->getHostPtrSlicePitch();
    retVal = pCmdOOQ->enqueueWriteImage(dstImage2.get(),
                                        CL_FALSE,
                                        origin,
                                        region,
                                        rowPitch,
                                        slicePitch,
                                        ptr,
                                        0,
                                        nullptr,
                                        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdOOQ->taskLevel, 0u);
}

HWTEST_F(EnqueueWriteImageTest, GivenImage2DAndImageShareTheSameStorageWithHostPtrAndEventsWhenReadWriteImageIsCalledThenImageIsNotWritten) {
    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<Image> dstImage2(Image2dHelper<>::create(context));
    auto imageDesc = dstImage2->getImageDesc();
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_height, 1};
    void *ptr = dstImage2->getCpuAddressForMemoryTransfer();

    size_t rowPitch = dstImage2->getHostPtrRowPitch();
    size_t slicePitch = dstImage2->getHostPtrSlicePitch();
    uint32_t taskLevelCmdQ = 17;
    pCmdQ->taskLevel = taskLevelCmdQ;

    uint32_t taskLevelEvent1 = 8;
    uint32_t taskLevelEvent2 = 19;
    Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent1, 4);
    Event event2(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent2, 10);

    cl_event eventWaitList[] =
        {
            &event1,
            &event2};
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;

    retVal = pCmdQ->enqueueWriteImage(dstImage2.get(),
                                      CL_FALSE,
                                      origin,
                                      region,
                                      rowPitch,
                                      slicePitch,
                                      ptr,
                                      numEventsInWaitList,
                                      eventWaitList,
                                      &event);

    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_EQ(19u, pEvent->taskLevel);
    EXPECT_EQ(19u, pCmdQ->taskLevel);
    EXPECT_EQ(CL_COMMAND_WRITE_IMAGE, (const int)pEvent->getCommandType());

    pEvent->release();
}

HWTEST_F(EnqueueWriteImageTest, GivenImage3DAndImageShareTheSameStorageWithHostPtrAndEventsWhenReadWriteImageIsCalledThenImageIsNotWritten) {
    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<Image> dstImage2(Image3dHelper<>::create(context));
    auto imageDesc = dstImage2->getImageDesc();
    std::unique_ptr<CommandQueue> pCmdOOQ(createCommandQueue(pDevice, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE));
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_height, imageDesc.image_depth};
    void *ptr = dstImage2->getCpuAddressForMemoryTransfer();

    size_t rowPitch = dstImage2->getHostPtrRowPitch();
    size_t slicePitch = dstImage2->getHostPtrSlicePitch();

    uint32_t taskLevelCmdQ = 17;
    pCmdOOQ->taskLevel = taskLevelCmdQ;

    uint32_t taskLevelEvent1 = 8;
    uint32_t taskLevelEvent2 = 19;
    Event event1(pCmdOOQ.get(), CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent1, 4);
    Event event2(pCmdOOQ.get(), CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent2, 10);

    cl_event eventWaitList[] =
        {
            &event1,
            &event2};
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;

    retVal = pCmdOOQ->enqueueWriteImage(dstImage2.get(),
                                        CL_FALSE,
                                        origin,
                                        region,
                                        rowPitch,
                                        slicePitch,
                                        ptr,
                                        numEventsInWaitList,
                                        eventWaitList,
                                        &event);

    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_EQ(19u, pEvent->taskLevel);
    EXPECT_EQ(19u, pCmdOOQ->taskLevel);
    EXPECT_EQ(CL_COMMAND_WRITE_IMAGE, (const int)pEvent->getCommandType());

    pEvent->release();
}

HWTEST_F(EnqueueWriteImageTest, GivenNonZeroCopyImage2DAndImageShareTheSameStorageWithHostPtrWhenReadWriteImageIsCalledThenImageIsWritten) {
    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<Image> dstImage2(ImageHelper<ImageUseHostPtr<Image2dDefaults>>::create(context));
    auto imageDesc = dstImage2->getImageDesc();
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_height, 1};
    void *ptr = dstImage2->getCpuAddressForMemoryTransfer();

    size_t rowPitch = dstImage2->getHostPtrRowPitch();
    size_t slicePitch = dstImage2->getHostPtrSlicePitch();
    retVal = pCmdQ->enqueueWriteImage(dstImage2.get(),
                                      CL_FALSE,
                                      origin,
                                      region,
                                      rowPitch,
                                      slicePitch,
                                      ptr,
                                      0,
                                      nullptr,
                                      nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdQ->taskLevel, 2u);
}

typedef EnqueueWriteImageMipMapTest MipMapWriteImageTest;

HWTEST_P(MipMapWriteImageTest, GivenImageWithMipLevelNonZeroWhenReadImageIsCalledThenProperMipLevelIsSet) {
    auto image_type = (cl_mem_object_type)GetParam();
    auto &origBuilder = BuiltIns::getInstance().getBuiltinDispatchInfoBuilder(
        EBuiltInOps::CopyBufferToImage3d,
        pCmdQ->getContext(),
        pCmdQ->getDevice());

    // substitute original builder with mock builder
    auto oldBuilder = BuiltIns::getInstance().setBuiltinDispatchInfoBuilder(
        EBuiltInOps::CopyBufferToImage3d,
        pCmdQ->getContext(),
        pCmdQ->getDevice(),
        std::unique_ptr<OCLRT::BuiltinDispatchInfoBuilder>(new MockBuiltinDispatchInfoBuilder(BuiltIns::getInstance(), &origBuilder)));

    cl_int retVal = CL_SUCCESS;
    cl_image_desc imageDesc = {};
    uint32_t expectedMipLevel = 3;
    imageDesc.image_type = image_type;
    imageDesc.num_mip_levels = 10;
    imageDesc.image_width = 4;
    imageDesc.image_height = 1;
    imageDesc.image_depth = 1;
    size_t origin[] = {0, 0, 0, 0};
    size_t region[] = {imageDesc.image_width, 1, 1};
    std::unique_ptr<Image> image;
    switch (image_type) {
    case CL_MEM_OBJECT_IMAGE1D:
        origin[1] = expectedMipLevel;
        image = std::unique_ptr<Image>(ImageHelper<Image1dDefaults>::create(context, &imageDesc));
        break;
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        imageDesc.image_array_size = 2;
        origin[2] = expectedMipLevel;
        image = std::unique_ptr<Image>(ImageHelper<Image1dArrayDefaults>::create(context, &imageDesc));
        break;
    case CL_MEM_OBJECT_IMAGE2D:
        origin[2] = expectedMipLevel;
        image = std::unique_ptr<Image>(ImageHelper<Image2dDefaults>::create(context, &imageDesc));
        break;
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        imageDesc.image_array_size = 2;
        origin[3] = expectedMipLevel;
        image = std::unique_ptr<Image>(ImageHelper<Image2dArrayDefaults>::create(context, &imageDesc));
        break;
    case CL_MEM_OBJECT_IMAGE3D:
        origin[3] = expectedMipLevel;
        image = std::unique_ptr<Image>(ImageHelper<Image3dDefaults>::create(context, &imageDesc));
        break;
    }
    EXPECT_NE(nullptr, image.get());

    std::unique_ptr<uint32_t[]> ptr = std::unique_ptr<uint32_t[]>(new uint32_t[3]);
    retVal = pCmdQ->enqueueWriteImage(image.get(),
                                      CL_FALSE,
                                      origin,
                                      region,
                                      0,
                                      0,
                                      ptr.get(),
                                      0,
                                      nullptr,
                                      nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto &mockBuilder = static_cast<MockBuiltinDispatchInfoBuilder &>(BuiltIns::getInstance().getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToImage3d,
                                                                                                                            pCmdQ->getContext(),
                                                                                                                            pCmdQ->getDevice()));
    auto params = mockBuilder.getBuiltinOpParams();

    EXPECT_EQ(expectedMipLevel, params->dstMipLevel);

    // restore original builder and retrieve mock builder
    auto newBuilder = BuiltIns::getInstance().setBuiltinDispatchInfoBuilder(
        EBuiltInOps::CopyBufferToImage3d,
        pCmdQ->getContext(),
        pCmdQ->getDevice(),
        std::move(oldBuilder));
    EXPECT_NE(nullptr, newBuilder);
}

INSTANTIATE_TEST_CASE_P(MipMapWriteImageTest_GivenImageWithMipLevelNonZeroWhenReadImageIsCalledThenProperMipLevelIsSet,
                        MipMapWriteImageTest, ::testing::Values(CL_MEM_OBJECT_IMAGE1D, CL_MEM_OBJECT_IMAGE1D_ARRAY, CL_MEM_OBJECT_IMAGE2D, CL_MEM_OBJECT_IMAGE2D_ARRAY, CL_MEM_OBJECT_IMAGE3D));

using NegativeFailAllocationTest = Test<NegativeFailAllocationCommandEnqueueBaseFixture>;

HWTEST_F(NegativeFailAllocationTest, givenEnqueueReadImageWhenHostPtrAllocationCreationFailsThenReturnOutOfResource) {
    cl_int retVal = CL_SUCCESS;
    auto imageDesc = image->getImageDesc();

    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_height, 1};

    size_t rowPitch = image->getHostPtrRowPitch();
    size_t slicePitch = image->getHostPtrSlicePitch();

    retVal = pCmdQ->enqueueReadImage(image.get(),
                                     CL_FALSE,
                                     origin,
                                     region,
                                     rowPitch,
                                     slicePitch,
                                     ptr,
                                     0,
                                     nullptr,
                                     nullptr);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);
}
