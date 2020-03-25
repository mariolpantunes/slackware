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

#include "runtime/kernel/kernel.h"
#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "runtime/indirect_heap/indirect_heap.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/hash.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/surface.h"
#include "program_tests.h"
#include "unit_tests/helpers/kernel_binary_helper.h"
#include "unit_tests/program/program_from_binary.h"
#include "unit_tests/program/program_with_source.h"
#include "test.h"
#include <memory>
#include <vector>
#include <map>
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

using namespace OCLRT;

struct ProgramMock : Program {
    void updateNonUniformFlag() {
        Program::updateNonUniformFlag();
    }

    void updateNonUniformFlag(const Program **inputPrograms, uint32_t numInputPrograms) {
        Program::updateNonUniformFlag(inputPrograms, numInputPrograms);
    }

    void setBuildOptions(const char *buildOptions) {
        options = buildOptions != nullptr ? buildOptions : "";
    }

    void forceNonUniformFlag(bool flag) {
        this->allowNonUniform = flag;
    }
};

TEST(ProgramNonUniform, UpdateAllowNonUniform) {
    ProgramMock pm;
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(12u, pm.getProgramOptionVersion());
    pm.setBuildOptions(nullptr);
    pm.updateNonUniformFlag();
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(12u, pm.getProgramOptionVersion());
}

TEST(ProgramNonUniform, UpdateAllowNonUniform12) {
    ProgramMock pm;
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(12u, pm.getProgramOptionVersion());
    pm.setBuildOptions("-cl-std=CL1.2");
    pm.updateNonUniformFlag();
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(12u, pm.getProgramOptionVersion());
}

TEST(ProgramNonUniform, UpdateAllowNonUniform20) {
    ProgramMock pm;
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(12u, pm.getProgramOptionVersion());
    pm.setBuildOptions("-cl-std=CL2.0");
    pm.updateNonUniformFlag();
    EXPECT_TRUE(pm.getAllowNonUniform());
    EXPECT_EQ(20u, pm.getProgramOptionVersion());
}

TEST(ProgramNonUniform, UpdateAllowNonUniform21) {
    ProgramMock pm;
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(12u, pm.getProgramOptionVersion());
    pm.setBuildOptions("-cl-std=CL2.1");
    pm.updateNonUniformFlag();
    EXPECT_TRUE(pm.getAllowNonUniform());
    EXPECT_EQ(21u, pm.getProgramOptionVersion());
}

TEST(ProgramNonUniform, UpdateAllowNonUniform20UniformFlag) {
    ProgramMock pm;
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(12u, pm.getProgramOptionVersion());
    pm.setBuildOptions("-cl-std=CL2.0 -cl-uniform-work-group-size");
    pm.updateNonUniformFlag();
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(20u, pm.getProgramOptionVersion());
}

TEST(ProgramNonUniform, UpdateAllowNonUniform21UniformFlag) {
    ProgramMock pm;
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(12u, pm.getProgramOptionVersion());
    pm.setBuildOptions("-cl-std=CL2.1 -cl-uniform-work-group-size");
    pm.updateNonUniformFlag();
    EXPECT_FALSE(pm.getAllowNonUniform());
    EXPECT_EQ(21u, pm.getProgramOptionVersion());
}

TEST(KernelNonUniform, GetAllowNonUniformFlag) {
    ProgramMock pm;
    KernelInfo ki;
    MockDevice d(*platformDevices[0]);
    struct KernelMock : Kernel {
        KernelMock(Program *p, KernelInfo &ki, Device &d)
            : Kernel(p, ki, d) {
        }
    };

    KernelMock k{&pm, ki, d};
    pm.forceNonUniformFlag(false);
    EXPECT_FALSE(k.getAllowNonUniform());
    pm.forceNonUniformFlag(true);
    EXPECT_TRUE(k.getAllowNonUniform());
    pm.forceNonUniformFlag(false);
    EXPECT_FALSE(k.getAllowNonUniform());
}

TEST(ProgramNonUniform, UpdateAllowNonUniformOutcomeUniformFlag) {
    ProgramMock pm;
    ProgramMock pm1;
    ProgramMock pm2;
    const ProgramMock *inputPrograms[] = {&pm1, &pm2};
    cl_uint numInputPrograms = 2;

    pm1.forceNonUniformFlag(false);
    pm2.forceNonUniformFlag(false);
    pm.updateNonUniformFlag((const Program **)inputPrograms, numInputPrograms);
    EXPECT_FALSE(pm.getAllowNonUniform());

    pm1.forceNonUniformFlag(false);
    pm2.forceNonUniformFlag(true);
    pm.updateNonUniformFlag((const Program **)inputPrograms, numInputPrograms);
    EXPECT_FALSE(pm.getAllowNonUniform());

    pm1.forceNonUniformFlag(true);
    pm2.forceNonUniformFlag(false);
    pm.updateNonUniformFlag((const Program **)inputPrograms, numInputPrograms);
    EXPECT_FALSE(pm.getAllowNonUniform());

    pm1.forceNonUniformFlag(true);
    pm2.forceNonUniformFlag(true);
    pm.updateNonUniformFlag((const Program **)inputPrograms, numInputPrograms);
    EXPECT_TRUE(pm.getAllowNonUniform());
}

