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
#include "runtime/memory_manager/host_ptr_defines.h"
#include "runtime/memory_manager/host_ptr_manager.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/os_interface/32bit_memory.h"
#include "runtime/helpers/aligned_memory.h"

#include <cstdint>
#include <vector>
#include <mutex>

namespace OCLRT {
class Device;
class DeferredDeleter;
class GraphicsAllocation;
class CommandStreamReceiver;

struct HwPerfCounter;
struct HwTimeStamps;

template <typename T1>
class TagAllocator;

template <typename T1>
struct TagNode;

class AllocsTracker;
class MapBaseAllocationTracker;
class SVMAllocsManager;

enum allocationType {
    TEMPORARY_ALLOCATION,
    REUSABLE_ALLOCATION
};

enum AllocationOrigin {
    EXTERNAL_ALLOCATION,
    INTERNAL_ALLOCATION
};

struct AlignedMallocRestrictions {
    uintptr_t minAddress;
};

constexpr size_t paddingBufferSize = 2 * MemoryConstants::megaByte;

class AllocationsList : public IDList<GraphicsAllocation, true, true> {
  public:
    std::unique_ptr<GraphicsAllocation> detachAllocation(size_t requiredMinimalSize, volatile uint32_t *csrTagAddress, bool internalAllocationRequired);

  private:
    GraphicsAllocation *detachAllocationImpl(GraphicsAllocation *, void *);
};

class Gmm;
struct ImageInfo;

class MemoryManager {
  public:
    enum AllocationStatus {
        Success = 0,
        Error,
        InvalidHostPointer,
    };

    MemoryManager(bool enable64kbpages);

    virtual ~MemoryManager();
    MOCKABLE_VIRTUAL void *allocateSystemMemory(size_t size, size_t alignment);

    virtual GraphicsAllocation *allocateGraphicsMemory(size_t size) {
        return allocateGraphicsMemory(size, static_cast<size_t>(0u));
    }

    virtual GraphicsAllocation *allocateGraphicsMemory(size_t size, size_t alignment) {
        return allocateGraphicsMemory(size, alignment, false, false);
    }

    virtual GraphicsAllocation *allocateGraphicsMemory(size_t size, size_t alignment, bool forcePin, bool uncacheable) = 0;

    virtual GraphicsAllocation *allocateGraphicsMemory64kb(size_t size, size_t alignment, bool forcePin) = 0;

    virtual GraphicsAllocation *allocateGraphicsMemory(size_t size, const void *ptr) {
        return MemoryManager::allocateGraphicsMemory(size, ptr, false);
    }
    virtual GraphicsAllocation *allocateGraphicsMemory(size_t size, const void *ptr, bool forcePin);

    virtual GraphicsAllocation *allocate32BitGraphicsMemory(size_t size, void *ptr, AllocationOrigin allocationOrigin) = 0;

    virtual GraphicsAllocation *allocateGraphicsMemoryForImage(ImageInfo &imgInfo, Gmm *gmm) = 0;

    GraphicsAllocation *allocateGraphicsMemoryForSVM(size_t size, bool coherent);

    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, bool requireSpecificBitness) {
        return createGraphicsAllocationFromSharedHandle(handle, requireSpecificBitness, false);
    }

