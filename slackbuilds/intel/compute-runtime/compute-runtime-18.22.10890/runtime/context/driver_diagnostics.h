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
#include "CL/cl_ext_intel.h"

namespace OCLRT {

enum PerformanceHints {
    CL_BUFFER_DOESNT_MEET_ALIGNMENT_RESTRICTIONS,
    CL_BUFFER_MEETS_ALIGNMENT_RESTRICTIONS,
    CL_BUFFER_NEEDS_ALLOCATE_MEMORY,
    CL_IMAGE_MEETS_ALIGNMENT_RESTRICTIONS,
    DRIVER_CALLS_INTERNAL_CL_FLUSH,
    PROFILING_ENABLED,
    PROFILING_ENABLED_WITH_DISABLED_PREEMPTION,
    SUBBUFFER_SHARES_MEMORY,
    CL_SVM_ALLOC_MEETS_ALIGNMENT_RESTRICTIONS,
    CL_ENQUEUE_READ_BUFFER_REQUIRES_COPY_DATA,
    CL_ENQUEUE_READ_BUFFER_DOESNT_REQUIRE_COPY_DATA,
    CL_ENQUEUE_READ_BUFFER_DOESNT_MEET_ALIGNMENT_RESTRICTIONS,
    CL_ENQUEUE_READ_BUFFER_RECT_REQUIRES_COPY_DATA,
    CL_ENQUEUE_READ_BUFFER_RECT_DOESNT_REQUIRES_COPY_DATA,
    CL_ENQUEUE_READ_BUFFER_RECT_DOESNT_MEET_ALIGNMENT_RESTRICTIONS,
    CL_ENQUEUE_WRITE_BUFFER_REQUIRES_COPY_DATA,
    CL_ENQUEUE_WRITE_BUFFER_DOESNT_REQUIRE_COPY_DATA,
    CL_ENQUEUE_WRITE_BUFFER_RECT_REQUIRES_COPY_DATA,
    CL_ENQUEUE_WRITE_BUFFER_RECT_DOESNT_REQUIRE_COPY_DATA,
    CL_ENQUEUE_READ_IMAGE_DOESNT_MEET_ALIGNMENT_RESTRICTIONS,
    CL_ENQUEUE_READ_IMAGE_DOESNT_REQUIRES_COPY_DATA,
    CL_ENQUEUE_WRITE_IMAGE_REQUIRES_COPY_DATA,
    CL_ENQUEUE_WRITE_IMAGE_DOESNT_REQUIRES_COPY_DATA,
    CL_ENQUEUE_MAP_BUFFER_REQUIRES_COPY_DATA,
    CL_ENQUEUE_MAP_BUFFER_DOESNT_REQUIRE_COPY_DATA,
    CL_ENQUEUE_MAP_IMAGE_REQUIRES_COPY_DATA,
    CL_ENQUEUE_MAP_IMAGE_DOESNT_REQUIRE_COPY_DATA,
    CL_ENQUEUE_UNMAP_MEM_OBJ_DOESNT_REQUIRE_COPY_DATA,
    CL_ENQUEUE_UNMAP_MEM_OBJ_REQUIRES_COPY_DATA,
    CL_ENQUEUE_SVM_MAP_DOESNT_REQUIRE_COPY_DATA,
    PRINTF_DETECTED_IN_KERNEL,
    NULL_LOCAL_WORKGROUP_SIZE,
    BAD_LOCAL_WORKGROUP_SIZE,
    REGISTER_PRESSURE_TOO_HIGH,
    PRIVATE_MEMORY_USAGE_TOO_HIGH,
    KERNEL_REQUIRES_COHERENCY
};

class DriverDiagnostics {
  public:
    DriverDiagnostics(cl_diagnostics_verbose_level level);
    bool validFlags(cl_diagnostics_verbose_level flags) const;
    ~DriverDiagnostics() = default;
    static const char *hintFormat[];
    static const cl_int maxHintStringSize = 1024;

  protected:
    cl_diagnostics_verbose_level verboseLevel;
};
} // namespace OCLRT