/*
 * Copyright (c) 2017, Intel Corporation
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
#include "runtime/mem_obj/image.h"
#include "runtime/sampler/sampler.h"
#include "unit_tests/aub_tests/fixtures/aub_parent_kernel_fixture.h"
#include "unit_tests/fixtures/buffer_fixture.h"

#include "test.h"

using namespace OCLRT;

GEN9TEST_F(AUBParentKernelFixture, EnqueueParentKernel) {
    if (pDevice->getSupportedClVersion() >= 20) {
        ASSERT_NE(nullptr, pKernel);
        ASSERT_TRUE(pKernel->isParentKernel);

        const cl_queue_properties properties[3] = {(CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE),
                                                   0, 0};

        DeviceQueue *devQueue = DeviceQueue::create(
            &pCmdQ->getContext(),
            pDevice,
            properties[0],
            retVal);

        BuiltIns &builtIns = BuiltIns::getInstance();
        SchedulerKernel &scheduler = builtIns.getSchedulerKernel(pCmdQ->getContext());
        // Aub execution takes huge time for bigger GWS
        scheduler.setGws(24);

        size_t offset[3] = {0, 0, 0};
        size_t gws[3] = {1, 1, 1};
        size_t lws[3] = {1, 1, 1};

        // clang-format off
        cl_image_format imageFormat;
        imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
        imageFormat.image_channel_order = CL_R;

        cl_image_desc desc = { 0 };
        desc.image_array_size = 0;
        desc.image_depth = 1;
        desc.image_height = 4;
        desc.image_width = 4;
        desc.image_type = CL_MEM_OBJECT_IMAGE3D;
        desc.image_row_pitch = 0;
        desc.image_slice_pitch = 0;
        // clang-format on

        auto surfaceFormat = Image::getSurfaceFormatFromTable(0, &imageFormat);
        Image *image = Image::create(
            pContext,
            0,
            surfaceFormat,
            &desc,
            nullptr,
            retVal);

        Buffer *buffer = BufferHelper<BufferUseHostPtr<>>::create(pContext);

        cl_mem bufferMem = buffer;
        cl_mem imageMem = image;

        auto sampler = Sampler::create(
            pContext,
            CL_TRUE,
            CL_ADDRESS_NONE,
            CL_FILTER_LINEAR,
            retVal);

        size_t argScalar = 2;
        pKernel->setArg(
            3,
            sizeof(size_t),
            &argScalar);

        pKernel->setArg(
            2,
            sizeof(cl_mem),
            &bufferMem);

        pKernel->setArg(
            1,
            sizeof(cl_mem),
            &imageMem);

        pKernel->setArg(
            0,
            sizeof(cl_sampler),
            &sampler);

        pCmdQ->enqueueKernel(pKernel, 1, offset, gws, lws, 0, 0, 0);

        pCmdQ->finish(false);

        uint32_t expectedNumberOfEnqueues = 1;
        IGIL_CommandQueue *igilQueue = reinterpret_cast<IGIL_CommandQueue *>(devQueue->getQueueBuffer()->getUnderlyingBuffer());
        uint64_t gpuAddress = devQueue->getQueueBuffer()->getGpuAddress();
        uint32_t *pointerTonumberOfEnqueues = &igilQueue->m_controls.m_TotalNumberOfQueues;
        size_t offsetToEnqueues = ptrDiff(pointerTonumberOfEnqueues, igilQueue);
        gpuAddress = gpuAddress + offsetToEnqueues;

        AUBCommandStreamFixture::expectMemory<FamilyType>((void *)(uintptr_t)gpuAddress, &expectedNumberOfEnqueues, sizeof(uint32_t));
        AUBCommandStreamFixture::expectMemory<FamilyType>((void *)(uintptr_t)buffer->getGraphicsAllocation()->getGpuAddress(), &argScalar, sizeof(size_t));

        delete devQueue;
        delete image;
        delete buffer;
        delete sampler;
    }
}
