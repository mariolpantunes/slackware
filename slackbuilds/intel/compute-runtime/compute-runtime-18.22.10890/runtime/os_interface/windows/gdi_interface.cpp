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

#include "gdi_interface.h"

namespace OCLRT {

Gdi::Gdi() : gdiDll(Os::gdiDllName),
             initialized(false) {
    if (gdiDll.isLoaded()) {
        initialized = getAllProcAddresses();
    }
}

bool Gdi::setupHwQueueProcAddresses() {
    createHwQueue = reinterpret_cast<PFND3DKMT_CREATEHWQUEUE>(gdiDll.getProcAddress("D3DKMTCreateHwQueue"));
    destroyHwQueue = reinterpret_cast<PFND3DKMT_DESTROYHWQUEUE>(gdiDll.getProcAddress("D3DKMTDestroyHwQueue"));
    submitCommandToHwQueue = reinterpret_cast<PFND3DKMT_SUBMITCOMMANDTOHWQUEUE>(gdiDll.getProcAddress("D3DKMTSubmitCommandToHwQueue"));

    if (!createHwQueue || !destroyHwQueue || !submitCommandToHwQueue) {
        return false;
    }
    return true;
}

bool Gdi::getAllProcAddresses() {
    openAdapterFromHdc = reinterpret_cast<PFND3DKMT_OPENADAPTERFROMHDC>(gdiDll.getProcAddress("D3DKMTOpenAdapterFromHdc"));
    openAdapterFromLuid = reinterpret_cast<PFND3DKMT_OPENADAPTERFROMLUID>(gdiDll.getProcAddress("D3DKMTOpenAdapterFromLuid"));
    createAllocation = reinterpret_cast<PFND3DKMT_CREATEALLOCATION>(gdiDll.getProcAddress("D3DKMTCreateAllocation"));
    destroyAllocation = reinterpret_cast<PFND3DKMT_DESTROYALLOCATION>(gdiDll.getProcAddress("D3DKMTDestroyAllocation"));
    destroyAllocation2 = reinterpret_cast<PFND3DKMT_DESTROYALLOCATION2>(gdiDll.getProcAddress("D3DKMTDestroyAllocation2"));
    queryAdapterInfo = reinterpret_cast<PFND3DKMT_QUERYADAPTERINFO>(gdiDll.getProcAddress("D3DKMTQueryAdapterInfo"));
    closeAdapter = reinterpret_cast<PFND3DKMT_CLOSEADAPTER>(gdiDll.getProcAddress("D3DKMTCloseAdapter"));
    createDevice = reinterpret_cast<PFND3DKMT_CREATEDEVICE>(gdiDll.getProcAddress("D3DKMTCreateDevice"));
    destroyDevice = reinterpret_cast<PFND3DKMT_DESTROYDEVICE>(gdiDll.getProcAddress("D3DKMTDestroyDevice"));
    escape = reinterpret_cast<PFND3DKMT_ESCAPE>(gdiDll.getProcAddress("D3DKMTEscape"));
    createContext = reinterpret_cast<PFND3DKMT_CREATECONTEXTVIRTUAL>(gdiDll.getProcAddress("D3DKMTCreateContextVirtual"));
    destroyContext = reinterpret_cast<PFND3DKMT_DESTROYCONTEXT>(gdiDll.getProcAddress("D3DKMTDestroyContext"));
    openResource = reinterpret_cast<PFND3DKMT_OPENRESOURCE>(gdiDll.getProcAddress("D3DKMTOpenResource"));
    openResourceFromNtHandle = reinterpret_cast<PFND3DKMT_OPENRESOURCEFROMNTHANDLE>(gdiDll.getProcAddress("D3DKMTOpenResourceFromNtHandle"));
    queryResourceInfo = reinterpret_cast<PFND3DKMT_QUERYRESOURCEINFO>(gdiDll.getProcAddress("D3DKMTQueryResourceInfo"));
    queryResourceInfoFromNtHandle = reinterpret_cast<PFND3DKMT_QUERYRESOURCEINFOFROMNTHANDLE>(gdiDll.getProcAddress("D3DKMTQueryResourceInfoFromNtHandle"));
    lock = reinterpret_cast<PFND3DKMT_LOCK>(gdiDll.getProcAddress("D3DKMTLock"));
    unlock = reinterpret_cast<PFND3DKMT_UNLOCK>(gdiDll.getProcAddress("D3DKMTUnlock"));
    render = reinterpret_cast<PFND3DKMT_RENDER>(gdiDll.getProcAddress("D3DKMTRender"));
    createSynchronizationObject = reinterpret_cast<PFND3DKMT_CREATESYNCHRONIZATIONOBJECT>(gdiDll.getProcAddress("D3DKMTCreateSynchronizationObject"));
    createSynchronizationObject2 = reinterpret_cast<PFND3DKMT_CREATESYNCHRONIZATIONOBJECT2>(gdiDll.getProcAddress("D3DKMTCreateSynchronizationObject2"));
    destroySynchronizationObject = reinterpret_cast<PFND3DKMT_DESTROYSYNCHRONIZATIONOBJECT>(gdiDll.getProcAddress("D3DKMTDestroySynchronizationObject"));
    signalSynchronizationObject = reinterpret_cast<PFND3DKMT_SIGNALSYNCHRONIZATIONOBJECT>(gdiDll.getProcAddress("D3DKMTSignalSynchronizationObject"));
    waitForSynchronizationObject = reinterpret_cast<PFND3DKMT_WAITFORSYNCHRONIZATIONOBJECT>(gdiDll.getProcAddress("D3DKMTWaitForSynchronizationObject"));
    waitForSynchronizationObjectFromCpu = reinterpret_cast<PFND3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU>(gdiDll.getProcAddress("D3DKMTWaitForSynchronizationObjectFromCpu"));
    signalSynchronizationObjectFromCpu = reinterpret_cast<PFND3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU>(gdiDll.getProcAddress("D3DKMTSignalSynchronizationObjectFromCpu"));
    waitForSynchronizationObjectFromGpu = reinterpret_cast<PFND3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU>(gdiDll.getProcAddress("D3DKMTWaitForSynchronizationObjectFromGpu"));
    signalSynchronizationObjectFromGpu = reinterpret_cast<PFND3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMGPU>(gdiDll.getProcAddress("D3DKMTSignalSynchronizationObjectFromGpu"));
    createPagingQueue = reinterpret_cast<PFND3DKMT_CREATEPAGINGQUEUE>(gdiDll.getProcAddress("D3DKMTCreatePagingQueue"));
    destroyPagingQueue = reinterpret_cast<PFND3DKMT_DESTROYPAGINGQUEUE>(gdiDll.getProcAddress("D3DKMTDestroyPagingQueue"));
    lock2 = reinterpret_cast<PFND3DKMT_LOCK2>(gdiDll.getProcAddress("D3DKMTLock2"));
    unlock2 = reinterpret_cast<PFND3DKMT_UNLOCK2>(gdiDll.getProcAddress("D3DKMTUnlock2"));
    mapGpuVirtualAddress = reinterpret_cast<PFND3DKMT_MAPGPUVIRTUALADDRESS>(gdiDll.getProcAddress("D3DKMTMapGpuVirtualAddress"));
    reserveGpuVirtualAddress = reinterpret_cast<PFND3DKMT_RESERVEGPUVIRTUALADDRESS>(gdiDll.getProcAddress("D3DKMTReserveGpuVirtualAddress"));
    freeGpuVirtualAddress = reinterpret_cast<PFND3DKMT_FREEGPUVIRTUALADDRESS>(gdiDll.getProcAddress("D3DKMTFreeGpuVirtualAddress"));
    updateGpuVirtualAddress = reinterpret_cast<PFND3DKMT_UPDATEGPUVIRTUALADDRESS>(gdiDll.getProcAddress("D3DKMTUpdateGpuVirtualAddress"));
    submitCommand = reinterpret_cast<PFND3DKMT_SUBMITCOMMAND>(gdiDll.getProcAddress("D3DKMTSubmitCommand"));
    makeResident = reinterpret_cast<PFND3DKMT_MAKERESIDENT>(gdiDll.getProcAddress("D3DKMTMakeResident"));
    evict = reinterpret_cast<PFND3DKMT_EVICT>(gdiDll.getProcAddress("D3DKMTEvict"));
    registerTrimNotification = reinterpret_cast<PFND3DKMT_REGISTERTRIMNOTIFICATION>(gdiDll.getProcAddress("D3DKMTRegisterTrimNotification"));
    unregisterTrimNotification = reinterpret_cast<PFND3DKMT_UNREGISTERTRIMNOTIFICATION>(gdiDll.getProcAddress("D3DKMTUnregisterTrimNotification"));

    // For debug purposes
    getDeviceState = reinterpret_cast<PFND3DKMT_GETDEVICESTATE>(gdiDll.getProcAddress("D3DKMTGetDeviceState"));

    // clang-format off
    if (openAdapterFromHdc && openAdapterFromLuid && createAllocation && destroyAllocation
        && destroyAllocation2 && queryAdapterInfo && closeAdapter && createDevice 
        && destroyDevice && escape && createContext && destroyContext 
        && openResource && queryResourceInfo && lock && unlock && render
        && createSynchronizationObject && createSynchronizationObject2 
        && destroySynchronizationObject && signalSynchronizationObject
        && waitForSynchronizationObject && waitForSynchronizationObjectFromCpu
        && signalSynchronizationObjectFromCpu && waitForSynchronizationObjectFromGpu
        && signalSynchronizationObjectFromGpu && createPagingQueue && destroyPagingQueue
        && lock2 && unlock2 && mapGpuVirtualAddress && reserveGpuVirtualAddress
        && freeGpuVirtualAddress && updateGpuVirtualAddress &&submitCommand 
        && makeResident && evict && registerTrimNotification && unregisterTrimNotification){
        return true;
    }
    // clang-format on
    return false;
}
} // namespace OCLRT
