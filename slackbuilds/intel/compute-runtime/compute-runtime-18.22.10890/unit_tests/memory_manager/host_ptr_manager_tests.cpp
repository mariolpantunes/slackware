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

#include "gtest/gtest.h"
#include "runtime/memory_manager/host_ptr_manager.h"
#include "runtime/helpers/ptr_math.h"

using namespace OCLRT;

TEST(HostPtrManager, AlignedPointerAndAlignedSizeAskedForAllocationCountReturnsOne) {
    auto size = MemoryConstants::pageSize * 10;
    void *ptr = (void *)0x1000;

    AllocationRequirements reqs = HostPtrManager::getAllocationRequirements(ptr, size);

    EXPECT_EQ(1u, reqs.requiredFragmentsCount);
    EXPECT_EQ(reqs.AllocationFragments[0].allocationType, MIDDLE);
    EXPECT_EQ(reqs.AllocationFragments[1].allocationType, NONE);
    EXPECT_EQ(reqs.AllocationFragments[2].allocationType, NONE);

    EXPECT_EQ(reqs.totalRequiredSize, size);

    EXPECT_EQ(ptr, reqs.AllocationFragments[0].allocationPtr);
    EXPECT_EQ(size, reqs.AllocationFragments[0].allocationSize);

    EXPECT_EQ(nullptr, reqs.AllocationFragments[1].allocationPtr);
    EXPECT_EQ(nullptr, reqs.AllocationFragments[2].allocationPtr);
}

TEST(HostPtrManager, AlignedPointerAndNotAlignedSizeAskedForAllocationCountReturnsTwo) {
    auto size = MemoryConstants::pageSize * 10 - 1;
    void *ptr = (void *)0x1000;

    AllocationRequirements reqs = HostPtrManager::getAllocationRequirements(ptr, size);
    EXPECT_EQ(2u, reqs.requiredFragmentsCount);

    EXPECT_EQ(reqs.AllocationFragments[0].allocationType, MIDDLE);
    EXPECT_EQ(reqs.AllocationFragments[1].allocationType, TRAILING);
    EXPECT_EQ(reqs.AllocationFragments[2].allocationType, NONE);
    EXPECT_EQ(reqs.totalRequiredSize, alignUp(size, MemoryConstants::pageSize));

    EXPECT_EQ(ptr, reqs.AllocationFragments[0].allocationPtr);
    EXPECT_EQ(9 * MemoryConstants::pageSize, reqs.AllocationFragments[0].allocationSize);

    auto trailingPtr = alignDown(ptrOffset(ptr, size), MemoryConstants::pageSize);
    EXPECT_EQ(trailingPtr, reqs.AllocationFragments[1].allocationPtr);
    EXPECT_EQ(MemoryConstants::pageSize, reqs.AllocationFragments[1].allocationSize);

    EXPECT_EQ(nullptr, reqs.AllocationFragments[2].allocationPtr);
    EXPECT_EQ(0u, reqs.AllocationFragments[2].allocationSize);
}

TEST(HostPtrManager, NotAlignedPointerAndNotAlignedSizeAskedForAllocationCountReturnsThree) {
    auto size = MemoryConstants::pageSize * 10 - 1;
    void *ptr = (void *)0x1045;

    AllocationRequirements reqs = HostPtrManager::getAllocationRequirements(ptr, size);
    EXPECT_EQ(3u, reqs.requiredFragmentsCount);

    EXPECT_EQ(reqs.AllocationFragments[0].allocationType, LEADING);
    EXPECT_EQ(reqs.AllocationFragments[1].allocationType, MIDDLE);
    EXPECT_EQ(reqs.AllocationFragments[2].allocationType, TRAILING);

    auto leadingPtr = (void *)0x1000;
    auto middlePtr = (void *)0x2000;
    auto trailingPtr = (void *)0xb000;

    EXPECT_EQ(reqs.totalRequiredSize, 11 * MemoryConstants::pageSize);

    EXPECT_EQ(leadingPtr, reqs.AllocationFragments[0].allocationPtr);
    EXPECT_EQ(MemoryConstants::pageSize, reqs.AllocationFragments[0].allocationSize);

    EXPECT_EQ(middlePtr, reqs.AllocationFragments[1].allocationPtr);
    EXPECT_EQ(9 * MemoryConstants::pageSize, reqs.AllocationFragments[1].allocationSize);

    EXPECT_EQ(trailingPtr, reqs.AllocationFragments[2].allocationPtr);
    EXPECT_EQ(MemoryConstants::pageSize, reqs.AllocationFragments[2].allocationSize);
}

