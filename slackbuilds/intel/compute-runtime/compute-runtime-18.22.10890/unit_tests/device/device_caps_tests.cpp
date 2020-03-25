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

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/device/driver_info.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/os_interface/32bit_memory.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/source_level_debugger/source_level_debugger.h"

#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/hw_helper_tests.h"
#include "unit_tests/mocks/mock_builtins.h"
#include "unit_tests/mocks/mock_device.h"

#include "hw_cmds.h"
#include "test.h"
#include "driver_version.h"

#include <memory>

namespace OCLRT {
extern const char *familyName[];
}

using namespace OCLRT;

typedef Test<DeviceFixture> DeviceGetCapsF;

TEST_F(DeviceGetCapsF, GivenDeviceCapsWhenQueryingForSLMWindowStartAddressThenSharedAndValidPointerGetsReturned) {
    auto adr1 = pDevice->getSLMWindowStartAddress();
    EXPECT_NE(nullptr, adr1);
    auto adr2 = pDevice->getSLMWindowStartAddress();
    EXPECT_EQ(adr2, adr1);
}

TEST_F(DeviceGetCapsF, GivenDeviceCapsWhenCallinThenSharedAndValidPointerGetsReturned) {
    EXPECT_EQ(nullptr, this->pDevice->peekSlmWindowStartAddress());
    this->pDevice->prepareSLMWindow();
    void *preparedAddr = this->pDevice->peekSlmWindowStartAddress();
    EXPECT_NE(nullptr, preparedAddr);
    EXPECT_EQ(preparedAddr, this->pDevice->getSLMWindowStartAddress());
    this->pDevice->prepareSLMWindow();
    EXPECT_EQ(preparedAddr, this->pDevice->peekSlmWindowStartAddress());
}

TEST_F(DeviceGetCapsF, GivenDeviceCapsWhenQueryingForSLMWindowStartAddressThenPointerWithProperAlignmentIsReturned) {
    auto addr = reinterpret_cast<uintptr_t>(pDevice->getSLMWindowStartAddress());
    constexpr uintptr_t alignment = 128 * 1024; // 128 KB
    constexpr uintptr_t mask = alignment - 1;
    EXPECT_EQ(0U, mask & addr);
}

