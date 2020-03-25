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
#include "runtime/command_queue/command_queue_hw.h"

////////////////////////////////////////////////////////////////////////////////
// MockCommandQueue - Core implementation
////////////////////////////////////////////////////////////////////////////////

namespace OCLRT {
class MockCommandQueue : public CommandQueue {
  public:
    using CommandQueue::device;

    void setProfilingEnabled() {
        commandQueueProperties |= CL_QUEUE_PROFILING_ENABLE;
    }
    MockCommandQueue() : CommandQueue(nullptr, nullptr, 0) {}
    MockCommandQueue(Context *context, Device *device, const cl_queue_properties *props)
        : CommandQueue(context, device, props) {
    }

    void releaseIndirectHeap(IndirectHeap::Type heap) override {
        releaseIndirectHeapCalled = true;
        CommandQueue::releaseIndirectHeap(heap);
    }
    bool releaseIndirectHeapCalled = false;
};

template <typename GfxFamily>
class MockCommandQueueHw : public CommandQueueHw<GfxFamily> {
    typedef CommandQueueHw<GfxFamily> BaseClass;

  public:
    using BaseClass::createAllocationForHostSurface;

    MockCommandQueueHw(Context *context,
                       Device *device,
                       cl_queue_properties *properties) : BaseClass(context, device, properties) {
    }

    cl_int enqueueWriteImage(Image *dstImage,
                             cl_bool blockingWrite,
                             const size_t *origin,
                             const size_t *region,
                             size_t inputRowPitch,
                             size_t inputSlicePitch,
                             const void *ptr,
                             cl_uint numEventsInWaitList,
                             const cl_event *eventWaitList,
                             cl_event *event) override {
        EnqueueWriteImageCounter++;
        return BaseClass::enqueueWriteImage(dstImage,
                                            blockingWrite,
                                            origin,
                                            region,
                                            inputRowPitch,
                                            inputSlicePitch,
                                            ptr,
                                            numEventsInWaitList,
                                            eventWaitList,
                                            event);
    }

    cl_int enqueueWriteBuffer(Buffer *buffer, cl_bool blockingWrite, size_t offset, size_t size,
                              const void *ptr, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) override {
        EnqueueWriteBufferCounter++;
        blockingWriteBuffer = blockingWrite == CL_TRUE;
        return BaseClass::enqueueWriteBuffer(buffer, blockingWrite, offset, size, ptr, numEventsInWaitList, eventWaitList, event);
    }

    void enqueueHandlerHook(const unsigned int commandType, const MultiDispatchInfo &dispatchInfo) override {
        lastCommandType = commandType;
        for (auto &di : dispatchInfo) {
            lastEnqueuedKernels.push_back(di.getKernel());
        }
    }

    unsigned int lastCommandType;
    std::vector<Kernel *> lastEnqueuedKernels;
    size_t EnqueueWriteImageCounter = 0;
    size_t EnqueueWriteBufferCounter = 0;
    bool blockingWriteBuffer = false;

    LinearStream *peekCommandStream() {
        return this->commandStream;
    }
};
} // namespace OCLRT