TEST(HostPtrManager, NotAlignedPointerAndNotAlignedSizeWithinOnePageAskedForAllocationCountReturnsOne) {
    auto size = 200;
    void *ptr = (void *)0x1045;

    AllocationRequirements reqs = HostPtrManager::getAllocationRequirements(ptr, size);
    EXPECT_EQ(1u, reqs.requiredFragmentsCount);

    EXPECT_EQ(reqs.AllocationFragments[0].allocationType, LEADING);
    EXPECT_EQ(reqs.AllocationFragments[1].allocationType, NONE);
    EXPECT_EQ(reqs.AllocationFragments[2].allocationType, NONE);

    auto leadingPtr = (void *)0x1000;

    EXPECT_EQ(reqs.totalRequiredSize, MemoryConstants::pageSize);

    EXPECT_EQ(leadingPtr, reqs.AllocationFragments[0].allocationPtr);
    EXPECT_EQ(MemoryConstants::pageSize, reqs.AllocationFragments[0].allocationSize);

    EXPECT_EQ(nullptr, reqs.AllocationFragments[1].allocationPtr);
    EXPECT_EQ(0u, reqs.AllocationFragments[1].allocationSize);

    EXPECT_EQ(nullptr, reqs.AllocationFragments[2].allocationPtr);
    EXPECT_EQ(0u, reqs.AllocationFragments[2].allocationSize);
}

TEST(HostPtrManager, NotAlignedPointerAndNotAlignedSizeWithinTwoPagesAskedForAllocationCountReturnsTwo) {
    auto size = MemoryConstants::pageSize;
    void *ptr = (void *)0x1045;

    AllocationRequirements reqs = HostPtrManager::getAllocationRequirements(ptr, size);
    EXPECT_EQ(2u, reqs.requiredFragmentsCount);

    EXPECT_EQ(reqs.AllocationFragments[0].allocationType, LEADING);
    EXPECT_EQ(reqs.AllocationFragments[1].allocationType, TRAILING);
    EXPECT_EQ(reqs.AllocationFragments[2].allocationType, NONE);

    auto leadingPtr = (void *)0x1000;
    auto trailingPtr = (void *)0x2000;

    EXPECT_EQ(reqs.totalRequiredSize, 2 * MemoryConstants::pageSize);

    EXPECT_EQ(leadingPtr, reqs.AllocationFragments[0].allocationPtr);
    EXPECT_EQ(MemoryConstants::pageSize, reqs.AllocationFragments[0].allocationSize);

    EXPECT_EQ(trailingPtr, reqs.AllocationFragments[1].allocationPtr);
    EXPECT_EQ(MemoryConstants::pageSize, reqs.AllocationFragments[1].allocationSize);

    EXPECT_EQ(nullptr, reqs.AllocationFragments[2].allocationPtr);
    EXPECT_EQ(0u, reqs.AllocationFragments[2].allocationSize);
}

TEST(HostPtrManager, AlignedPointerAndAlignedSizeOfOnePageAskedForAllocationCountReturnsMiddleOnly) {
    auto size = MemoryConstants::pageSize * 10;
    void *ptr = (void *)0x1000;

    AllocationRequirements reqs = HostPtrManager::getAllocationRequirements(ptr, size);
    EXPECT_EQ(1u, reqs.requiredFragmentsCount);

    EXPECT_EQ(reqs.AllocationFragments[0].allocationType, MIDDLE);
    EXPECT_EQ(reqs.AllocationFragments[1].allocationType, NONE);
    EXPECT_EQ(reqs.AllocationFragments[2].allocationType, NONE);

    auto middlePtr = (void *)0x1000;

    EXPECT_EQ(reqs.totalRequiredSize, 10 * MemoryConstants::pageSize);

    EXPECT_EQ(middlePtr, reqs.AllocationFragments[0].allocationPtr);
    EXPECT_EQ(10 * MemoryConstants::pageSize, reqs.AllocationFragments[0].allocationSize);

    EXPECT_EQ(nullptr, reqs.AllocationFragments[1].allocationPtr);
    EXPECT_EQ(0u, reqs.AllocationFragments[1].allocationSize);

    EXPECT_EQ(nullptr, reqs.AllocationFragments[2].allocationPtr);
    EXPECT_EQ(0u, reqs.AllocationFragments[2].allocationSize);
}

TEST(HostPtrManager, NotAlignedPointerAndSizeThatFitsToPageAskedForAllocationCountReturnsMiddleAndLeading) {
    auto size = MemoryConstants::pageSize * 10 - 1;
    void *ptr = (void *)0x1001;

    AllocationRequirements reqs = HostPtrManager::getAllocationRequirements(ptr, size);
    EXPECT_EQ(2u, reqs.requiredFragmentsCount);

    EXPECT_EQ(reqs.AllocationFragments[0].allocationType, LEADING);
    EXPECT_EQ(reqs.AllocationFragments[1].allocationType, MIDDLE);
    EXPECT_EQ(reqs.AllocationFragments[2].allocationType, NONE);

    auto leadingPtr = (void *)0x1000;
    auto middlePtr = (void *)0x2000;

    EXPECT_EQ(reqs.totalRequiredSize, 10 * MemoryConstants::pageSize);

    EXPECT_EQ(leadingPtr, reqs.AllocationFragments[0].allocationPtr);
    EXPECT_EQ(MemoryConstants::pageSize, reqs.AllocationFragments[0].allocationSize);

    EXPECT_EQ(middlePtr, reqs.AllocationFragments[1].allocationPtr);
    EXPECT_EQ(9 * MemoryConstants::pageSize, reqs.AllocationFragments[1].allocationSize);

    EXPECT_EQ(nullptr, reqs.AllocationFragments[2].allocationPtr);
    EXPECT_EQ(0u, reqs.AllocationFragments[2].allocationSize);
}