TEST(Device_GetCaps, validate) {
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();
    const auto &sysInfo = *platformDevices[0]->pSysInfo;

    EXPECT_NE(nullptr, caps.builtInKernels);

    std::string strDriverName = caps.name;
    std::string strFamilyName = familyName[device->getRenderCoreFamily()];
    EXPECT_NE(std::string::npos, strDriverName.find(strFamilyName));

    EXPECT_NE(nullptr, caps.name);
    EXPECT_NE(nullptr, caps.vendor);
    EXPECT_NE(nullptr, caps.driverVersion);
    EXPECT_NE(nullptr, caps.profile);
    EXPECT_NE(nullptr, caps.clVersion);
    EXPECT_NE(nullptr, caps.clCVersion);

    EXPECT_NE(nullptr, caps.spirVersions);
    EXPECT_NE(nullptr, caps.deviceExtensions);
    EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), caps.deviceAvailable);
    EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), caps.compilerAvailable);
    EXPECT_EQ(16u, caps.preferredVectorWidthChar);
    EXPECT_EQ(8u, caps.preferredVectorWidthShort);
    EXPECT_EQ(4u, caps.preferredVectorWidthInt);
    EXPECT_EQ(1u, caps.preferredVectorWidthLong);
    EXPECT_EQ(1u, caps.preferredVectorWidthFloat);
    EXPECT_EQ(1u, caps.preferredVectorWidthDouble);
    EXPECT_EQ(8u, caps.preferredVectorWidthHalf);
    EXPECT_EQ(16u, caps.nativeVectorWidthChar);
    EXPECT_EQ(8u, caps.nativeVectorWidthShort);
    EXPECT_EQ(4u, caps.nativeVectorWidthInt);
    EXPECT_EQ(1u, caps.nativeVectorWidthLong);
    EXPECT_EQ(1u, caps.nativeVectorWidthFloat);
    EXPECT_EQ(1u, caps.nativeVectorWidthDouble);
    EXPECT_EQ(8u, caps.nativeVectorWidthHalf);
    EXPECT_EQ(1u, caps.linkerAvailable);
    EXPECT_NE(0u, caps.globalMemCachelineSize);
    EXPECT_NE(0u, caps.globalMemCacheSize);
    EXPECT_LT(0u, caps.globalMemSize);
    EXPECT_EQ(caps.maxMemAllocSize, caps.maxConstantBufferSize);
    EXPECT_NE(nullptr, caps.ilVersion);

    EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), caps.deviceAvailable);
    EXPECT_EQ(static_cast<cl_device_mem_cache_type>(CL_READ_WRITE_CACHE), caps.globalMemCacheType);

    EXPECT_EQ(sysInfo.EUCount, caps.maxComputUnits);
    EXPECT_LT(0u, caps.maxConstantArgs);

    EXPECT_LE(128u, caps.maxReadImageArgs);
    EXPECT_LE(128u, caps.maxWriteImageArgs);
    EXPECT_EQ(128u, caps.maxReadWriteImageArgs);

    EXPECT_LE(caps.maxReadImageArgs * sizeof(cl_mem), caps.maxParameterSize);
    EXPECT_LE(caps.maxWriteImageArgs * sizeof(cl_mem), caps.maxParameterSize);
    EXPECT_LE(128u * MB, caps.maxMemAllocSize);
    EXPECT_GE((4 * GB) - (8 * KB), caps.maxMemAllocSize);
    EXPECT_LE(65536u, caps.imageMaxBufferSize);

    if (sysInfo.EUCount > 0) {
        auto expected = sysInfo.MaxSubSlicesSupported * sysInfo.MaxEuPerSubSlice *
                        sysInfo.ThreadCount / sysInfo.EUCount;
        EXPECT_EQ(expected, caps.computeUnitsUsedForScratch);
    }

    EXPECT_GT(caps.maxWorkGroupSize, 0u);
    EXPECT_EQ(caps.maxWorkItemSizes[0], caps.maxWorkGroupSize);
    EXPECT_EQ(caps.maxWorkItemSizes[1], caps.maxWorkGroupSize);
    EXPECT_EQ(caps.maxWorkItemSizes[2], caps.maxWorkGroupSize);
    EXPECT_LT(0u, caps.maxSamplers);

    // Minimum requirements for OpenCL 1.x
    EXPECT_EQ(static_cast<cl_device_fp_config>(CL_FP_ROUND_TO_NEAREST), CL_FP_ROUND_TO_NEAREST & caps.singleFpConfig);
    EXPECT_EQ(static_cast<cl_device_fp_config>(CL_FP_INF_NAN), CL_FP_INF_NAN & caps.singleFpConfig);

    cl_device_fp_config singleFpConfig = CL_FP_ROUND_TO_NEAREST |
                                         CL_FP_ROUND_TO_ZERO |
                                         CL_FP_ROUND_TO_INF |
                                         CL_FP_INF_NAN |
                                         CL_FP_FMA |
                                         CL_FP_DENORM;

    EXPECT_EQ(singleFpConfig, caps.singleFpConfig & singleFpConfig);

    EXPECT_EQ(static_cast<cl_device_exec_capabilities>(CL_EXEC_KERNEL), CL_EXEC_KERNEL & caps.executionCapabilities);
    EXPECT_EQ(static_cast<cl_command_queue_properties>(CL_QUEUE_PROFILING_ENABLE), CL_QUEUE_PROFILING_ENABLE & caps.queueOnHostProperties);

    EXPECT_EQ(static_cast<cl_command_queue_properties>(CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE), CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE & caps.queueOnHostProperties);
    EXPECT_LT(128u, caps.memBaseAddressAlign);
    EXPECT_LT(0u, caps.minDataTypeAlignSize);

    EXPECT_EQ(1u, caps.endianLittle);

    // should be hardcoded 8, 16, 32
    EXPECT_EQ(8u, caps.maxSubGroups[0]);
    EXPECT_EQ(16u, caps.maxSubGroups[1]);
    EXPECT_EQ(32u, caps.maxSubGroups[2]);

    if (device->getEnabledClVersion() >= 21) {
        EXPECT_TRUE(caps.independentForwardProgress != 0);
    } else {
        EXPECT_FALSE(caps.independentForwardProgress != 0);
    }

    EXPECT_EQ(caps.maxWorkGroupSize / 8, caps.maxNumOfSubGroups);

    EXPECT_EQ(1024u, caps.maxOnDeviceEvents);
    EXPECT_EQ(1u, caps.maxOnDeviceQueues);
    EXPECT_EQ(64u * MB, caps.queueOnDeviceMaxSize);
    EXPECT_EQ(128 * KB, caps.queueOnDevicePreferredSize);
    EXPECT_EQ(static_cast<cl_command_queue_properties>(CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE),
              caps.queueOnDeviceProperties);

    EXPECT_EQ(64u, caps.preferredGlobalAtomicAlignment);
    EXPECT_EQ(64u, caps.preferredLocalAtomicAlignment);
    EXPECT_EQ(64u, caps.preferredPlatformAtomicAlignment);

    EXPECT_EQ(1u, caps.imageSupport);
    EXPECT_EQ(16384u, caps.image2DMaxWidth);
    EXPECT_EQ(16384u, caps.image2DMaxHeight);
    EXPECT_EQ(2048u, caps.imageMaxArraySize);
    if (device->getHardwareInfo().capabilityTable.clVersionSupport == 12 && is32BitOsAllocatorAvailable) {
        EXPECT_TRUE(caps.force32BitAddressess);
    } else {
        //EXPECT_FALSE(caps.force32BitAddressess);
    }
}