#include "runtime/helpers/options.h"
#include "runtime/kernel/kernel.h"
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/fixtures/program_fixture.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/mocks/mock_program.h"

#include <vector>

namespace OCLRT {

class ProgramNonUniformTest : public ContextFixture,
                              public PlatformFixture,
                              public ProgramFixture,
                              public CommandQueueHwFixture,
                              public testing::Test {

    using ContextFixture::SetUp;
    using PlatformFixture::SetUp;

  protected:
    ProgramNonUniformTest() {
    }

    void SetUp() override {
        PlatformFixture::SetUp(numPlatformDevices, platformDevices);
        device = pPlatform->getDevice(0);
        ContextFixture::SetUp(1, &device);
        ProgramFixture::SetUp();
        CommandQueueHwFixture::SetUp(pPlatform->getDevice(0), 0);
    }

    void TearDown() override {
        CommandQueueHwFixture::TearDown();
        ProgramFixture::TearDown();
        ContextFixture::TearDown();
        PlatformFixture::TearDown();
        CompilerInterface::shutdown();
    }
    cl_device_id device;
    cl_int retVal = CL_SUCCESS;
};

TEST_F(ProgramNonUniformTest, ExecuteKernelNonUniform21) {
    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.1") != std::string::npos) {
        CreateProgramFromBinary<MockProgram>(pContext, &device, "kernel_data_param");
        auto mockProgram = (MockProgram *)pProgram;
        ASSERT_NE(nullptr, mockProgram);

        mockProgram->setBuildOptions("-cl-std=CL2.1");
        retVal = mockProgram->build(
            1,
            &device,
            nullptr,
            nullptr,
            nullptr,
            false);
        EXPECT_EQ(CL_SUCCESS, retVal);

        auto pKernelInfo = mockProgram->Program::getKernelInfo("test_get_local_size");
        EXPECT_NE(nullptr, pKernelInfo);

        // create a kernel
        auto pKernel = Kernel::create<MockKernel>(mockProgram, *pKernelInfo, &retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, pKernel);

        size_t globalWorkSize[3] = {12, 12, 12};
        size_t localWorkSize[3] = {11, 12, 1};

        retVal = pCmdQ->enqueueKernel(
            pKernel,
            3,
            nullptr,
            globalWorkSize,
            localWorkSize,
            0,
            nullptr,
            nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        delete pKernel;
    }
}

TEST_F(ProgramNonUniformTest, ExecuteKernelNonUniform20) {
    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.0") != std::string::npos) {
        CreateProgramFromBinary<MockProgram>(pContext, &device, "kernel_data_param");
        auto mockProgram = (MockProgram *)pProgram;
        ASSERT_NE(nullptr, mockProgram);

        mockProgram->setBuildOptions("-cl-std=CL2.0");
        retVal = mockProgram->build(
            1,
            &device,
            nullptr,
            nullptr,
            nullptr,
            false);
        EXPECT_EQ(CL_SUCCESS, retVal);

        auto pKernelInfo = mockProgram->Program::getKernelInfo("test_get_local_size");
        EXPECT_NE(nullptr, pKernelInfo);

        // create a kernel
        auto pKernel = Kernel::create<MockKernel>(mockProgram, *pKernelInfo, &retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, pKernel);

        size_t globalWorkSize[3] = {12, 12, 12};
        size_t localWorkSize[3] = {11, 12, 12};

        retVal = pCmdQ->enqueueKernel(
            pKernel,
            3,
            nullptr,
            globalWorkSize,
            localWorkSize,
            0,
            nullptr,
            nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        delete pKernel;
    }
}

TEST_F(ProgramNonUniformTest, ExecuteKernelNonUniform12) {
    CreateProgramFromBinary<MockProgram>(pContext, &device, "kernel_data_param");
    auto mockProgram = (MockProgram *)pProgram;
    ASSERT_NE(nullptr, mockProgram);

    mockProgram->setBuildOptions("-cl-std=CL1.2");
    retVal = mockProgram->build(
        1,
        &device,
        nullptr,
        nullptr,
        nullptr,
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pKernelInfo = mockProgram->Program::getKernelInfo("test_get_local_size");
    EXPECT_NE(nullptr, pKernelInfo);

    // create a kernel
    auto pKernel = Kernel::create<MockKernel>(mockProgram, *pKernelInfo, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, pKernel);

    size_t globalWorkSize[3] = {12, 12, 12};
    size_t localWorkSize[3] = {11, 12, 12};

    retVal = pCmdQ->enqueueKernel(
        pKernel,
        3,
        nullptr,
        globalWorkSize,
        localWorkSize,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_WORK_GROUP_SIZE, retVal);

    delete pKernel;
}
} // namespace OCLRT