TEST(HostPtrManager, AlignedPointerAndPageSizeAskedForAllocationCountRetrunsMiddle) {
    auto size = MemoryConstants::pageSize;
    void *ptr = (void *)0x1000;

    AllocationRequirements reqs = HostPtrManager::getAllocationRequirements(ptr, size);
    EXPECT_EQ(1u, reqs.requiredFragmentsCount);

    EXPECT_EQ(reqs.AllocationFragments[0].allocationType, MIDDLE);
    EXPECT_EQ(reqs.AllocationFragments[1].allocationType, NONE);
    EXPECT_EQ(reqs.AllocationFragments[2].allocationType, NONE);

    auto middlePtr = (void *)0x1000;

    EXPECT_EQ(reqs.totalRequiredSize, MemoryConstants::pageSize);

    EXPECT_EQ(middlePtr, reqs.AllocationFragments[0].allocationPtr);
    EXPECT_EQ(MemoryConstants::pageSize, reqs.AllocationFragments[0].allocationSize);

    EXPECT_EQ(nullptr, reqs.AllocationFragments[1].allocationPtr);
    EXPECT_EQ(0u, reqs.AllocationFragments[1].allocationSize);

    EXPECT_EQ(nullptr, reqs.AllocationFragments[2].allocationPtr);
    EXPECT_EQ(0u, reqs.AllocationFragments[2].allocationSize);
}

TEST(HostPtrManager, AllocationRequirementsForMiddleAllocationThatIsNotStoredInManagerAskedForGraphicsAllocationReturnsNotAvailable) {
    auto size = MemoryConstants::pageSize;
    void *ptr = (void *)0x1000;
    auto reqs = HostPtrManager::getAllocationRequirements(ptr, size);
    HostPtrManager hostPtrManager;

    auto gpuAllocationFragments = hostPtrManager.populateAlreadyAllocatedFragments(reqs, nullptr);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[0].osHandleStorage);
    EXPECT_EQ(ptr, gpuAllocationFragments.fragmentStorageData[0].cpuPtr);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[1].cpuPtr);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[2].osHandleStorage);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[2].cpuPtr);
}

TEST(HostPtrManager, AllocationRequirementsForMiddleAllocationThatIsStoredInManagerAskedForGraphicsAllocationReturnsProperAllocationAndIncreasesRefCount) {

    HostPtrManager hostPtrManager;
    FragmentStorage allocationFragment;
    auto cpuPtr = (void *)0x1000;
    auto ptrSize = MemoryConstants::pageSize;
    auto osInternalStorage = (OsHandle *)0x12312;
    allocationFragment.fragmentCpuPointer = cpuPtr;
    allocationFragment.fragmentSize = ptrSize;
    allocationFragment.osInternalStorage = osInternalStorage;

    hostPtrManager.storeFragment(allocationFragment);

    auto reqs = HostPtrManager::getAllocationRequirements(cpuPtr, ptrSize);

    auto gpuAllocationFragments = hostPtrManager.populateAlreadyAllocatedFragments(reqs, nullptr);

    EXPECT_EQ(osInternalStorage, gpuAllocationFragments.fragmentStorageData[0].osHandleStorage);
    EXPECT_EQ(cpuPtr, gpuAllocationFragments.fragmentStorageData[0].cpuPtr);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[1].cpuPtr);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[2].osHandleStorage);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[2].cpuPtr);

    auto fragment = hostPtrManager.getFragment(cpuPtr);
    EXPECT_EQ(2, fragment->refCount);
}

TEST(HostPtrManager, AllocationRequirementsForAllocationWithinSizeOfStoredAllocationInManagerAskedForGraphicsAllocationReturnsProperAllocation) {

    HostPtrManager hostPtrManager;
    FragmentStorage allocationFragment;
    auto cpuPtr = (void *)0x1000;
    auto ptrSize = MemoryConstants::pageSize * 10;
    auto osInternalStorage = (OsHandle *)0x12312;
    allocationFragment.fragmentCpuPointer = cpuPtr;
    allocationFragment.fragmentSize = ptrSize;
    allocationFragment.osInternalStorage = osInternalStorage;

    hostPtrManager.storeFragment(allocationFragment);

    auto reqs = HostPtrManager::getAllocationRequirements(cpuPtr, MemoryConstants::pageSize);

    auto gpuAllocationFragments = hostPtrManager.populateAlreadyAllocatedFragments(reqs, nullptr);

    EXPECT_EQ(osInternalStorage, gpuAllocationFragments.fragmentStorageData[0].osHandleStorage);
    EXPECT_EQ(cpuPtr, gpuAllocationFragments.fragmentStorageData[0].cpuPtr);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[1].cpuPtr);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[2].osHandleStorage);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[2].cpuPtr);

    auto fragment = hostPtrManager.getFragment(cpuPtr);
    EXPECT_EQ(2, fragment->refCount);
}