TEST(Device_GetCaps, validateImage3DDimensions) {
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    if (device->getHardwareInfo().pPlatform->eRenderCoreFamily > IGFX_GEN8_CORE) {
        EXPECT_EQ(16384u, caps.image3DMaxWidth);
        EXPECT_EQ(16384u, caps.image3DMaxHeight);
    } else {
        EXPECT_EQ(2048u, caps.image3DMaxWidth);
        EXPECT_EQ(2048u, caps.image3DMaxHeight);
    }

    EXPECT_EQ(2048u, caps.image3DMaxDepth);
}

TEST(DeviceGetCapsSimple, givenDeviceWhenEUCountIsZeroThenmaxWgsIsDefault) {
    auto hardwareInfo = hardwareInfoTable[productFamily];
    GT_SYSTEM_INFO sysInfo = *hardwareInfo->pSysInfo;
    sysInfo.EUCount = 0;
    HardwareInfo hwInfo = {hardwareInfo->pPlatform, hardwareInfo->pSkuTable, hardwareInfo->pWaTable, &sysInfo, hardwareInfo->capabilityTable};

    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(&hwInfo));
    const auto &caps = device->getDeviceInfo();

    //default value
    uint32_t expected = 128u;
    EXPECT_EQ(expected, caps.maxWorkGroupSize);
}

TEST(Device_GetCaps, givenDontForcePreemptionModeDebugVariableWhenCreateDeviceThenSetDefaultHwPreemptionMode) {
    DebugManagerStateRestore dbgRestorer;
    {
        DebugManager.flags.ForcePreemptionMode.set(-1);
        auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
        EXPECT_TRUE(device->getHardwareInfo().capabilityTable.defaultPreemptionMode ==
                    device->getPreemptionMode());
    }
}

TEST(Device_GetCaps, givenForcePreemptionModeDebugVariableWhenCreateDeviceThenSetForcedMode) {
    DebugManagerStateRestore dbgRestorer;
    {
        PreemptionMode forceMode = PreemptionMode::MidThread;
        if (platformDevices[0]->capabilityTable.defaultPreemptionMode == forceMode) {
            // force non-default mode
            forceMode = PreemptionMode::ThreadGroup;
        }
        DebugManager.flags.ForcePreemptionMode.set((int32_t)forceMode);
        auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));

        EXPECT_TRUE(forceMode == device->getPreemptionMode());
    }
}

TEST(Device_GetCaps, givenDeviceWithMidThreadPreemptionWhenDeviceIsCreatedThenSipKernelIsCreated) {
    DebugManagerStateRestore dbgRestorer;
    {
        BuiltIns::shutDown();

        std::unique_ptr<MockBuiltins> mockBuiltins(new MockBuiltins);
        EXPECT_EQ(nullptr, mockBuiltins->peekCurrentInstance());
        mockBuiltins->overrideGlobalBuiltins();
        EXPECT_EQ(mockBuiltins.get(), mockBuiltins->peekCurrentInstance());
        EXPECT_FALSE(mockBuiltins->getSipKernelCalled);

        DebugManager.flags.ForcePreemptionMode.set((int32_t)PreemptionMode::MidThread);
        auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
        EXPECT_TRUE(mockBuiltins->getSipKernelCalled);
        mockBuiltins->restoreGlobalBuiltins();
        //make sure to release builtins prior to device as they use device
        mockBuiltins.reset();
    }
}

