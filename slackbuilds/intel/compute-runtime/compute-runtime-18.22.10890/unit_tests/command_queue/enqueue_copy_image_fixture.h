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

#pragma once
#include "gtest/gtest.h"
#include "runtime/helpers/ptr_math.h"
#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/fixtures/built_in_fixture.h"

namespace OCLRT {

struct EnqueueCopyImageTest : public CommandEnqueueFixture,
                              public ::testing::Test {

    EnqueueCopyImageTest() : srcImage(nullptr),
                             dstImage(nullptr) {
    }

    virtual void SetUp(void) override {
        CommandEnqueueFixture::SetUp();
        context = new MockContext(pDevice);
        srcImage = Image2dHelper<>::create(context);
        dstImage = Image2dHelper<>::create(context);
    }

    virtual void TearDown(void) override {
        delete dstImage;
        delete srcImage;
        delete context;
        CommandEnqueueFixture::TearDown();
    }

  protected:
    template <typename FamilyType>
    void enqueueCopyImage() {
        auto retVal = EnqueueCopyImageHelper<>::enqueueCopyImage(
            pCmdQ,
            srcImage,
            dstImage);
        EXPECT_EQ(CL_SUCCESS, retVal);
        parseCommands<FamilyType>(*pCmdQ);
    }

    MockContext *context;
    Image *srcImage;
    Image *dstImage;
};

struct EnqueueCopyImageMipMapTest : public CommandEnqueueFixture,
                                    public ::testing::Test,
                                    public ::testing::WithParamInterface<std::tuple<uint32_t, uint32_t>> {

    void SetUp(void) override {
        CommandEnqueueFixture::SetUp();
        context = new MockContext(pDevice);
    }

    virtual void TearDown(void) override {
        delete context;
        CommandEnqueueFixture::TearDown();
    }

    MockContext *context;
};
} // namespace OCLRT
