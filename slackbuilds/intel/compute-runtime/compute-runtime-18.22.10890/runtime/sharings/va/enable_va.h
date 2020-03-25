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

#include "runtime/sharings/sharing_factory.h"
#include "runtime/sharings/va/va_sharing_defines.h"

#include <memory>

namespace OCLRT {
class Context;

struct VaCreateContextProperties {
    VADisplay vaDisplay = nullptr;
};

class VaSharingContextBuilder : public SharingContextBuilder {
  protected:
    std::unique_ptr<VaCreateContextProperties> contextData;

  public:
    bool processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue, cl_int &errcodeRet) override;
    bool finalizeProperties(Context &context, int32_t &errcodeRet) override;
};

class VaSharingBuilderFactory : public SharingBuilderFactory {
  public:
    std::unique_ptr<SharingContextBuilder> createContextBuilder() override;
    std::string getExtensions() override;
    void fillGlobalDispatchTable() override;
    void *getExtensionFunctionAddress(const std::string &functionName) override;
};
}