TEST(HostPtrManager, HostPtrAndSizeStoredToHostPtrManagerIncreasesTheContainerCount) {
    HostPtrManager hostPtrManager;

    FragmentStorage allocationFragment;
    EXPECT_EQ(allocationFragment.fragmentCpuPointer, nullptr);
    EXPECT_EQ(allocationFragment.fragmentSize, 0u);
    EXPECT_EQ(allocationFragment.refCount, 0);

    hostPtrManager.storeFragment(allocationFragment);

    EXPECT_EQ(1u, hostPtrManager.getFragmentCount());
}

TEST(HostPtrManager, HostPtrAndSizeStoredToHostPtrManagerTwiceReturnsOneAsFragmentCount) {
    HostPtrManager hostPtrManager;

    FragmentStorage allocationFragment;

    hostPtrManager.storeFragment(allocationFragment);
    hostPtrManager.storeFragment(allocationFragment);

    EXPECT_EQ(1u, hostPtrManager.getFragmentCount());
}

TEST(HostPtrManager, EmptyHostPtrManagerAskedForFragmentReturnsNullptr) {
    HostPtrManager hostPtrManager;
    auto fragment = hostPtrManager.getFragment((void *)0x10121);
    EXPECT_EQ(nullptr, fragment);
    EXPECT_EQ(0u, hostPtrManager.getFragmentCount());
}

TEST(HostPtrManager, NonEmptyHostPtrManagerAskedForFragmentReturnsProperFragmentWithRefCountOne) {
    HostPtrManager hostPtrManager;
    FragmentStorage fragment;
    void *cpuPtr = (void *)0x10121;
    auto fragmentSize = 101u;
    fragment.fragmentCpuPointer = cpuPtr;
    fragment.fragmentSize = fragmentSize;
    fragment.refCount = 0;

    hostPtrManager.storeFragment(fragment);
    auto retFragment = hostPtrManager.getFragment(cpuPtr);

    EXPECT_NE(retFragment, &fragment);
    EXPECT_EQ(1, retFragment->refCount);
    EXPECT_EQ(cpuPtr, retFragment->fragmentCpuPointer);
    EXPECT_EQ(fragmentSize, retFragment->fragmentSize);
    EXPECT_EQ(1u, hostPtrManager.getFragmentCount());
}

TEST(HostPtrManager, HostPtrManagerFilledTwiceWithTheSamePointerWhenAskedForFragmentReturnsItWithRefCountSetToTwo) {
    HostPtrManager hostPtrManager;
    FragmentStorage fragment;
    void *cpuPtr = (void *)0x10121;
    auto fragmentSize = 101u;
    fragment.fragmentCpuPointer = cpuPtr;
    fragment.fragmentSize = fragmentSize;
    fragment.refCount = 0;

    hostPtrManager.storeFragment(fragment);
    hostPtrManager.storeFragment(fragment);
    auto retFragment = hostPtrManager.getFragment(cpuPtr);

    EXPECT_NE(retFragment, &fragment);
    EXPECT_EQ(2, retFragment->refCount);
    EXPECT_EQ(cpuPtr, retFragment->fragmentCpuPointer);
    EXPECT_EQ(fragmentSize, retFragment->fragmentSize);
    EXPECT_EQ(1u, hostPtrManager.getFragmentCount());
}

