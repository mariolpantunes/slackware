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
#include "gtest/gtest.h"
#include "CL/cl.h"
#include "runtime/device/device.h"
#include "runtime/helpers/file_io.h"
#include "runtime/kernel/kernel.h"
#include "runtime/program/program.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/program_fixture.h"

namespace OCLRT {

class Kernel;
class Program;

template <typename T>
inline const char *type_name(T &) {
    return "unknown";
}

template <>
inline const char *type_name(char &) {
    return "char";
}

template <>
inline const char *type_name(int &) {
    return "int";
}

template <>
inline const char *type_name(float &) {
    return "float";
}

template <>
inline const char *type_name(short &) {
    return "short";
}

template <>
inline const char *type_name(unsigned char &) {
    return "unsigned char";
}

template <>
inline const char *type_name(unsigned int &) {
    return "unsigned int";
}

template <>
inline const char *type_name(unsigned short &) {
    return "unsigned short";
}

class SimpleArgKernelFixture : public ProgramFixture {

  public:
    using ProgramFixture::SetUp;
    SimpleArgKernelFixture()
        : retVal(CL_SUCCESS), pKernel(nullptr) {
    }

  protected:
    virtual void SetUp(Device *pDevice) {
        ProgramFixture::SetUp();

        std::string testFile;
        int forTheName = 0;

        testFile.append("simple_arg_");
        testFile.append(type_name(forTheName));

        auto pos = testFile.find(" ");
        if (pos != (size_t)-1) {
            testFile.replace(pos, 1, "_");
        }

        cl_device_id device = pDevice;
        pContext = Context::create<MockContext>(nullptr, DeviceVector(&device, 1), nullptr, nullptr, retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, pContext);

        CreateProgramFromBinary<Program>(
            pContext,
            &device,
            testFile);
        ASSERT_NE(nullptr, pProgram);

        retVal = pProgram->build(
            1,
            &device,
            nullptr,
            nullptr,
            nullptr,
            false);
        ASSERT_EQ(CL_SUCCESS, retVal);

        // create a kernel
        pKernel = Kernel::create<MockKernel>(
            pProgram,
            *pProgram->getKernelInfo("SimpleArg"),
            &retVal);

        ASSERT_NE(nullptr, pKernel);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    virtual void TearDown() {
        delete pKernel;
        pKernel = nullptr;

        pContext->release();

        ProgramFixture::TearDown();
    }

    cl_int retVal;
    Kernel *pKernel;
    MockContext *pContext;
};
} // namespace OCLRT
