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

#include "runtime/helpers/base_object.h"
#include "runtime/helpers/validators.h"
#include "runtime/context/context.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/device/device.h"
#include "runtime/event/event.h"
#include "runtime/kernel/kernel.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/platform/platform.h"
#include "runtime/program/program.h"
#include "runtime/sampler/sampler.h"

namespace OCLRT {

cl_int validateObject(void *ptr) {
    return ptr != nullptr
               ? CL_SUCCESS
               : CL_INVALID_VALUE;
}

cl_int validateObject(cl_context object) {
    return castToObject<Context>(object) != nullptr
               ? CL_SUCCESS
               : CL_INVALID_CONTEXT;
}

cl_int validateObject(cl_device_id object) {
    return castToObject<Device>(object) != nullptr
               ? CL_SUCCESS
               : CL_INVALID_DEVICE;
}

cl_int validateObject(cl_platform_id object) {
    return castToObject<Platform>(object) != nullptr
               ? CL_SUCCESS
               : CL_INVALID_PLATFORM;
}

cl_int validateObject(cl_command_queue object) {
    return castToObject<CommandQueue>(object) != nullptr
               ? CL_SUCCESS
               : CL_INVALID_COMMAND_QUEUE;
}

cl_int validateObject(cl_event object) {
    return castToObject<Event>(object) != nullptr
               ? CL_SUCCESS
               : CL_INVALID_EVENT;
}

cl_int validateObject(cl_mem object) {
    return castToObject<MemObj>(object) != nullptr
               ? CL_SUCCESS
               : CL_INVALID_MEM_OBJECT;
}

cl_int validateObject(cl_sampler object) {
    return castToObject<Sampler>(object) != nullptr
               ? CL_SUCCESS
               : CL_INVALID_SAMPLER;
}

cl_int validateObject(cl_program object) {
    return castToObject<Program>(object) != nullptr
               ? CL_SUCCESS
               : CL_INVALID_PROGRAM;
}

cl_int validateObject(cl_kernel object) {
    return castToObject<Kernel>(object) != nullptr
               ? CL_SUCCESS
               : CL_INVALID_KERNEL;
}

cl_int validateObject(const EventWaitList &eventWaitList) {
    if ((!eventWaitList.first) != (!eventWaitList.second))
        return CL_INVALID_EVENT_WAIT_LIST;

    for (cl_uint i = 0; i < eventWaitList.first; i++) {
        if (validateObject(eventWaitList.second[i]) != CL_SUCCESS)
            return CL_INVALID_EVENT_WAIT_LIST;
    }

    return CL_SUCCESS;
}

cl_int validateObject(const DeviceList &deviceList) {
    if ((!deviceList.first) != (!deviceList.second))
        return CL_INVALID_VALUE;

    for (cl_uint i = 0; i < deviceList.first; i++) {
        if (validateObject(deviceList.second[i]) != CL_SUCCESS)
            return CL_INVALID_DEVICE;
    }

    return CL_SUCCESS;
}

cl_int validateObject(const MemObjList &memObjList) {
    if ((!memObjList.first) != (!memObjList.second))
        return CL_INVALID_VALUE;

    for (cl_uint i = 0; i < memObjList.first; i++) {
        if (validateObject(memObjList.second[i]) != CL_SUCCESS)
            return CL_INVALID_MEM_OBJECT;
    }

    return CL_SUCCESS;
}

cl_int validateObject(const NonZeroBufferSize &nzbs) {
    return nzbs ? CL_SUCCESS : CL_INVALID_BUFFER_SIZE;
}

cl_int validateObject(const PatternSize &ps) {
    switch ((cl_int)ps) {
    case 128:
    case 64:
    case 32:
    case 16:
    case 8:
    case 4:
    case 2:
    case 1:
        return CL_SUCCESS;
    default:
        break;
    }

    return CL_INVALID_VALUE;
}

cl_int validateYuvOperation(const size_t *origin, const size_t *region) {
    if (!origin || !region)
        return CL_INVALID_VALUE;
    return ((origin[0] % 2 == 0) && (region[0] % 2 == 0)) ? CL_SUCCESS : CL_INVALID_VALUE;
}

bool IsPackedYuvImage(const cl_image_format *imageFormat) {
    auto channelOrder = imageFormat->image_channel_order;
    return (channelOrder == CL_YUYV_INTEL) ||
           (channelOrder == CL_UYVY_INTEL) ||
           (channelOrder == CL_YVYU_INTEL) ||
           (channelOrder == CL_VYUY_INTEL);
}

bool IsNV12Image(const cl_image_format *imageFormat) {
    return imageFormat->image_channel_order == CL_NV12_INTEL;
}
} // namespace OCLRT