TEST(HostPtrManager, GivenHostPtrManagerFilledWithFragmentsWhenFragmentIsBeingReleasedThenManagerMaintainsProperRefferenceCount) {
    HostPtrManager hostPtrManager;
    FragmentStorage fragment;
    void *cpuPtr = (void *)0x1000;
    auto fragmentSize = MemoryConstants::pageSize;

    fragment.fragmentCpuPointer = cpuPtr;
    fragment.fragmentSize = fragmentSize;

    hostPtrManager.storeFragment(fragment);
    hostPtrManager.storeFragment(fragment);
    ASSERT_EQ(1u, hostPtrManager.getFragmentCount());

    auto fragmentReadyForRelease = hostPtrManager.releaseHostPtr(cpuPtr);
    EXPECT_FALSE(fragmentReadyForRelease);

    auto retFragment = hostPtrManager.getFragment(cpuPtr);

    EXPECT_EQ(1, retFragment->refCount);

    fragmentReadyForRelease = hostPtrManager.releaseHostPtr(cpuPtr);

    EXPECT_TRUE(fragmentReadyForRelease);

    retFragment = hostPtrManager.getFragment(cpuPtr);

    EXPECT_EQ(nullptr, retFragment);
    EXPECT_EQ(0u, hostPtrManager.getFragmentCount());
}
TEST(HostPtrManager, GivenOsHandleStorageWhenAskedToStoreTheFragmentThenFragmentIsStoredProperly) {
    OsHandleStorage storage;
    void *cpu1 = (void *)0x1000;
    void *cpu2 = (void *)0x2000;

    auto size1 = MemoryConstants::pageSize;
    auto size2 = MemoryConstants::pageSize * 2;

    storage.fragmentStorageData[0].cpuPtr = cpu1;
    storage.fragmentStorageData[0].fragmentSize = size1;

    storage.fragmentStorageData[1].cpuPtr = cpu2;
    storage.fragmentStorageData[1].fragmentSize = size2;

    HostPtrManager hostPtrManager;

    EXPECT_EQ(0u, hostPtrManager.getFragmentCount());

    hostPtrManager.storeFragment(storage.fragmentStorageData[0]);
    hostPtrManager.storeFragment(storage.fragmentStorageData[1]);

    EXPECT_EQ(2u, hostPtrManager.getFragmentCount());

    hostPtrManager.releaseHandleStorage(storage);

    EXPECT_EQ(0u, hostPtrManager.getFragmentCount());
}

TEST(HostPtrManager, GivenHostPtrFilledWith3TripleFragmentsWhenAskedForPopulationThenAllFragmentsAreResued) {
    void *cpuPtr = (void *)0x1001;
    auto fragmentSize = MemoryConstants::pageSize * 10;

    HostPtrManager hostPtrManager;

    auto reqs = hostPtrManager.getAllocationRequirements(cpuPtr, fragmentSize);
    ASSERT_EQ(3u, reqs.requiredFragmentsCount);

    FragmentStorage fragments[max_fragments_count];
    //check all fragments
    for (int i = 0; i < max_fragments_count; i++) {
        fragments[i].fragmentCpuPointer = const_cast<void *>(reqs.AllocationFragments[i].allocationPtr);
        fragments[i].fragmentSize = reqs.AllocationFragments[i].allocationSize;
        hostPtrManager.storeFragment(fragments[i]);
    }

    EXPECT_EQ(3u, hostPtrManager.getFragmentCount());

    auto OsHandles = hostPtrManager.populateAlreadyAllocatedFragments(reqs, nullptr);

    EXPECT_EQ(3u, hostPtrManager.getFragmentCount());
    for (int i = 0; i < max_fragments_count; i++) {
        EXPECT_EQ(OsHandles.fragmentStorageData[i].cpuPtr, reqs.AllocationFragments[i].allocationPtr);
        EXPECT_EQ(OsHandles.fragmentStorageData[i].fragmentSize, reqs.AllocationFragments[i].allocationSize);
        auto fragment = hostPtrManager.getFragment(const_cast<void *>(reqs.AllocationFragments[i].allocationPtr));
        ASSERT_NE(nullptr, fragment);
        EXPECT_EQ(2, fragment->refCount);
        EXPECT_EQ(OsHandles.fragmentStorageData[i].cpuPtr, fragment->fragmentCpuPointer);
    }

    for (int i = 0; i < max_fragments_count; i++) {
        hostPtrManager.releaseHostPtr(fragments[i].fragmentCpuPointer);
    }
    EXPECT_EQ(3u, hostPtrManager.getFragmentCount());
    for (int i = 0; i < max_fragments_count; i++) {
        auto fragment = hostPtrManager.getFragment(const_cast<void *>(reqs.AllocationFragments[i].allocationPtr));
        ASSERT_NE(nullptr, fragment);
        EXPECT_EQ(1, fragment->refCount);
    }
    for (int i = 0; i < max_fragments_count; i++) {
        hostPtrManager.releaseHostPtr(fragments[i].fragmentCpuPointer);
    }
    EXPECT_EQ(0u, hostPtrManager.getFragmentCount());
}