TEST(Device_GetCaps, givenForceOclVersion21WhenCapsAreCreatedThenDeviceReportsOpenCL21) {
    DebugManagerStateRestore dbgRestorer;
    {
        DebugManager.flags.ForceOCLVersion.set(21);
        auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
        const auto &caps = device->getDeviceInfo();
        EXPECT_STREQ("OpenCL 2.1 NEO ", caps.clVersion);
        EXPECT_STREQ("OpenCL C 2.0 ", caps.clCVersion);
        DebugManager.flags.ForceOCLVersion.set(0);
    }
}

TEST(Device_GetCaps, givenForceOclVersion20WhenCapsAreCreatedThenDeviceReportsOpenCL20) {
    DebugManagerStateRestore dbgRestorer;
    {
        DebugManager.flags.ForceOCLVersion.set(20);
        auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
        const auto &caps = device->getDeviceInfo();
        EXPECT_STREQ("OpenCL 2.0 NEO ", caps.clVersion);
        EXPECT_STREQ("OpenCL C 2.0 ", caps.clCVersion);
        DebugManager.flags.ForceOCLVersion.set(0);
    }
}

TEST(Device_GetCaps, givenForceOclVersion12WhenCapsAreCreatedThenDeviceReportsOpenCL12) {
    DebugManagerStateRestore dbgRestorer;
    {
        DebugManager.flags.ForceOCLVersion.set(12);
        auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
        const auto &caps = device->getDeviceInfo();
        EXPECT_STREQ("OpenCL 1.2 NEO ", caps.clVersion);
        EXPECT_STREQ("OpenCL C 1.2 ", caps.clCVersion);
        DebugManager.flags.ForceOCLVersion.set(0);
    }
}

TEST(Device_GetCaps, givenForceInvalidOclVersionWhenCapsAreCreatedThenDeviceWillDefaultToOpenCL12) {
    DebugManagerStateRestore dbgRestorer;
    {
        DebugManager.flags.ForceOCLVersion.set(1);
        auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
        const auto &caps = device->getDeviceInfo();
        EXPECT_STREQ("OpenCL 1.2 NEO ", caps.clVersion);
        EXPECT_STREQ("OpenCL C 1.2 ", caps.clCVersion);
        DebugManager.flags.ForceOCLVersion.set(0);
    }
}

TEST(Device_GetCaps, givenForce32bitAddressingWhenCapsAreCreatedThenDeviceReports32bitAddressingOptimization) {
    DebugManagerStateRestore dbgRestorer;
    {
        DebugManager.flags.Force32bitAddressing.set(true);
        auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
        const auto &caps = device->getDeviceInfo();
        if (is64bit) {
            EXPECT_TRUE(caps.force32BitAddressess);
        } else {
            EXPECT_FALSE(caps.force32BitAddressess);
        }
        auto expectedSize = (cl_ulong)(4 * 0.8 * GB);
        EXPECT_LE(caps.globalMemSize, expectedSize);
        EXPECT_LE(caps.maxMemAllocSize, expectedSize);
        EXPECT_LE(caps.maxConstantBufferSize, expectedSize);
        EXPECT_EQ(caps.addressBits, 32u);
    }
}

TEST(Device_GetCaps, alignGlobalMemSizeDownToPageSize) {
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    auto expectedSize = alignDown(caps.globalMemSize, MemoryConstants::pageSize);

    EXPECT_EQ(caps.globalMemSize, expectedSize);
}

TEST(Device_GetCaps, checkGlobalMemSize) {
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();
    auto pMemManager = device->getMemoryManager();
    unsigned int enabledCLVer = device->getEnabledClVersion();
    bool addressing32Bit = is32bit || (is64bit && (enabledCLVer < 20)) || DebugManager.flags.Force32bitAddressing.get();

    cl_ulong sharedMem = (cl_ulong)pMemManager->getSystemSharedMemory();
    cl_ulong maxAppAddrSpace = (cl_ulong)pMemManager->getMaxApplicationAddress() + 1ULL;
    cl_ulong memSize = std::min(sharedMem, maxAppAddrSpace);
    memSize = (cl_ulong)((double)memSize * 0.8);
    if (addressing32Bit) {
        memSize = std::min(memSize, (uint64_t)(4 * GB * 0.8));
    }
    cl_ulong expectedSize = alignDown(memSize, MemoryConstants::pageSize);

    EXPECT_EQ(caps.globalMemSize, expectedSize);
}

