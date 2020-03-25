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

#include "runtime/helpers/debug_helpers.h"
#include "runtime/os_interface/os_library.h"
#include "os_library.h"
#include <dlfcn.h>

namespace OCLRT {
OsLibrary *OsLibrary::load(const std::string &name) {
    auto ptr = new (std::nothrow) Linux::OsLibrary(name);
    if (ptr == nullptr)
        return nullptr;

    if (!ptr->isLoaded()) {
        delete ptr;
        return nullptr;
    }
    return ptr;
}
namespace Linux {

OsLibrary::OsLibrary(const std::string &name) {
    if (name.empty()) {
        this->handle = dlopen(0, RTLD_LAZY);
    } else {
        this->handle = dlopen(name.c_str(), RTLD_LAZY);
    }
}

OsLibrary::~OsLibrary() {
    if (this->handle != nullptr) {
        dlclose(this->handle);
        this->handle = nullptr;
    }
}

bool OsLibrary::isLoaded() {
    return this->handle != nullptr;
}

void *OsLibrary::getProcAddress(const std::string &procName) {
    DEBUG_BREAK_IF(this->handle == nullptr);

    return dlsym(this->handle, procName.c_str());
}
}
}
