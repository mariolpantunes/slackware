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
#include "runtime/utilities/heap_allocator.h"
#include <stdint.h>
#include <memory>

namespace OCLRT {
const uintptr_t max32BitAddress = 0xffffffff;
extern bool is32BitOsAllocatorAvailable;
class Allocator32bit {
  protected:
    class OsInternals;

  public:
    Allocator32bit(uint64_t base, uint64_t size);
    Allocator32bit(Allocator32bit::OsInternals *osInternals);
    Allocator32bit();
    ~Allocator32bit();

    uint64_t allocate(size_t &size);
    uintptr_t getBase();
    int free(uint64_t ptr, size_t size);

  protected:
    std::unique_ptr<OsInternals> osInternals;
    std::unique_ptr<HeapAllocator> heapAllocator;
    uint64_t base = 0;
    uint64_t size = 0;
};
} // namespace OCLRT