TEST(HostPtrManager, FragmentFindWhenFragmentSizeIsZero) {
    HostPtrManager hostPtrManager;

    auto ptr1 = (void *)0x010000;
    FragmentStorage fragment1;
    fragment1.fragmentCpuPointer = ptr1;
    fragment1.fragmentSize = 0;
    hostPtrManager.storeFragment(fragment1);

    auto ptr2 = (void *)0x040000;
    FragmentStorage fragment2;
    fragment2.fragmentCpuPointer = ptr2;
    fragment2.fragmentSize = 0;
    hostPtrManager.storeFragment(fragment2);

    auto cptr1 = (void *)0x00F000;
    auto frag1 = hostPtrManager.getFragment(cptr1);
    EXPECT_EQ(frag1, nullptr);

    auto cptr2 = (void *)0x010000;
    auto frag2 = hostPtrManager.getFragment(cptr2);
    EXPECT_NE(frag2, nullptr);

    auto cptr3 = (void *)0x010001;
    auto frag3 = hostPtrManager.getFragment(cptr3);
    EXPECT_EQ(frag3, nullptr);

    auto cptr4 = (void *)0x020000;
    auto frag4 = hostPtrManager.getFragment(cptr4);
    EXPECT_EQ(frag4, nullptr);

    auto cptr5 = (void *)0x040000;
    auto frag5 = hostPtrManager.getFragment(cptr5);
    EXPECT_NE(frag5, nullptr);

    auto cptr6 = (void *)0x040001;
    auto frag6 = hostPtrManager.getFragment(cptr6);
    EXPECT_EQ(frag6, nullptr);

    auto cptr7 = (void *)0x060000;
    auto frag7 = hostPtrManager.getFragment(cptr7);
    EXPECT_EQ(frag7, nullptr);
}

TEST(HostPtrManager, FragmentFindWhenFragmentSizeIsNotZero) {
    HostPtrManager hostPtrManager;

    auto size1 = MemoryConstants::pageSize;

    auto ptr1 = (void *)0x010000;
    FragmentStorage fragment1;
    fragment1.fragmentCpuPointer = ptr1;
    fragment1.fragmentSize = size1;
    hostPtrManager.storeFragment(fragment1);

    auto ptr2 = (void *)0x040000;
    FragmentStorage fragment2;
    fragment2.fragmentCpuPointer = ptr2;
    fragment2.fragmentSize = size1;
    hostPtrManager.storeFragment(fragment2);

    auto cptr1 = (void *)0x010060;
    auto frag1 = hostPtrManager.getFragment(cptr1);
    EXPECT_NE(frag1, nullptr);

    auto cptr2 = (void *)0x020000;
    auto frag2 = hostPtrManager.getFragment(cptr2);
    EXPECT_EQ(frag2, nullptr);

    auto cptr3 = (void *)0x040060;
    auto frag3 = hostPtrManager.getFragment(cptr3);
    EXPECT_NE(frag3, nullptr);

    auto cptr4 = (void *)0x060000;
    auto frag4 = hostPtrManager.getFragment(cptr4);
    EXPECT_EQ(frag4, nullptr);

    AllocationRequirements requiredAllocations;
    auto ptr3 = (void *)0x040000;
    auto size3 = MemoryConstants::pageSize * 2;
    requiredAllocations = hostPtrManager.getAllocationRequirements(ptr3, size3);
    auto catchme = false;
    try {
        OsHandleStorage st = hostPtrManager.populateAlreadyAllocatedFragments(requiredAllocations, nullptr);
        EXPECT_EQ(st.fragmentCount, 0u);
    } catch (...) {
        catchme = true;
    }
    EXPECT_TRUE(catchme);
}

TEST(HostPtrManager, FragmentCheck) {
    HostPtrManager hostPtrManager;

    auto size1 = MemoryConstants::pageSize;

    auto ptr1 = (void *)0x010000;
    FragmentStorage fragment1;
    fragment1.fragmentCpuPointer = ptr1;
    fragment1.fragmentSize = size1;
    hostPtrManager.storeFragment(fragment1);

    auto ptr2 = (void *)0x040000;
    FragmentStorage fragment2;
    fragment2.fragmentCpuPointer = ptr2;
    fragment2.fragmentSize = size1;
    hostPtrManager.storeFragment(fragment2);

    OverlapStatus overlappingStatus;
    auto cptr1 = (void *)0x010060;
    auto frag1 = hostPtrManager.getFragmentAndCheckForOverlaps(cptr1, 1u, overlappingStatus);
    EXPECT_NE(frag1, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_WITHIN_STORED_FRAGMENT);

    frag1 = hostPtrManager.getFragmentAndCheckForOverlaps(ptr1, size1, overlappingStatus);
    EXPECT_NE(frag1, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_WITH_EXACT_SIZE_AS_STORED_FRAGMENT);

    frag1 = hostPtrManager.getFragmentAndCheckForOverlaps(ptr1, size1 - 1, overlappingStatus);
    EXPECT_NE(frag1, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_WITHIN_STORED_FRAGMENT);

    auto cptr2 = (void *)0x020000;
    auto frag2 = hostPtrManager.getFragmentAndCheckForOverlaps(cptr2, 1u, overlappingStatus);
    EXPECT_EQ(frag2, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER);

    auto cptr3 = (void *)0x040060;
    auto frag3 = hostPtrManager.getFragmentAndCheckForOverlaps(cptr3, 1u, overlappingStatus);
    EXPECT_NE(frag3, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_WITHIN_STORED_FRAGMENT);

    auto cptr4 = (void *)0x060000;
    auto frag4 = hostPtrManager.getFragmentAndCheckForOverlaps(cptr4, 1u, overlappingStatus);
    EXPECT_EQ(frag4, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER);

    auto cptr5 = (void *)0x040000;
    auto frag5 = hostPtrManager.getFragmentAndCheckForOverlaps(cptr5, size1 - 1, overlappingStatus);
    EXPECT_NE(frag5, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_WITHIN_STORED_FRAGMENT);

    auto cptr6 = (void *)0x040000;
    auto frag6 = hostPtrManager.getFragmentAndCheckForOverlaps(cptr6, size1 + 1, overlappingStatus);
    EXPECT_EQ(frag6, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT);

    auto cptr7 = (void *)0x03FFF0;
    auto frag7 = hostPtrManager.getFragmentAndCheckForOverlaps(cptr7, 2 * size1, overlappingStatus);
    EXPECT_EQ(frag7, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT);

    auto cptr8 = (void *)0x040000;
    auto frag8 = hostPtrManager.getFragmentAndCheckForOverlaps(cptr8, size1, overlappingStatus);
    EXPECT_NE(frag8, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_WITH_EXACT_SIZE_AS_STORED_FRAGMENT);

    auto cptr9 = (void *)0x010060;
    auto frag9 = hostPtrManager.getFragmentAndCheckForOverlaps(cptr9, 2 * size1, overlappingStatus);
    EXPECT_EQ(frag9, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT);
}

