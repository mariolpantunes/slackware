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

#include "offline_compiler/offline_compiler.h"
#include "runtime/os_interface/os_library.h"

#include <CL/cl.h>

using namespace OCLRT;

int main(int numArgs, const char *argv[]) {
    int retVal = CL_SUCCESS;
    OfflineCompiler *pCompiler = OfflineCompiler::create(numArgs, argv, retVal);

    if (retVal == CL_SUCCESS) {
        retVal = pCompiler->build();

        std::string buildLog = pCompiler->getBuildLog();
        if (buildLog.empty() == false) {
            printf("%s\n", buildLog.c_str());
        }

        if (retVal == CL_SUCCESS) {
            if (!pCompiler->isQuiet())
                printf("Build succeeded.\n");
        } else {
            printf("Build failed with error code: %d\n", retVal);
        }
    }

    delete pCompiler;
    return retVal;
}
