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
#include "runtime/mem_obj/image.h"
#include "unit_tests/mocks/mock_context.h"
#include "CL/cl.h"
#include <cassert>
#include <cstdio>

struct Image1dDefaults {
    enum { flags = 0 };
    static const cl_image_format imageFormat;
    static const cl_image_desc imageDesc;
    static void *hostPtr;
    static OCLRT::Context *context;
};

struct Image2dDefaults : public Image1dDefaults {
    static const cl_image_desc imageDesc;
};

struct Image3dDefaults : public Image2dDefaults {
    static const cl_image_desc imageDesc;
};

struct Image2dArrayDefaults : public Image2dDefaults {
    static const cl_image_desc imageDesc;
};

struct Image1dArrayDefaults : public Image2dDefaults {
    static const cl_image_desc imageDesc;
};

struct LuminanceImage : public Image2dDefaults {
    static const cl_image_format imageFormat;
};

template <typename BaseClass>
struct ImageUseHostPtr : public BaseClass {
    enum { flags = BaseClass::flags | CL_MEM_USE_HOST_PTR };
};

template <typename BaseClass>
struct ImageReadOnly : public BaseClass {
    enum { flags = BaseClass::flags | CL_MEM_READ_ONLY };
};

template <typename BaseClass>
struct ImageWriteOnly : public BaseClass {
    enum { flags = BaseClass::flags | CL_MEM_WRITE_ONLY };
};

template <typename Traits>
struct ImageHelper {
    using Context = OCLRT::Context;
    using Image = OCLRT::Image;
    using MockContext = OCLRT::MockContext;

    static Image *create(Context *context = Traits::context, const cl_image_desc *imgDesc = &Traits::imageDesc,
                         const cl_image_format *imgFormat = &Traits::imageFormat) {
        auto retVal = CL_INVALID_VALUE;
        auto surfaceFormat = Image::getSurfaceFormatFromTable(Traits::flags, imgFormat);
        auto image = Image::create(
            context,
            Traits::flags,
            surfaceFormat,
            imgDesc,
            Traits::hostPtr,
            retVal);

        assert(image != nullptr);
        return image;
    }
};

template <typename Traits = Image1dDefaults>
struct Image1dHelper : public ImageHelper<Traits> {
};

template <typename Traits = Image2dDefaults>
struct Image2dHelper : public ImageHelper<Traits> {
};

template <typename Traits = Image3dDefaults>
struct Image3dHelper : public ImageHelper<Traits> {
};

template <typename Traits = Image2dArrayDefaults>
struct Image2dArrayHelper : public ImageHelper<Traits> {
};

template <typename Traits = Image1dArrayDefaults>
struct Image1dArrayHelper : public ImageHelper<Traits> {
};
