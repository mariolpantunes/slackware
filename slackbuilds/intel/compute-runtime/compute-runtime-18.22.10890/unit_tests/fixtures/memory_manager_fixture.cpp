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

#include "unit_tests/fixtures/memory_manager_fixture.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_memory_manager.h"

using namespace OCLRT;
using ::testing::NiceMock;

void MemoryManagerWithCsrFixture::SetUp() {
    gmockMemoryManager = new NiceMock<GMockMemoryManager>;
    memoryManager = gmockMemoryManager;

    ON_CALL(*gmockMemoryManager, cleanAllocationList(::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(gmockMemoryManager, &GMockMemoryManager::MemoryManagerCleanAllocationList));
    ON_CALL(*gmockMemoryManager, populateOsHandles(::testing::_)).WillByDefault(::testing::Invoke(gmockMemoryManager, &GMockMemoryManager::MemoryManagerPopulateOsHandles));

    csr.tagAddress = &currentGpuTag;
    memoryManager->csr = &csr;
}

void MemoryManagerWithCsrFixture::TearDown() {
    delete memoryManager;
}