TEST(Device_GetCaps, extensionsStringEndsWithSpace) {
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();
    auto len = strlen(caps.deviceExtensions);
    ASSERT_LT(0U, len);
    EXPECT_EQ(' ', caps.deviceExtensions[len - 1]);
}

TEST(Device_GetCaps, DISABLED_givenDeviceWhenCapsAreCreateThenClGLSharingIsReported) {
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();
    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_khr_gl_sharing ")));
}

TEST(Device_GetCaps, givenOpenCLVersion21WhenCapsAreCreatedThenDeviceReportsClKhrSubgroupsExtension) {
    DebugManager.flags.ForceOCLVersion.set(21);
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_khr_subgroups")));

    DebugManager.flags.ForceOCLVersion.set(0);
}

TEST(Device_GetCaps, givenOpenCLVersion20WhenCapsAreCreatedThenDeviceDoesntReportClKhrSubgroupsExtension) {
    DebugManager.flags.ForceOCLVersion.set(20);
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_khr_subgroups"))));

    DebugManager.flags.ForceOCLVersion.set(0);
}

TEST(Device_GetCaps, givenOpenCLVersion21WhenCapsAreCreatedThenDeviceReportsClKhrIlProgramExtension) {
    DebugManager.flags.ForceOCLVersion.set(21);
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_khr_il_program")));

    DebugManager.flags.ForceOCLVersion.set(0);
}

TEST(Device_GetCaps, givenOpenCLVersion20WhenCapsAreCreatedThenDeviceDoesntReportClKhrIlProgramExtension) {
    DebugManager.flags.ForceOCLVersion.set(20);
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_khr_il_program"))));

    DebugManager.flags.ForceOCLVersion.set(0);
}

TEST(Device_GetCaps, givenEnableNV12setToTrueWhenCapsAreCreatedThenDeviceReportsNV12Extension) {
    DebugManager.flags.EnableNV12.set(true);
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_planar_yuv")));
    EXPECT_TRUE(caps.nv12Extension);

    DebugManager.flags.EnableNV12.set(false);
}

TEST(Device_GetCaps, givenEnablePackedYuvsetToTrueWhenCapsAreCreatedThenDeviceReportsPackedYuvExtension) {
    DebugManager.flags.EnablePackedYuv.set(true);
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_packed_yuv")));
    EXPECT_TRUE(caps.packedYuvExtension);

    DebugManager.flags.EnablePackedYuv.set(false);
}

TEST(Device_GetCaps, givenEnableVmeSetToTrueWhenCapsAreCreatedThenDeviceReportsVmeExtensionAndBuiltins) {
    DebugManager.flags.EnableIntelVme.set(true);
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_motion_estimation")));
    EXPECT_TRUE(caps.vmeExtension);

    EXPECT_THAT(caps.builtInKernels, testing::HasSubstr("block_motion_estimate_intel"));

    DebugManager.flags.EnableIntelVme.set(false);
}

TEST(Device_GetCaps, givenEnableVmeSetToFalseWhenCapsAreCreatedThenDeviceDoesNotReportsVmeExtensionAndBuiltins) {
    DebugManager.flags.EnableIntelVme.set(false);
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_intel_motion_estimation"))));
    EXPECT_FALSE(caps.vmeExtension);

    EXPECT_THAT(caps.builtInKernels, testing::Not(testing::HasSubstr("block_motion_estimate_intel")));

    DebugManager.flags.EnableIntelVme.set(false);
}

TEST(Device_GetCaps, givenEnableAdvancedVmeSetToTrueWhenCapsAreCreatedThenDeviceReportsAdvancedVmeExtensionAndBuiltins) {
    DebugManager.flags.EnableIntelAdvancedVme.set(true);
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_advanced_motion_estimation")));

    EXPECT_THAT(caps.builtInKernels, testing::HasSubstr("block_advanced_motion_estimate_check_intel"));
    EXPECT_THAT(caps.builtInKernels, testing::HasSubstr("block_advanced_motion_estimate_bidirectional_check_intel"));

    DebugManager.flags.EnableIntelAdvancedVme.set(false);
}

