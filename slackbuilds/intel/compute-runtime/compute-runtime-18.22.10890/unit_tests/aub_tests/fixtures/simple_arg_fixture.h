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

#include "runtime/command_stream/command_stream_receiver.h"
#include "unit_tests/aub_tests/command_stream/aub_command_stream_fixture.h"
#include "unit_tests/command_stream/command_stream_fixture.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/indirect_heap/indirect_heap_fixture.h"
#include "unit_tests/fixtures/simple_arg_fixture.h"
#include "unit_tests/fixtures/simple_arg_kernel_fixture.h"

namespace OCLRT {

////////////////////////////////////////////////////////////////////////////////
// Factory where all command stream traffic funnels to an AUB file
////////////////////////////////////////////////////////////////////////////////
struct AUBSimpleArgFixtureFactory : public SimpleArgFixtureFactory {
    typedef AUBCommandStreamFixture CommandStreamFixture;
};

////////////////////////////////////////////////////////////////////////////////
// SimpleArgTest
//      Instantiates a fixture based on the supplied fixture factory.
//      Performs proper initialization/shutdown of various elements in factory.
//      Used by most tests for integration testing with command queues.
////////////////////////////////////////////////////////////////////////////////
template <typename FixtureFactory>
struct SimpleArgFixture : public FixtureFactory::IndirectHeapFixture,
                          public FixtureFactory::CommandStreamFixture,
                          public FixtureFactory::CommandQueueFixture,
                          public FixtureFactory::KernelFixture,
                          public DeviceFixture {
    typedef typename FixtureFactory::IndirectHeapFixture IndirectHeapFixture;
    typedef typename FixtureFactory::CommandStreamFixture CommandStreamFixture;
    typedef typename FixtureFactory::CommandQueueFixture CommandQueueFixture;
    typedef typename FixtureFactory::KernelFixture KernelFixture;

    using CommandQueueFixture::pCmdQ;
    using CommandStreamFixture::pCS;
    using KernelFixture::pKernel;
    using IndirectHeapFixture::SetUp;
    using AUBCommandStreamFixture::SetUp;
    using SimpleArgKernelFixture::SetUp;

    SimpleArgFixture()
        : pDestMemory(nullptr), sizeUserMemory(128 * sizeof(float)) {
    }

  public:
    virtual void SetUp() {
        DeviceFixture::SetUp();
        ASSERT_NE(nullptr, pDevice);
        CommandQueueFixture::SetUp(pDevice, 0);
        ASSERT_NE(nullptr, pCmdQ);
        CommandStreamFixture::SetUp(pCmdQ);
        ASSERT_NE(nullptr, pCS);
        IndirectHeapFixture::SetUp(pCmdQ);
        KernelFixture::SetUp(pDevice);
        ASSERT_NE(nullptr, pKernel);

        int argVal = (int)0x22222222;
        pDestMemory = alignedMalloc(sizeUserMemory, 4096);
        ASSERT_NE(nullptr, pDestMemory);

        pExpectedMemory = alignedMalloc(sizeUserMemory, 4096);
        ASSERT_NE(nullptr, pExpectedMemory);

        // Initialize user memory to known values
        memset(pDestMemory, 0x11, sizeUserMemory);
        memset(pExpectedMemory, 0x22, sizeUserMemory);

        pKernel->setArg(0, sizeof(int), &argVal);
        pKernel->setArgSvm(1, sizeUserMemory, pDestMemory);

        auto &commandStreamReceiver = pDevice->getCommandStreamReceiver();
        commandStreamReceiver.createAllocationAndHandleResidency(pDestMemory, sizeUserMemory);
    }

    virtual void TearDown() {
        alignedFree(pExpectedMemory);
        alignedFree(pDestMemory);

        KernelFixture::TearDown();
        IndirectHeapFixture::TearDown();
        CommandStreamFixture::TearDown();
        CommandQueueFixture::TearDown();
        DeviceFixture::TearDown();
    }

    void *pDestMemory;
    void *pExpectedMemory;
    size_t sizeUserMemory;
};
}