TEST(HostPtrManager, GivenHostPtrManagerFilledWithBigFragmentWhenAskedForFragmnetInTheMiddleOfBigFragmentThenBigFragmentIsReturned) {
    auto bigSize = 10 * MemoryConstants::pageSize;
    auto bigPtr = (void *)0x01000;
    FragmentStorage fragment;
    fragment.fragmentCpuPointer = bigPtr;
    fragment.fragmentSize = bigSize;
    HostPtrManager hostPtrManager;
    hostPtrManager.storeFragment(fragment);
    EXPECT_EQ(1u, hostPtrManager.getFragmentCount());

    auto ptrInTheMiddle = (void *)0x2000;
    auto smallSize = MemoryConstants::pageSize;

    auto storedBigFragment = hostPtrManager.getFragment(bigPtr);

    auto fragment2 = hostPtrManager.getFragment(ptrInTheMiddle);
    EXPECT_EQ(storedBigFragment, fragment2);

    OverlapStatus overlapStatus;
    auto fragment3 = hostPtrManager.getFragmentAndCheckForOverlaps(ptrInTheMiddle, smallSize, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_WITHIN_STORED_FRAGMENT, overlapStatus);
    EXPECT_EQ(fragment3, storedBigFragment);

    auto ptrOutside = (void *)0x1000000;
    auto outsideSize = 1;

    auto perfectMatchFragment = hostPtrManager.getFragmentAndCheckForOverlaps(bigPtr, bigSize, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_WITH_EXACT_SIZE_AS_STORED_FRAGMENT, overlapStatus);
    EXPECT_EQ(perfectMatchFragment, storedBigFragment);

    auto oustideFragment = hostPtrManager.getFragmentAndCheckForOverlaps(ptrOutside, outsideSize, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER, overlapStatus);
    EXPECT_EQ(nullptr, oustideFragment);

    //partialOverlap
    auto ptrPartial = (void *)(((uintptr_t)bigPtr + bigSize) - 100);
    auto partialBigSize = MemoryConstants::pageSize * 100;

    auto partialFragment = hostPtrManager.getFragmentAndCheckForOverlaps(ptrPartial, partialBigSize, overlapStatus);
    EXPECT_EQ(nullptr, partialFragment);
    EXPECT_EQ(OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT, overlapStatus);
}
TEST(HostPtrManager, GivenHostPtrManagerFilledWithFragmentsWhenCheckedForOverlappingThenProperOverlappingStatusIsReturned) {
    auto bigPtr = (void *)0x04000;
    auto bigSize = 10 * MemoryConstants::pageSize;
    FragmentStorage fragment;
    fragment.fragmentCpuPointer = bigPtr;
    fragment.fragmentSize = bigSize;
    HostPtrManager hostPtrManager;
    hostPtrManager.storeFragment(fragment);
    EXPECT_EQ(1u, hostPtrManager.getFragmentCount());

    auto ptrNonOverlapingPriorToBigPtr = (void *)0x2000;
    auto smallSize = MemoryConstants::pageSize;

    OverlapStatus overlapStatus;
    auto fragment2 = hostPtrManager.getFragmentAndCheckForOverlaps(ptrNonOverlapingPriorToBigPtr, smallSize, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER, overlapStatus);
    EXPECT_EQ(nullptr, fragment2);

    auto ptrNonOverlapingPriorToBigPtrByPage = (void *)0x3000;
    auto checkMatch = (uintptr_t)ptrNonOverlapingPriorToBigPtrByPage + smallSize;
    EXPECT_EQ(checkMatch, (uintptr_t)bigPtr);

    auto fragment3 = hostPtrManager.getFragmentAndCheckForOverlaps(ptrNonOverlapingPriorToBigPtrByPage, smallSize, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER, overlapStatus);
    EXPECT_EQ(nullptr, fragment3);

    auto fragment4 = hostPtrManager.getFragmentAndCheckForOverlaps(ptrNonOverlapingPriorToBigPtrByPage, smallSize + 1, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT, overlapStatus);
    EXPECT_EQ(nullptr, fragment4);
}
TEST(HostPtrManager, GivenEmptyHostPtrManagerWhenAskedForOverlapingThenNoOverlappingIsReturned) {
    HostPtrManager hostPtrManager;
    auto bigPtr = (void *)0x04000;
    auto bigSize = 10 * MemoryConstants::pageSize;

    OverlapStatus overlapStatus;
    auto fragment3 = hostPtrManager.getFragmentAndCheckForOverlaps(bigPtr, bigSize, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER, overlapStatus);
    EXPECT_EQ(nullptr, fragment3);
}