TEST(Device_GetCaps, givenEnableAdvancedVmeSetToFalseWhenCapsAreCreatedThenDeviceDoesNotReportsAdvancedVmeExtensionAndBuiltins) {
    DebugManager.flags.EnableIntelAdvancedVme.set(false);
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_intel_advanced_motion_estimation"))));

    EXPECT_THAT(caps.builtInKernels, testing::Not(testing::HasSubstr("block_advanced_motion_estimate_check_intel")));
    EXPECT_THAT(caps.builtInKernels, testing::Not(testing::HasSubstr("block_advanced_motion_estimate_bidirectional_check_intel")));

    DebugManager.flags.EnableIntelAdvancedVme.set(false);
}

TEST(Device_GetCaps, byDefaultVmeIsTurnedOn) {
    DebugSettingsManager<DebugFunctionalityLevel::RegKeys> freshDebugSettingsManager;
    EXPECT_TRUE(freshDebugSettingsManager.flags.EnableIntelVme.get());
}

TEST(Device_GetCaps, givenOpenCL21DeviceCapsWhenAskedForCPUcopyFlagThenTrueIsReturned) {
    DebugManagerStateRestore stateRestorer;
    DebugManager.flags.ForceOCLVersion.set(21);
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();
    EXPECT_TRUE(caps.cpuCopyAllowed);
}

TEST(Device_GetCaps, givenOpenCL20DeviceCapsWhenAskedForCPUcopyFlagThenTrueIsReturned) {
    DebugManagerStateRestore stateRestorer;
    DebugManager.flags.ForceOCLVersion.set(20);
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();
    EXPECT_TRUE(caps.cpuCopyAllowed);
}

TEST(Device_GetCaps, givenOpenCL12DeviceCapsWhenAskedForCPUcopyFlagThenTrueIsReturned) {
    DebugManagerStateRestore stateRestorer;
    DebugManager.flags.ForceOCLVersion.set(12);
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();
    EXPECT_TRUE(caps.cpuCopyAllowed);
}

TEST(Device_GetCaps, deviceReportsPriorityHintsExtension) {
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_khr_priority_hints")));
}

TEST(Device_GetCaps, deviceReportsCreateCommandQueueExtension) {
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_khr_create_command_queue")));
}

TEST(Device_GetCaps, deviceReportsThrottleHintsExtension) {
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_khr_throttle_hints")));
}

TEST(Device_GetCaps, givenAtleastOCL2DeviceThenExposesMipmapImageExtensions) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForceOCLVersion.set(20);

    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_khr_mipmap_image")));
    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_khr_mipmap_image_writes")));
}

TEST(Device_GetCaps, givenOCL12DeviceThenDoesNotExposeMipmapImageExtensions) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForceOCLVersion.set(12);

    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_khr_mipmap_image"))));
    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_khr_mipmap_image_writes"))));
}

TEST(Device_GetCaps, givenDeviceThatDoesntHaveFp64ThenExtensionIsNotReported) {
    HardwareInfo nonFp64Device = *platformDevices[0];
    nonFp64Device.capabilityTable.ftrSupportsFP64 = false;
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(&nonFp64Device));

    const auto &caps = device->getDeviceInfo();
    std::string extensionString = caps.deviceExtensions;
    EXPECT_EQ(std::string::npos, extensionString.find(std::string("cl_khr_fp64")));
    EXPECT_EQ(0u, caps.doubleFpConfig);
}

TEST(Device_GetCaps, givenOclVersionLessThan21WhenCapsAreCreatedThenDeviceReportsNoSupportedIlVersions) {
    DebugManagerStateRestore dbgRestorer;
    {
        DebugManager.flags.ForceOCLVersion.set(12);
        auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
        const auto &caps = device->getDeviceInfo();
        EXPECT_STREQ("", caps.ilVersion);
        DebugManager.flags.ForceOCLVersion.set(0);
    }
}

