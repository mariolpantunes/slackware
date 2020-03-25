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
#include <cstdint>
#include <cstring>
#include <string>
#include "CL/cl.h"
#include "runtime/utilities/stackvec.h"

const int maximalStackSizeSizes = 16;

inline int createCombinedString(
    std::string &dstString,
    size_t &dstStringSizeInBytes,
    uint32_t numStrings,
    const char **strings,
    const size_t *lengths) {
    int retVal = CL_SUCCESS;

    if (numStrings == 0 || strings == nullptr) {
        retVal = CL_INVALID_VALUE;
    }

    using SourceSizesT = StackVec<size_t, maximalStackSizeSizes>;
    SourceSizesT localSizes;

    if (retVal == CL_SUCCESS) {
        localSizes.resize(numStrings);
        dstStringSizeInBytes = 1;
        for (uint32_t i = 0; i < numStrings; i++) {
            if (strings[i] == nullptr) {
                retVal = CL_INVALID_VALUE;
                break;
            }
            if ((lengths == nullptr) ||
                (lengths[i] == 0)) {
                localSizes[i] = strlen((const char *)strings[i]);
            } else {
                localSizes[i] = lengths[i];
            }

            dstStringSizeInBytes += localSizes[i];
        }
    }

    if (retVal == CL_SUCCESS) {
        dstString.reserve(dstStringSizeInBytes);
        for (uint32_t i = 0; i < numStrings; i++) {
            dstString.append(strings[i], localSizes[i]);
        }
        // add the null terminator
        dstString += '\0';
    }

    return retVal;
}
