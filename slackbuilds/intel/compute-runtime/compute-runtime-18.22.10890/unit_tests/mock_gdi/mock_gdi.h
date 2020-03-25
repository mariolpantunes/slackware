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

#include <d3d9types.h>
#include <d3dkmthk.h>
#include "umKmInc/sharedata.h"

#define DECL_FUNCTIONS()                                                                                 \
    FUNCTION(OpenAdapterFromHdc, IN OUT D3DKMT_OPENADAPTERFROMHDC *)                                     \
    FUNCTION(DestroyAllocation, IN CONST D3DKMT_DESTROYALLOCATION *)                                     \
    FUNCTION(CloseAdapter, IN CONST D3DKMT_CLOSEADAPTER *)                                               \
    FUNCTION(Lock, IN OUT D3DKMT_LOCK *)                                                                 \
    FUNCTION(Unlock, IN CONST D3DKMT_UNLOCK *)                                                           \
    FUNCTION(Render, IN OUT D3DKMT_RENDER *)                                                             \
    FUNCTION(CreateSynchronizationObject, IN OUT D3DKMT_CREATESYNCHRONIZATIONOBJECT *)                   \
    FUNCTION(DestroySynchronizationObject, IN CONST D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *)               \
    FUNCTION(SignalSynchronizationObject, IN CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECT *)                 \
    FUNCTION(WaitForSynchronizationObject, IN OUT CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECT *)           \
    FUNCTION(WaitForSynchronizationObjectFromCpu, IN CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *) \
    FUNCTION(SignalSynchronizationObjectFromCpu, IN CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU *)   \
    FUNCTION(WaitForSynchronizationObjectFromGpu, IN CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU *) \
    FUNCTION(SignalSynchronizationObjectFromGpu, IN CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMGPU *)   \
    FUNCTION(ReserveGpuVirtualAddress, IN OUT D3DDDI_RESERVEGPUVIRTUALADDRESS *)                         \
    FUNCTION(FreeGpuVirtualAddress, IN CONST D3DKMT_FREEGPUVIRTUALADDRESS *)                             \
    FUNCTION(UpdateGpuVirtualAddress, IN CONST D3DKMT_UPDATEGPUVIRTUALADDRESS *)                         \
    FUNCTION(SubmitCommand, IN CONST D3DKMT_SUBMITCOMMAND *)                                             \
    FUNCTION(Evict, IN OUT D3DKMT_EVICT *)                                                               \
    FUNCTION(GetDeviceState, IN OUT D3DKMT_GETDEVICESTATE *)                                             \
    FUNCTION(RegisterTrimNotification, IN D3DKMT_REGISTERTRIMNOTIFICATION *)                             \
    FUNCTION(UnregisterTrimNotification, IN D3DKMT_UNREGISTERTRIMNOTIFICATION *)

#define STR(X) #X

#define FUNCTION(FUNC_NAME, FUNC_ARG)                \
    NTSTATUS __stdcall D3DKMT##FUNC_NAME(FUNC_ARG) { \
        return STATUS_SUCCESS;                       \
    }

#define ADAPTER_HANDLE (static_cast<D3DKMT_HANDLE>(0x40001234))
#define DEVICE_HANDLE (static_cast<D3DKMT_HANDLE>(0x40004321))
#define PAGINGQUEUE_HANDLE (static_cast<D3DKMT_HANDLE>(0x40005678))
#define PAGINGQUEUE_SYNCOBJECT_HANDLE (static_cast<D3DKMT_HANDLE>(0x40008765))
#define CONTEXT_HANDLE (static_cast<D3DKMT_HANDLE>(0x40001111))
#define INVALID_HANDLE (static_cast<D3DKMT_HANDLE>(0))

#define RESOURCE_HANDLE (static_cast<D3DKMT_HANDLE>(0x80000000))
#define ALLOCATION_HANDLE (static_cast<D3DKMT_HANDLE>(0x80000008))

#define NT_RESOURCE_HANDLE (static_cast<D3DKMT_HANDLE>(0x80000001))
#define NT_ALLOCATION_HANDLE (static_cast<D3DKMT_HANDLE>(0x80000009))

#define GPUVA (static_cast<D3DGPU_VIRTUAL_ADDRESS>(0x80123000000))

NTSTATUS MockSetSizes(void *gmmPtr, UINT numAllocsToReturn, UINT gmmSize, UINT totalPrivateSize);
NTSTATUS GetMockSizes(UINT &destroyAlloactionWithResourceHandleCalled, D3DKMT_DESTROYALLOCATION2 *&ptrDestroyAlloc);
D3DKMT_HANDLE GetMockLastDestroyedResHandle();
void SetMockLastDestroyedResHandle(D3DKMT_HANDLE handle);
D3DKMT_CREATEDEVICE GetMockCreateDeviceParams();
void SetMockCreateDeviceParams(D3DKMT_CREATEDEVICE params);
D3DKMT_CREATEALLOCATION *getMockAllocation();
ADAPTER_INFO *getAdapterInfoAddress();
D3DDDI_MAPGPUVIRTUALADDRESS *getLastCallMapGpuVaArg();
void setMapGpuVaFailConfig(uint32_t count, uint32_t max);
D3DKMT_CREATECONTEXTVIRTUAL *getCreateContextData();
D3DKMT_CREATEHWQUEUE *getCreateHwQueueData();
D3DKMT_DESTROYHWQUEUE *getDestroyHwQueueData();
D3DKMT_SUBMITCOMMANDTOHWQUEUE *getSubmitCommandToHwQueueData();