TEST(Device_GetCaps, givenOclVersion21WhenCapsAreCreatedThenDeviceReportsSpirvAsSupportedIl) {
    DebugManagerStateRestore dbgRestorer;
    {
        DebugManager.flags.ForceOCLVersion.set(21);
        auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
        const auto &caps = device->getDeviceInfo();
        EXPECT_STREQ("SPIR-V_1.0 ", caps.ilVersion);
        DebugManager.flags.ForceOCLVersion.set(0);
    }
}

TEST(Device_GetCaps, givenDisabledFtrPooledEuWhenCalculatingMaxEuPerSSThenIgnoreEuCountPerPoolMin) {
    GT_SYSTEM_INFO mySysInfo = *platformDevices[0]->pSysInfo;
    FeatureTable mySkuTable = *platformDevices[0]->pSkuTable;
    HardwareInfo myHwInfo = {platformDevices[0]->pPlatform, &mySkuTable, platformDevices[0]->pWaTable,
                             &mySysInfo, platformDevices[0]->capabilityTable};

    mySysInfo.EUCount = 20;
    mySysInfo.EuCountPerPoolMin = 99999;
    mySkuTable.ftrPooledEuEnabled = 0;

    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(&myHwInfo));

    auto expectedMaxWGS = (mySysInfo.EUCount / mySysInfo.SubSliceCount) *
                          (mySysInfo.ThreadCount / mySysInfo.EUCount) * 8;
    expectedMaxWGS = std::min(Math::prevPowerOfTwo(expectedMaxWGS), 256u);

    EXPECT_EQ(expectedMaxWGS, device->getDeviceInfo().maxWorkGroupSize);
}

TEST(Device_GetCaps, givenEnabledFtrPooledEuWhenCalculatingMaxEuPerSSThenDontIgnoreEuCountPerPoolMin) {
    GT_SYSTEM_INFO mySysInfo = *platformDevices[0]->pSysInfo;
    FeatureTable mySkuTable = *platformDevices[0]->pSkuTable;
    HardwareInfo myHwInfo = {platformDevices[0]->pPlatform, &mySkuTable, platformDevices[0]->pWaTable,
                             &mySysInfo, platformDevices[0]->capabilityTable};

    mySysInfo.EUCount = 20;
    mySysInfo.EuCountPerPoolMin = 99999;
    mySkuTable.ftrPooledEuEnabled = 1;

    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(&myHwInfo));

    auto expectedMaxWGS = mySysInfo.EuCountPerPoolMin * (mySysInfo.ThreadCount / mySysInfo.EUCount) * 8;
    expectedMaxWGS = std::min(Math::prevPowerOfTwo(expectedMaxWGS), 256u);

    EXPECT_EQ(expectedMaxWGS, device->getDeviceInfo().maxWorkGroupSize);
}

TEST(Device_GetCaps, givenDebugFlagToUseMaxSimdSizeForWkgCalculationWhenDeviceCapsAreCreatedThen1024WorkgroupSizeIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseMaxSimdSizeToDeduceMaxWorkgroupSize.set(true);

    GT_SYSTEM_INFO mySysInfo = *platformDevices[0]->pSysInfo;
    FeatureTable mySkuTable = *platformDevices[0]->pSkuTable;
    HardwareInfo myHwInfo = {platformDevices[0]->pPlatform, &mySkuTable, platformDevices[0]->pWaTable,
                             &mySysInfo, platformDevices[0]->capabilityTable};

    mySysInfo.EUCount = 24;
    mySysInfo.SubSliceCount = 3;
    mySysInfo.ThreadCount = 24 * 7;
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(&myHwInfo));

    EXPECT_EQ(1024u, device->getDeviceInfo().maxWorkGroupSize);
    EXPECT_EQ(device->getDeviceInfo().maxWorkGroupSize / 32, device->getDeviceInfo().maxNumOfSubGroups);
}

class DriverInfoMock : public DriverInfo {
  public:
    DriverInfoMock(){};

    const static std::string testDeviceName;
    const static std::string testVersion;

    std::string getDeviceName(std::string defaultName) override { return testDeviceName; };
    std::string getVersion(std::string defaultVersion) override { return testVersion; };
};

const std::string DriverInfoMock::testDeviceName = "testDeviceName";
const std::string DriverInfoMock::testVersion = "testVersion";

