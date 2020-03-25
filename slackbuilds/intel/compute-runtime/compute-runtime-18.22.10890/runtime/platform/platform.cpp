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

#include "platform.h"
#include "runtime/api/api.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "CL/cl_ext.h"
#include "runtime/device/device.h"
#include "runtime/gtpin/gtpin_notify.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/get_info.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/string.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/event/async_events_handler.h"
#include "runtime/sharings/sharing_factory.h"
#include "runtime/platform/extensions.h"
#include "CL/cl_ext.h"

namespace OCLRT {

Platform platformImpl;
bool getDevices(HardwareInfo **hwInfo, size_t &numDevicesReturned);

Platform *platform() { return &platformImpl; }

Platform::Platform() {
    devices.reserve(64);
    setAsyncEventsHandler(std::unique_ptr<AsyncEventsHandler>(new AsyncEventsHandler()));
}

Platform::~Platform() {
    shutdown();
}

cl_int Platform::getInfo(cl_platform_info paramName,
                         size_t paramValueSize,
                         void *paramValue,
                         size_t *paramValueSizeRet) {
    auto retVal = CL_INVALID_VALUE;
    const std::string *param = nullptr;
    size_t paramSize = 0;
    uint64_t pVal = 0;

    switch (paramName) {
    case CL_PLATFORM_HOST_TIMER_RESOLUTION:
        pVal = static_cast<uint64_t>(this->devices[0]->getPlatformHostTimerResolution());
        paramSize = sizeof(uint64_t);
        retVal = ::getInfo(paramValue, paramValueSize, &pVal, paramSize);
        break;
    case CL_PLATFORM_PROFILE:
        param = &platformInfo->profile;
        break;
    case CL_PLATFORM_VERSION:
        param = &platformInfo->version;
        break;
    case CL_PLATFORM_NAME:
        param = &platformInfo->name;
        break;
    case CL_PLATFORM_VENDOR:
        param = &platformInfo->vendor;
        break;
    case CL_PLATFORM_EXTENSIONS:
        param = &platformInfo->extensions;
        break;
    case CL_PLATFORM_ICD_SUFFIX_KHR:
        param = &platformInfo->icdSuffixKhr;
        break;
    default:
        break;
    }

    // Case for string parameters
    if (param) {
        paramSize = param->length() + 1;
        retVal = ::getInfo(paramValue, paramValueSize, param->c_str(), paramSize);
    }

    if (paramValueSizeRet) {
        *paramValueSizeRet = paramSize;
    }

    return retVal;
}

const std::string &Platform::peekCompilerExtensions() const {
    return compilerExtensions;
}

bool Platform::initialize(size_t numDevices,
                          const HardwareInfo **devices) {
    HardwareInfo *hwInfo = nullptr;
    size_t numDevicesReturned = 0;
    const HardwareInfo **hwInfoConst = nullptr;

    TakeOwnershipWrapper<Platform> platformOwnership(*this);

    if (state == StateInited) {
        return true;
    }

    state = OCLRT::getDevices(&hwInfo, numDevicesReturned) ? StateIniting : StateNone;

    if (state == StateNone) {
        return false;
    }

    hwInfoConst = (hwInfo != nullptr) ? const_cast<const HardwareInfo **>(&hwInfo) : devices;
    numDevicesReturned = (hwInfo != nullptr) ? numDevicesReturned : numDevices;

    DEBUG_BREAK_IF(this->platformInfo);
    this->platformInfo = new PlatformInfo;

    this->devices.resize(numDevicesReturned);
    for (size_t deviceOrdinal = 0; deviceOrdinal < numDevicesReturned; ++deviceOrdinal) {
        auto pDevice = Device::create<OCLRT::Device>(hwInfoConst[deviceOrdinal]);
        DEBUG_BREAK_IF(!pDevice);
        if (pDevice) {
            this->devices[deviceOrdinal] = pDevice;

            this->platformInfo->extensions = pDevice->getDeviceInfo().deviceExtensions;

            switch (pDevice->getEnabledClVersion()) {
            case 21:
                this->platformInfo->version = "OpenCL 2.1 ";
                break;
            case 20:
                this->platformInfo->version = "OpenCL 2.0 ";
                break;
            default:
                this->platformInfo->version = "OpenCL 1.2 ";
                break;
            }

            compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(pDevice->getDeviceInfo().deviceExtensions);
        } else {
            return false;
        }
    }

    this->fillGlobalDispatchTable();

    state = StateInited;
    return true;
}

void Platform::fillGlobalDispatchTable() {
    sharingFactory.fillGlobalDispatchTable();
}

bool Platform::isInitialized() {
    TakeOwnershipWrapper<Platform> platformOwnership(*this);
    bool ret = (this->state == StateInited);
    return ret;
}

void Platform::shutdown() {
    asyncEventsHandler->closeThread();
    TakeOwnershipWrapper<Platform> platformOwnership(*this);

    if (state == StateNone) {
        return;
    }

    for (auto dev : this->devices) {
        delete dev;
    }
    devices.clear();
    state = StateNone;

    delete platformInfo;
    platformInfo = nullptr;

    DeviceFactory::releaseDevices();
    std::string().swap(compilerExtensions);

    gtpinNotifyPlatformShutdown();
}

Device *Platform::getDevice(size_t deviceOrdinal) {
    TakeOwnershipWrapper<Platform> platformOwnership(*this);

    if (this->state != StateInited || deviceOrdinal >= devices.size()) {
        return nullptr;
    }

    auto pDevice = devices[deviceOrdinal];
    DEBUG_BREAK_IF(pDevice == nullptr);

    return pDevice;
}

size_t Platform::getNumDevices() const {
    TakeOwnershipWrapper<const Platform> platformOwnership(*this);

    if (this->state != StateInited) {
        return 0;
    }

    return devices.size();
}

Device **Platform::getDevices() {
    TakeOwnershipWrapper<Platform> platformOwnership(*this);

    if (this->state != StateInited) {
        return nullptr;
    }

    return devices.data();
}

const PlatformInfo &Platform::getPlatformInfo() const {
    DEBUG_BREAK_IF(!platformInfo);
    return *platformInfo;
}

AsyncEventsHandler *Platform::getAsyncEventsHandler() {
    return asyncEventsHandler.get();
}

std::unique_ptr<AsyncEventsHandler> Platform::setAsyncEventsHandler(std::unique_ptr<AsyncEventsHandler> handler) {
    asyncEventsHandler.swap(handler);
    return handler;
}

} // namespace OCLRT
