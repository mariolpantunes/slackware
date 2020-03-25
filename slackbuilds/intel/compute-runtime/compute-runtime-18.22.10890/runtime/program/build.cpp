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

#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/compiler_interface/compiler_options.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/platform/platform.h"
#include "runtime/source_level_debugger/source_level_debugger.h"
#include "runtime/helpers/validators.h"
#include "program.h"
#include <cstring>

namespace OCLRT {

cl_int Program::build(
    cl_uint numDevices,
    const cl_device_id *deviceList,
    const char *buildOptions,
    void(CL_CALLBACK *funcNotify)(cl_program program, void *userData),
    void *userData,
    bool enableCaching) {
    cl_int retVal = CL_SUCCESS;

    do {
        if (((deviceList == nullptr) && (numDevices != 0)) ||
            ((deviceList != nullptr) && (numDevices == 0))) {
            retVal = CL_INVALID_VALUE;
            break;
        }

        if ((funcNotify == nullptr) &&
            (userData != nullptr)) {
            retVal = CL_INVALID_VALUE;
            break;
        }

        // if a device_list is specified, make sure it points to our device
        // NOTE: a null device_list is ok - it means "all devices"
        if (deviceList && validateObject(*deviceList) != CL_SUCCESS) {
            retVal = CL_INVALID_DEVICE;
            break;
        }

        // check to see if a previous build request is in progress
        if (buildStatus == CL_BUILD_IN_PROGRESS) {
            retVal = CL_INVALID_OPERATION;
            break;
        }

        if (isCreatedFromBinary == false) {
            buildStatus = CL_BUILD_IN_PROGRESS;

            options = (buildOptions) ? buildOptions : "";
            std::string reraStr = "-cl-intel-gtpin-rera";
            size_t pos = options.find(reraStr);
            if (pos != std::string::npos) {
                // build option "-cl-intel-gtpin-rera" is present, move it to internalOptions
                size_t reraLen = reraStr.length();
                options.erase(pos, reraLen);
                internalOptions.append(reraStr);
                internalOptions.append(" ");
            }

            CompilerInterface *pCompilerInterface = getCompilerInterface();
            if (!pCompilerInterface) {
                retVal = CL_OUT_OF_HOST_MEMORY;
                break;
            }

            TranslationArgs inputArgs = {};
            if (strcmp(sourceCode.c_str(), "") == 0) {
                retVal = CL_INVALID_PROGRAM;
                break;
            }

            if (isKernelDebugEnabled()) {
                internalOptions.append(CompilerOptions::debugKernelEnable);
                options.append(" -g ");
                if (pDevice->getSourceLevelDebugger()) {
                    if (pDevice->getSourceLevelDebugger()->isOptimizationDisabled()) {
                        options.append("-cl-opt-disable ");
                    }
                    std::string filename;
                    pDevice->getSourceLevelDebugger()->notifySourceCode(sourceCode.c_str(), sourceCode.size(), filename);
                    if (!filename.empty()) {
                        // Add "-s" flag first so it will be ignored by clang in case the options already have this flag set.
                        options = std::string("-s ") + filename + " " + options;
                    }
                }
            }

            internalOptions.append(platform()->peekCompilerExtensions());

            inputArgs.pInput = (char *)(sourceCode.c_str());
            inputArgs.InputSize = (uint32_t)sourceCode.size();
            inputArgs.pOptions = options.c_str();
            inputArgs.OptionsSize = (uint32_t)options.length();
            inputArgs.pInternalOptions = internalOptions.c_str();
            inputArgs.InternalOptionsSize = (uint32_t)internalOptions.length();
            inputArgs.pTracingOptions = nullptr;
            inputArgs.TracingOptionsCount = 0;
            DBG_LOG(LogApiCalls,
                    "Build Options", inputArgs.pOptions,
                    "\nBuild Internal Options", inputArgs.pInternalOptions);

            retVal = pCompilerInterface->build(*this, inputArgs, enableCaching);
            if (retVal != CL_SUCCESS) {
                break;
            }
        }
        updateNonUniformFlag();

        retVal = processGenBinary();
        if (retVal != CL_SUCCESS) {
            break;
        }

        if (isKernelDebugEnabled()) {
            processDebugData();
            if (pDevice->getSourceLevelDebugger()) {
                for (size_t i = 0; i < kernelInfoArray.size(); i++) {
                    pDevice->getSourceLevelDebugger()->notifyKernelDebugData(kernelInfoArray[i]);
                }
            }
        }

        separateBlockKernels();
    } while (false);

    if (retVal != CL_SUCCESS) {
        buildStatus = CL_BUILD_ERROR;
        programBinaryType = CL_PROGRAM_BINARY_TYPE_NONE;
    } else {
        buildStatus = CL_BUILD_SUCCESS;
        programBinaryType = CL_PROGRAM_BINARY_TYPE_EXECUTABLE;
    }

    if (funcNotify != nullptr) {
        (*funcNotify)(this, userData);
    }

    return retVal;
}

cl_int Program::build(const cl_device_id device, const char *buildOptions, bool enableCaching,
                      std::unordered_map<std::string, BuiltinDispatchInfoBuilder *> &builtinsMap) {
    auto ret = this->build(1, &device, buildOptions, nullptr, nullptr, enableCaching);
    if (ret != CL_SUCCESS) {
        return ret;
    }

    for (auto &ki : this->kernelInfoArray) {
        auto fit = builtinsMap.find(ki->name);
        if (fit == builtinsMap.end()) {
            continue;
        }
        ki->builtinDispatchBuilder = fit->second;
    }
    return ret;
}

cl_int Program::build(
    const char *pKernelData,
    size_t kernelDataSize) {
    cl_int retVal = CL_SUCCESS;
    processKernel(pKernelData, retVal);

    return retVal;
}
} // namespace OCLRT
