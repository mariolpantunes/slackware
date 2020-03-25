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
#include "runtime/api/cl_types.h"
#include <vector>

namespace OCLRT {
class GraphicsAllocation;
class CommandStreamReceiver;
struct KernelInfo;

class BlockKernelManager {
  public:
    BlockKernelManager() = default;
    virtual ~BlockKernelManager();
    void addBlockKernelInfo(KernelInfo *);
    const KernelInfo *getBlockKernelInfo(size_t ordinal);
    size_t getCount() const {
        return blockKernelInfoArray.size();
    }
    bool getIfBlockUsesPrintf() const {
        return blockUsesPrintf;
    }

    void pushPrivateSurface(GraphicsAllocation *allocation, size_t ordinal);
    GraphicsAllocation *getPrivateSurface(size_t ordinal);

    void makeInternalAllocationsResident(CommandStreamReceiver &);

  protected:
    bool blockUsesPrintf = false;
    std::vector<KernelInfo *> blockKernelInfoArray;
    std::vector<GraphicsAllocation *> blockPrivateSurfaceArray;
};
} // namespace OCLRT