TEST(HostPtrManager, GivenHostPtrManagerFilledWithFragmentsWhenAskedForOverlpaingThenProperStatusIsReturned) {
    auto bigPtr1 = (void *)0x01000;
    auto bigPtr2 = (void *)0x03000;
    auto bigSize = MemoryConstants::pageSize;
    FragmentStorage fragment;
    fragment.fragmentCpuPointer = bigPtr1;
    fragment.fragmentSize = bigSize;
    HostPtrManager hostPtrManager;
    hostPtrManager.storeFragment(fragment);
    fragment.fragmentCpuPointer = bigPtr2;
    hostPtrManager.storeFragment(fragment);
    EXPECT_EQ(2u, hostPtrManager.getFragmentCount());

    auto ptrNonOverlapingInTheMiddleOfBigPtrs = (void *)0x2000;
    auto ptrNonOverlapingAfterBigPtr = (void *)0x4000;
    auto ptrNonOverlapingBeforeBigPtr = (void *)0;
    OverlapStatus overlapStatus;
    auto fragment1 = hostPtrManager.getFragmentAndCheckForOverlaps(ptrNonOverlapingInTheMiddleOfBigPtrs, bigSize, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER, overlapStatus);
    EXPECT_EQ(nullptr, fragment1);

    auto fragment2 = hostPtrManager.getFragmentAndCheckForOverlaps(ptrNonOverlapingInTheMiddleOfBigPtrs, bigSize * 5, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT, overlapStatus);
    EXPECT_EQ(nullptr, fragment2);

    auto fragment3 = hostPtrManager.getFragmentAndCheckForOverlaps(ptrNonOverlapingAfterBigPtr, bigSize * 5, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER, overlapStatus);
    EXPECT_EQ(nullptr, fragment3);

    auto fragment4 = hostPtrManager.getFragmentAndCheckForOverlaps(ptrNonOverlapingBeforeBigPtr, bigSize, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER, overlapStatus);
    EXPECT_EQ(nullptr, fragment4);
}
TEST(HostPtrManager, GivenHostPtrManagerFilledWithFragmentsWhenAskedForOverlapingThenProperOverlapingStatusIsReturned) {
    auto bigPtr1 = (void *)0x10000;
    auto bigPtr2 = (void *)0x03000;
    auto bigPtr3 = (void *)0x11000;

    auto bigSize1 = MemoryConstants::pageSize;
    auto bigSize2 = MemoryConstants::pageSize * 4;
    auto bigSize3 = MemoryConstants::pageSize * 10;

    FragmentStorage fragment;
    fragment.fragmentCpuPointer = bigPtr1;
    fragment.fragmentSize = bigSize1;
    HostPtrManager hostPtrManager;
    hostPtrManager.storeFragment(fragment);
    fragment.fragmentCpuPointer = bigPtr2;
    fragment.fragmentSize = bigSize2;
    hostPtrManager.storeFragment(fragment);
    fragment.fragmentCpuPointer = bigPtr3;
    fragment.fragmentSize = bigSize3;
    hostPtrManager.storeFragment(fragment);

    EXPECT_EQ(3u, hostPtrManager.getFragmentCount());

    OverlapStatus overlapStatus;
    auto fragment1 = hostPtrManager.getFragmentAndCheckForOverlaps(bigPtr1, bigSize1 + 1, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT, overlapStatus);
    EXPECT_EQ(nullptr, fragment1);

    auto priorToBig1 = (void *)0x9999;

    auto fragment2 = hostPtrManager.getFragmentAndCheckForOverlaps(priorToBig1, 1, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER, overlapStatus);
    EXPECT_EQ(nullptr, fragment2);

    auto middleOfBig3 = (void *)0x11111;
    auto fragment3 = hostPtrManager.getFragmentAndCheckForOverlaps(middleOfBig3, 1, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_WITHIN_STORED_FRAGMENT, overlapStatus);
    EXPECT_NE(nullptr, fragment3);
}
