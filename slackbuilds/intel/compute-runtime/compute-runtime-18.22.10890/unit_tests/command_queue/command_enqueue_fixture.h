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
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/command_stream/command_stream_fixture.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/indirect_heap/indirect_heap_fixture.h"
#include "unit_tests/helpers/hw_parse.h"

namespace OCLRT {

struct CommandDeviceFixture : public DeviceFixture,
                              public CommandQueueHwFixture {
    using CommandQueueHwFixture::SetUp;
    void SetUp(cl_command_queue_properties cmdQueueProperties = 0) {
        DeviceFixture::SetUp();
        CommandQueueHwFixture::SetUp(pDevice, cmdQueueProperties);
    }

    void TearDown() {
        CommandQueueHwFixture::TearDown();
        DeviceFixture::TearDown();
    }
};

struct CommandEnqueueBaseFixture : CommandDeviceFixture,
                                   public IndirectHeapFixture,
                                   public HardwareParse {
    using IndirectHeapFixture::SetUp;
    void SetUp(cl_command_queue_properties cmdQueueProperties = 0) {
        CommandDeviceFixture::SetUp(cmdQueueProperties);
        IndirectHeapFixture::SetUp(pCmdQ);
        HardwareParse::SetUp();
    }

    void TearDown() {
        HardwareParse::TearDown();
        IndirectHeapFixture::TearDown();
        CommandDeviceFixture::TearDown();
    }
};

struct CommandEnqueueFixture : public CommandEnqueueBaseFixture,
                               public CommandStreamFixture {
    void SetUp(cl_command_queue_properties cmdQueueProperties = 0) {
        CommandEnqueueBaseFixture::SetUp(cmdQueueProperties);
        CommandStreamFixture::SetUp(pCmdQ);
    }

    void TearDown() {
        CommandEnqueueBaseFixture::TearDown();
        CommandStreamFixture::TearDown();
    }
};

struct NegativeFailAllocationCommandEnqueueBaseFixture : public CommandEnqueueBaseFixture {
    void SetUp() override {
        CommandEnqueueBaseFixture::SetUp();
        failMemManager.reset(new FailMemoryManager());

        BufferDefaults::context = context;
        Image2dDefaults::context = context;
        buffer.reset(BufferHelper<>::create());
        image.reset(ImageHelper<Image2dDefaults>::create());
        ptr = static_cast<void *>(array);
        oldMemManager = pDevice->getMemoryManager();
        pDevice->getCommandStreamReceiver().setMemoryManager(failMemManager.get());
    }

    void TearDown() override {
        pDevice->getCommandStreamReceiver().setMemoryManager(oldMemManager);
        buffer.reset(nullptr);
        image.reset(nullptr);
        BufferDefaults::context = nullptr;
        Image2dDefaults::context = nullptr;
        CommandEnqueueBaseFixture::TearDown();
    }

    std::unique_ptr<Buffer> buffer;
    std::unique_ptr<Image> image;
    std::unique_ptr<FailMemoryManager> failMemManager;
    char array[MemoryConstants::cacheLineSize];
    void *ptr;
    MemoryManager *oldMemManager;
};

} // namespace OCLRT
