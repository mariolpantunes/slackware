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

#include "unit_tests/helpers/gtest_helpers.h"
#include "test.h"
#include "hw_cmds.h"
#include "runtime/os_interface/windows/wddm_engine_mapper.h"

using namespace OCLRT;
using namespace std;

struct WddmMapperTestsGen8 : public ::testing::Test {
    void SetUp() override {}
    void TearDown() override {}
};

GEN8TEST_F(WddmMapperTestsGen8, engineNodeMapPass) {
    GPUNODE_ORDINAL gpuNode = GPUNODE_MAX;
    bool ret = WddmEngineMapper<BDWFamily>::engineNodeMap(EngineType::ENGINE_RCS, gpuNode);
    EXPECT_TRUE(ret);
    EXPECT_EQ(GPUNODE_3D, gpuNode);
}

GEN8TEST_F(WddmMapperTestsGen8, engineNodeMapNegative) {
    GPUNODE_ORDINAL gpuNode = GPUNODE_MAX;
    bool ret = WddmEngineMapper<BDWFamily>::engineNodeMap(EngineType::ENGINE_BCS, gpuNode);
    EXPECT_FALSE(ret);
}
