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

#include "runtime/utilities/linux/debug_env_reader.h"

namespace OCLRT {

SettingsReader *SettingsReader::createOsReader() {
    return new EnvironmentVariableReader;
}

bool EnvironmentVariableReader::getSetting(const char *settingName, bool defaultValue) {
    return getSetting(settingName, static_cast<int32_t>(defaultValue)) ? true : false;
}

int32_t EnvironmentVariableReader::getSetting(const char *settingName, int32_t defaultValue) {
    int32_t value = defaultValue;
    char *envValue;

    envValue = getenv(settingName);
    if (envValue) {
        value = atoi(envValue);
    }
    return value;
}

std::string EnvironmentVariableReader::getSetting(const char *settingName, const std::string &value) {
    char *envValue;
    std::string keyValue;
    keyValue.assign(value);

    envValue = getenv(settingName);
    if (envValue) {
        keyValue.assign(envValue);
    }
    return keyValue;
}
}