    virtual GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, bool requireSpecificBitness, bool reuseBO) = 0;

    virtual GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle) = 0;

    virtual bool mapAuxGpuVA(GraphicsAllocation *graphicsAllocation) { return false; };

    virtual void *lockResource(GraphicsAllocation *graphicsAllocation) = 0;
    virtual void unlockResource(GraphicsAllocation *graphicsAllocation) = 0;

    void cleanGraphicsMemoryCreatedFromHostPtr(GraphicsAllocation *);
    GraphicsAllocation *createGraphicsAllocationWithPadding(GraphicsAllocation *inputGraphicsAllocation, size_t sizeWithPadding);
    virtual GraphicsAllocation *createPaddedAllocation(GraphicsAllocation *inputGraphicsAllocation, size_t sizeWithPadding);

    virtual AllocationStatus populateOsHandles(OsHandleStorage &handleStorage) = 0;
    virtual void cleanOsHandles(OsHandleStorage &handleStorage) = 0;

    void freeSystemMemory(void *ptr);

    virtual void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) = 0;

    void freeGraphicsMemory(GraphicsAllocation *gfxAllocation);

    void checkGpuUsageAndDestroyGraphicsAllocations(GraphicsAllocation *gfxAllocation);

    void freeGmm(GraphicsAllocation *gfxAllocation);

    virtual uint64_t getSystemSharedMemory() = 0;

    virtual uint64_t getMaxApplicationAddress() = 0;

    virtual uint64_t getInternalHeapBaseAddress() = 0;

    virtual bool cleanAllocationList(uint32_t waitTaskCount, uint32_t allocationType);

    void freeAllocationsList(uint32_t waitTaskCount, AllocationsList &allocationsList);

    void storeAllocation(std::unique_ptr<GraphicsAllocation> gfxAllocation, uint32_t allocationType);
    void storeAllocation(std::unique_ptr<GraphicsAllocation> gfxAllocation, uint32_t allocationType, uint32_t taskCount);

    RequirementsStatus checkAllocationsForOverlapping(AllocationRequirements *requirements, CheckedFragments *checkedFragments);

    TagAllocator<HwTimeStamps> *getEventTsAllocator();
    TagAllocator<HwPerfCounter> *getEventPerfCountAllocator();

    std::unique_ptr<GraphicsAllocation> obtainReusableAllocation(size_t requiredSize, bool isInternalAllocationRequired);

    //intrusive list of allocation
    AllocationsList graphicsAllocations;

    //intrusive list of allocation for re-use
    AllocationsList allocationsForReuse;

    CommandStreamReceiver *csr = nullptr;
    Device *device = nullptr;
    HostPtrManager hostPtrManager;

    virtual GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, size_t hostPtrSize, const void *hostPtr) = 0;

    bool peekForce32BitAllocations() { return force32bitAllocations; }
    void setForce32BitAllocations(bool newValue);

    GraphicsAllocation *createGraphicsAllocationWithRequiredBitness(size_t size, void *ptr) {
        return createGraphicsAllocationWithRequiredBitness(size, ptr, false);
    }

    MOCKABLE_VIRTUAL GraphicsAllocation *createGraphicsAllocationWithRequiredBitness(size_t size, void *ptr, bool forcePin) {
        if (force32bitAllocations && is64bit) {
            return allocate32BitGraphicsMemory(size, ptr, AllocationOrigin::EXTERNAL_ALLOCATION);
        } else {
            if (ptr) {
                return allocateGraphicsMemory(size, ptr, forcePin);
            }
            if (enable64kbpages) {
                return allocateGraphicsMemory64kb(size, MemoryConstants::pageSize64k, forcePin);
            } else {
                return allocateGraphicsMemory(size, MemoryConstants::pageSize, forcePin, false);
            }
        }
    }

    std::unique_ptr<Allocator32bit> allocator32Bit;

    bool peekVirtualPaddingSupport() { return virtualPaddingAvailable; }
    void setVirtualPaddingSupport(bool virtualPaddingSupport) { virtualPaddingAvailable = virtualPaddingSupport; }
    GraphicsAllocation *peekPaddingAllocation() { return paddingAllocation; }

    void pushAllocationForResidency(GraphicsAllocation *gfxAllocation);
    void clearResidencyAllocations();
    ResidencyContainer &getResidencyAllocations() {
        return residencyAllocations;
    }
    void pushAllocationForEviction(GraphicsAllocation *gfxAllocation);
    void clearEvictionAllocations();
    ResidencyContainer &getEvictionAllocations() {
        return evictionAllocations;
    }

    DeferredDeleter *getDeferredDeleter() const {
        return deferredDeleter.get();
    }

    void waitForDeletions();

    bool isAsyncDeleterEnabled() const;
    virtual bool isMemoryBudgetExhausted() const;

    virtual AlignedMallocRestrictions *getAlignedMallocRestrictions() {
        return nullptr;
    }

    MOCKABLE_VIRTUAL void *alignedMallocWrapper(size_t bytes, size_t alignment) {
        return ::alignedMalloc(bytes, alignment);
    }

    MOCKABLE_VIRTUAL void alignedFreeWrapper(void *ptr) {
        ::alignedFree(ptr);
    }

  protected:
    std::recursive_mutex mtx;
    std::unique_ptr<TagAllocator<HwTimeStamps>> profilingTimeStampAllocator;
    std::unique_ptr<TagAllocator<HwPerfCounter>> perfCounterAllocator;
    bool force32bitAllocations = false;
    bool virtualPaddingAvailable = false;
    GraphicsAllocation *paddingAllocation = nullptr;
    void applyCommonCleanup();
    ResidencyContainer residencyAllocations;
    ResidencyContainer evictionAllocations;
    std::unique_ptr<DeferredDeleter> deferredDeleter;
    bool asyncDeleterEnabled = false;
    bool enable64kbpages = false;
};

std::unique_ptr<DeferredDeleter> createDeferredDeleter();
} // namespace OCLRT
