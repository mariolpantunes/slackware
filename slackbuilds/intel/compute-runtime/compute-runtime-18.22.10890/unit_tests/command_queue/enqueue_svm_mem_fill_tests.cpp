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

#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "runtime/memory_manager/svm_memory_manager.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/mocks/mock_builtin_dispatch_info_builder.h"
#include "test.h"

using namespace OCLRT;

struct EnqueueSvmMemFillTest : public DeviceFixture,
                               public CommandQueueHwFixture,
                               public ::testing::TestWithParam<size_t> {
    typedef CommandQueueHwFixture CommandQueueFixture;

    EnqueueSvmMemFillTest() {
    }

    void SetUp() override {
        DeviceFixture::SetUp();
        CommandQueueFixture::SetUp(pDevice, 0);
        patternSize = (size_t)GetParam();
        ASSERT_TRUE((0 < patternSize) && (patternSize <= 128));
        svmPtr = context->getSVMAllocsManager()->createSVMAlloc(256, true);
        ASSERT_NE(nullptr, svmPtr);
        svmAlloc = context->getSVMAllocsManager()->getSVMAlloc(svmPtr);
        ASSERT_NE(nullptr, svmAlloc);
    }

    void TearDown() override {
        context->getSVMAllocsManager()->freeSVMAlloc(svmPtr);
        CommandQueueFixture::TearDown();
        DeviceFixture::TearDown();
    }

    const uint64_t pattern[16] = {0x0011223344556677, 0x8899AABBCCDDEEFF, 0xFFEEDDCCBBAA9988, 0x7766554433221100,
                                  0xFFEEDDCCBBAA9988, 0x7766554433221100, 0x0011223344556677, 0x8899AABBCCDDEEFF};
    size_t patternSize = 0;
    void *svmPtr = nullptr;
    GraphicsAllocation *svmAlloc = nullptr;
};

HWTEST_P(EnqueueSvmMemFillTest, givenEnqueueSVMMemFillWhenUsingFillBufferBuilderThenItIsConfiguredWithBuitinOpParamsAndProducesDispatchInfo) {
    struct MockFillBufferBuilder : MockBuiltinDispatchInfoBuilder {
        MockFillBufferBuilder(BuiltIns &kernelLib, BuiltinDispatchInfoBuilder *origBuilder, const void *pattern, size_t patternSize)
            : MockBuiltinDispatchInfoBuilder(kernelLib, origBuilder),
              pattern(pattern), patternSize(patternSize) {
        }
        void validateInput(const BuiltinOpParams &conf) const override {
            auto patternAllocation = conf.srcMemObj->getGraphicsAllocation();
            EXPECT_EQ(patternSize, patternAllocation->getUnderlyingBufferSize());
            EXPECT_EQ(0, memcmp(pattern, patternAllocation->getUnderlyingBuffer(), patternSize));
        };
        const void *pattern;
        size_t patternSize;
    };

    // retrieve original builder
    auto &origBuilder = BuiltIns::getInstance().getBuiltinDispatchInfoBuilder(
        EBuiltInOps::FillBuffer,
        pCmdQ->getContext(),
        pCmdQ->getDevice());
    ASSERT_NE(nullptr, &origBuilder);

    // substitute original builder with mock builder
    auto oldBuilder = BuiltIns::getInstance().setBuiltinDispatchInfoBuilder(
        EBuiltInOps::FillBuffer,
        pCmdQ->getContext(),
        pCmdQ->getDevice(),
        std::unique_ptr<OCLRT::BuiltinDispatchInfoBuilder>(new MockFillBufferBuilder(BuiltIns::getInstance(), &origBuilder, pattern, patternSize)));
    EXPECT_EQ(&origBuilder, oldBuilder.get());

    // call enqueue on mock builder
    auto retVal = pCmdQ->enqueueSVMMemFill(
        svmPtr,      // void *svm_ptr
        pattern,     // const void *pattern
        patternSize, // size_t pattern_size
        256,         // size_t size
        0,           // cl_uint num_events_in_wait_list
        nullptr,     // cl_event *event_wait_list
        nullptr      // cL_event *event
        );
    EXPECT_EQ(CL_SUCCESS, retVal);

    // restore original builder and retrieve mock builder
    auto newBuilder = BuiltIns::getInstance().setBuiltinDispatchInfoBuilder(
        EBuiltInOps::FillBuffer,
        pCmdQ->getContext(),
        pCmdQ->getDevice(),
        std::move(oldBuilder));
    EXPECT_NE(nullptr, newBuilder);

    // check if original builder is restored correctly
    auto &restoredBuilder = BuiltIns::getInstance().getBuiltinDispatchInfoBuilder(
        EBuiltInOps::FillBuffer,
        pCmdQ->getContext(),
        pCmdQ->getDevice());
    EXPECT_EQ(&origBuilder, &restoredBuilder);

    // use mock builder to validate builder's input / output
    auto mockBuilder = static_cast<MockFillBufferBuilder *>(newBuilder.get());

    // validate builder's input - builtin ops
    auto params = mockBuilder->getBuiltinOpParams();
    EXPECT_EQ(nullptr, params->srcPtr);
    EXPECT_EQ(svmPtr, params->dstPtr);
    EXPECT_NE(nullptr, params->srcMemObj);
    EXPECT_EQ(nullptr, params->dstMemObj);
    EXPECT_EQ(nullptr, params->srcSvmAlloc);
    EXPECT_EQ(svmAlloc, params->dstSvmAlloc);
    EXPECT_EQ(Vec3<size_t>(0, 0, 0), params->srcOffset);
    EXPECT_EQ(Vec3<size_t>(0, 0, 0), params->dstOffset);
    EXPECT_EQ(Vec3<size_t>(256, 0, 0), params->size);

    // validate builder's output - multi dispatch info
    auto mdi = mockBuilder->getMultiDispatchInfo();
    EXPECT_EQ(1u, mdi->size());

    auto di = mdi->begin();
    size_t middleElSize = sizeof(uint32_t);
    EXPECT_EQ(Vec3<size_t>(256 / middleElSize, 1, 1), di->getGWS());

    auto kernel = di->getKernel();
    EXPECT_STREQ("FillBufferMiddle", kernel->getKernelInfo().name.c_str());
}

INSTANTIATE_TEST_CASE_P(size_t,
                        EnqueueSvmMemFillTest,
                        ::testing::Values(1, 2, 4, 8, 16, 32, 64, 128));