TEST(Device_GetCaps, givenSystemWithDriverInfoWhenGettingNameAndVersionThenReturnValuesFromDriverInfo) {
    auto device = Device::create<OCLRT::MockDevice>(platformDevices[0]);

    DriverInfoMock *driverInfoMock = new DriverInfoMock();
    device->setDriverInfo(driverInfoMock);
    device->initializeCaps();

    const auto &caps = device->getDeviceInfo();

    EXPECT_STREQ(DriverInfoMock::testDeviceName.c_str(), caps.name);
    EXPECT_STREQ(DriverInfoMock::testVersion.c_str(), caps.driverVersion);
    delete device;
}

TEST(Device_GetCaps, givenSystemWithNoDriverInfoWhenGettingNameAndVersionThenReturnDefaultValues) {
    auto device = Device::create<OCLRT::MockDevice>(platformDevices[0]);

    device->setDriverInfo(nullptr);
    device->initializeCaps();

    const auto &caps = device->getDeviceInfo();

    std::string tempName = "Intel(R) ";
    tempName += familyName[platformDevices[0]->pPlatform->eRenderCoreFamily];
    tempName += " HD Graphics NEO";

#define QTR(a) #a
#define TOSTR(b) QTR(b)
    const std::string expectedVersion = TOSTR(NEO_DRIVER_VERSION);
#undef QTR
#undef TOSTR

    EXPECT_STREQ(tempName.c_str(), caps.name);
    EXPECT_STREQ(expectedVersion.c_str(), caps.driverVersion);
    delete device;
}

TEST(Device_GetCaps, GivenFlagEnabled64kbPagesWhenSetThenReturnCorrectValue) {
    DebugManagerStateRestore dbgRestore;
    bool orgOsEnabled64kbPages = OSInterface::osEnabled64kbPages;
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    HardwareInfo &hwInfo = const_cast<HardwareInfo &>(device->getHardwareInfo());
    bool orgftr64KBpages = hwInfo.capabilityTable.ftr64KBpages;

    DebugManager.flags.Enable64kbpages.set(-1);

    hwInfo.capabilityTable.ftr64KBpages = false;
    OSInterface::osEnabled64kbPages = false;
    EXPECT_FALSE(device->getEnabled64kbPages());

    hwInfo.capabilityTable.ftr64KBpages = false;
    OSInterface::osEnabled64kbPages = true;
    EXPECT_FALSE(device->getEnabled64kbPages());

    hwInfo.capabilityTable.ftr64KBpages = true;
    OSInterface::osEnabled64kbPages = false;
    EXPECT_FALSE(device->getEnabled64kbPages());

    hwInfo.capabilityTable.ftr64KBpages = true;
    OSInterface::osEnabled64kbPages = true;
    EXPECT_TRUE(device->getEnabled64kbPages());

    DebugManager.flags.Enable64kbpages.set(0); // force false
    EXPECT_FALSE(device->getEnabled64kbPages());

    DebugManager.flags.Enable64kbpages.set(1); // force true
    EXPECT_TRUE(device->getEnabled64kbPages());

    OSInterface::osEnabled64kbPages = orgOsEnabled64kbPages;
    hwInfo.capabilityTable.ftr64KBpages = orgftr64KBpages;
}

TEST(Device_GetCaps, givenDeviceWithNullSourceLevelDebuggerWhenCapsAreInitializedThenSourceLevelDebuggerActiveIsSetToFalse) {
    std::unique_ptr<Device> device(Device::create<OCLRT::MockDevice>(platformDevices[0]));

    const auto &caps = device->getDeviceInfo();
    EXPECT_EQ(nullptr, device->getSourceLevelDebugger());
    EXPECT_FALSE(caps.sourceLevelDebuggerActive);
}

typedef HwHelperTest DeviceCapsWithModifiedHwInfoTest;

TEST_F(DeviceCapsWithModifiedHwInfoTest, givenPlatformWithSourceLevelDebuggerNotSupportedWhenDeviceIsCreatedThenSourceLevelDebuggerActiveIsSetToFalse) {

    hwInfo.capabilityTable.sourceLevelDebuggerSupported = false;

    std::unique_ptr<MockDeviceWithSourceLevelDebugger<>> device(Device::create<MockDeviceWithSourceLevelDebugger<>>(&hwInfo));

    const auto &caps = device->getDeviceInfo();
    EXPECT_NE(nullptr, device->getSourceLevelDebugger());
    EXPECT_FALSE(caps.sourceLevelDebuggerActive);
}
