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

#include "surface_formats.h"
#include "runtime/helpers/array_count.h"
#include "runtime/gmm_helper/gmm_lib.h"
#include "runtime/api/cl_types.h"

namespace OCLRT {

// clang-format off

//Initialize this with the required formats first.
//Append the optional one later
const SurfaceFormatInfo readOnlySurfaceFormats[] = {
    {{CL_RGBA,            CL_UNORM_INT8},     GMM_FORMAT_R8G8B8A8_UNORM_TYPE,      GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_UNORM          , 0, 4, 1, 4},
    {{CL_RGBA,            CL_UNORM_INT16},    GMM_FORMAT_R16G16B16A16_UNORM_TYPE,  GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UNORM      , 0, 4, 2, 8},
    {{CL_RGBA,            CL_SIGNED_INT8},    GMM_FORMAT_R8G8B8A8_SINT_TYPE,       GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_SINT           , 0, 4, 1, 4},
    {{CL_RGBA,            CL_SIGNED_INT16},   GMM_FORMAT_R16G16B16A16_SINT_TYPE,   GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_SINT       , 0, 4, 2, 8},
    {{CL_RGBA,            CL_SIGNED_INT32},   GMM_FORMAT_R32G32B32A32_SINT_TYPE,   GFX3DSTATE_SURFACEFORMAT_R32G32B32A32_SINT       , 0, 4, 4, 16},
    {{CL_RGBA,            CL_UNSIGNED_INT8},  GMM_FORMAT_R8G8B8A8_UINT_TYPE,       GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_UINT           , 0, 4, 1, 4},
    {{CL_RGBA,            CL_UNSIGNED_INT16}, GMM_FORMAT_R16G16B16A16_UINT_TYPE,   GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UINT       , 0, 4, 2, 8},
    {{CL_RGBA,            CL_UNSIGNED_INT32}, GMM_FORMAT_R32G32B32A32_UINT_TYPE,   GFX3DSTATE_SURFACEFORMAT_R32G32B32A32_UINT       , 0, 4, 4, 16},
    {{CL_RGBA,            CL_HALF_FLOAT},     GMM_FORMAT_R16G16B16A16_FLOAT_TYPE,  GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_FLOAT      , 0, 4, 2, 8},
    {{CL_RGBA,            CL_FLOAT},          GMM_FORMAT_R32G32B32A32_FLOAT_TYPE,  GFX3DSTATE_SURFACEFORMAT_R32G32B32A32_FLOAT      , 0, 4, 4, 16},
    {{CL_BGRA,            CL_UNORM_INT8},     GMM_FORMAT_B8G8R8A8_UNORM_TYPE,      GFX3DSTATE_SURFACEFORMAT_B8G8R8A8_UNORM          , 0, 4, 1, 4},
    {{CL_R,               CL_FLOAT},          GMM_FORMAT_R32_FLOAT_TYPE,           GFX3DSTATE_SURFACEFORMAT_R32_FLOAT               , 0, 1, 4, 4},
    {{CL_R,               CL_UNORM_INT8},     GMM_FORMAT_R8_UNORM_TYPE,            GFX3DSTATE_SURFACEFORMAT_R8_UNORM                , 0, 1, 1, 1},
    {{CL_R,               CL_UNORM_INT16},    GMM_FORMAT_R16_UNORM_TYPE,           GFX3DSTATE_SURFACEFORMAT_R16_UNORM               , 0, 1, 2, 2},
    {{CL_R,               CL_SIGNED_INT8},    GMM_FORMAT_R8_SINT_TYPE,             GFX3DSTATE_SURFACEFORMAT_R8_SINT                 , 0, 1, 1, 1},
    {{CL_R,               CL_SIGNED_INT16},   GMM_FORMAT_R16_SINT_TYPE,            GFX3DSTATE_SURFACEFORMAT_R16_SINT                , 0, 1, 2, 2},
    {{CL_R,               CL_SIGNED_INT32},   GMM_FORMAT_R32_SINT_TYPE,            GFX3DSTATE_SURFACEFORMAT_R32_SINT                , 0, 1, 4, 4},
    {{CL_R,               CL_UNSIGNED_INT8},  GMM_FORMAT_R8_UINT_TYPE,             GFX3DSTATE_SURFACEFORMAT_R8_UINT                 , 0, 1, 1, 1},
    {{CL_R,               CL_UNSIGNED_INT16}, GMM_FORMAT_R16_UINT_TYPE,            GFX3DSTATE_SURFACEFORMAT_R16_UINT                , 0, 1, 2, 2},
    {{CL_R,               CL_UNSIGNED_INT32}, GMM_FORMAT_R32_UINT_TYPE,            GFX3DSTATE_SURFACEFORMAT_R32_UINT                , 0, 1, 4, 4},
    {{CL_R,               CL_HALF_FLOAT},     GMM_FORMAT_R16_FLOAT_TYPE,           GFX3DSTATE_SURFACEFORMAT_R16_FLOAT               , 0, 1, 2, 2},
    {{CL_INTENSITY,       CL_UNORM_INT8},     GMM_FORMAT_GENERIC_8BIT,             GFX3DSTATE_SURFACEFORMAT_I8_UNORM                , 0, 1, 1, 1},
    {{CL_INTENSITY,       CL_UNORM_INT16},    GMM_FORMAT_GENERIC_16BIT,            GFX3DSTATE_SURFACEFORMAT_I16_UNORM               , 0, 1, 2, 2},
    {{CL_INTENSITY,       CL_HALF_FLOAT},     GMM_FORMAT_GENERIC_16BIT,            GFX3DSTATE_SURFACEFORMAT_I16_FLOAT               , 0, 1, 2, 2},
    {{CL_INTENSITY,       CL_FLOAT},          GMM_FORMAT_GENERIC_32BIT,            GFX3DSTATE_SURFACEFORMAT_I32_FLOAT               , 0, 1, 4, 4},
    // Read-only format:  CL_LUMINANCE
    {{CL_LUMINANCE,       CL_UNORM_INT8},     GMM_FORMAT_GENERIC_8BIT,             GFX3DSTATE_SURFACEFORMAT_R8_UNORM                , 0, 1, 1, 1},
    {{CL_LUMINANCE,       CL_UNORM_INT16},    GMM_FORMAT_GENERIC_16BIT,            GFX3DSTATE_SURFACEFORMAT_R16_UNORM               , 0, 1, 2, 2},
    {{CL_LUMINANCE,       CL_HALF_FLOAT},     GMM_FORMAT_GENERIC_16BIT,            GFX3DSTATE_SURFACEFORMAT_R16_FLOAT               , 0, 1, 2, 2},
    {{CL_LUMINANCE,       CL_FLOAT},          GMM_FORMAT_GENERIC_32BIT,            GFX3DSTATE_SURFACEFORMAT_R32_FLOAT               , 0, 1, 4, 4},
    {{CL_A,               CL_UNORM_INT8},     GMM_FORMAT_A8_UNORM_TYPE,            GFX3DSTATE_SURFACEFORMAT_A8_UNORM                , 0, 1, 1, 1},
    {{CL_A,               CL_UNORM_INT16},    GMM_FORMAT_GENERIC_16BIT,            GFX3DSTATE_SURFACEFORMAT_A16_UNORM               , 0, 1, 2, 2},
    {{CL_A,               CL_HALF_FLOAT},     GMM_FORMAT_GENERIC_16BIT,            GFX3DSTATE_SURFACEFORMAT_A16_FLOAT               , 0, 1, 2, 2},
    {{CL_A,               CL_FLOAT},          GMM_FORMAT_GENERIC_32BIT,            GFX3DSTATE_SURFACEFORMAT_A32_FLOAT               , 0, 1, 4, 4},
    {{CL_RG,              CL_UNORM_INT8},     GMM_FORMAT_R8G8_UNORM_TYPE,          GFX3DSTATE_SURFACEFORMAT_R8G8_UNORM              , 0, 2, 1, 2},
    {{CL_RG,              CL_UNORM_INT16},    GMM_FORMAT_R16G16_UNORM_TYPE,        GFX3DSTATE_SURFACEFORMAT_R16G16_UNORM            , 0, 2, 2, 4},
    {{CL_RG,              CL_SIGNED_INT8},    GMM_FORMAT_R8G8_SINT_TYPE,           GFX3DSTATE_SURFACEFORMAT_R8G8_SINT               , 0, 2, 1, 2},
    {{CL_RG,              CL_SIGNED_INT16},   GMM_FORMAT_R16G16_SINT_TYPE,         GFX3DSTATE_SURFACEFORMAT_R16G16_SINT             , 0, 2, 2, 4},
    {{CL_RG,              CL_SIGNED_INT32},   GMM_FORMAT_R32G32_SINT_TYPE,         GFX3DSTATE_SURFACEFORMAT_R32G32_SINT             , 0, 2, 4, 8},
    {{CL_RG,              CL_UNSIGNED_INT8},  GMM_FORMAT_R8G8_UINT_TYPE,           GFX3DSTATE_SURFACEFORMAT_R8G8_UINT               , 0, 2, 1, 2},
    {{CL_RG,              CL_UNSIGNED_INT16}, GMM_FORMAT_R16G16_UINT_TYPE,         GFX3DSTATE_SURFACEFORMAT_R16G16_UINT             , 0, 2, 2, 4},
    {{CL_RG,              CL_UNSIGNED_INT32}, GMM_FORMAT_R32G32_UINT_TYPE,         GFX3DSTATE_SURFACEFORMAT_R32G32_UINT             , 0, 2, 4, 8},
    {{CL_RG,              CL_HALF_FLOAT},     GMM_FORMAT_R16G16_FLOAT_TYPE,        GFX3DSTATE_SURFACEFORMAT_R16G16_FLOAT            , 0, 2, 2, 4},
    {{CL_RG,              CL_FLOAT},          GMM_FORMAT_R32G32_FLOAT_TYPE,        GFX3DSTATE_SURFACEFORMAT_R32G32_FLOAT            , 0, 2, 4, 8},
    {{CL_sRGBA,           CL_UNORM_INT8},     GMM_FORMAT_R8G8B8A8_UNORM_SRGB_TYPE, GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_UNORM_SRGB     , 0, 4, 1, 4},
    {{CL_sBGRA,           CL_UNORM_INT8},     GMM_FORMAT_B8G8R8A8_UNORM_SRGB_TYPE, GFX3DSTATE_SURFACEFORMAT_B8G8R8A8_UNORM_SRGB     , 0, 4, 1, 4},
    // SNORM formats
    {{CL_R,               CL_SNORM_INT8},     GMM_FORMAT_R8_SNORM_TYPE,            GFX3DSTATE_SURFACEFORMAT_R8_SNORM                , 0, 1, 1, 1},
    {{CL_R,               CL_SNORM_INT16},    GMM_FORMAT_R16_SNORM_TYPE,           GFX3DSTATE_SURFACEFORMAT_R16_SNORM               , 0, 1, 2, 2},
    {{CL_RG,              CL_SNORM_INT8},     GMM_FORMAT_R8G8_SNORM_TYPE,          GFX3DSTATE_SURFACEFORMAT_R8G8_SNORM              , 0, 2, 1, 2},
    {{CL_RG,              CL_SNORM_INT16},    GMM_FORMAT_R16G16_SNORM_TYPE,        GFX3DSTATE_SURFACEFORMAT_R16G16_SNORM            , 0, 2, 2, 4},
    {{CL_RGBA,            CL_SNORM_INT8},     GMM_FORMAT_R8G8B8A8_SNORM_TYPE,      GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_SNORM          , 0, 4, 1, 4},
    {{CL_RGBA,            CL_SNORM_INT16},    GMM_FORMAT_R16G16B16A16_SNORM_TYPE,  GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_SNORM      , 0, 4, 2, 8},
};

const SurfaceFormatInfo writeOnlySurfaceFormats[] = {
    {{CL_RGBA,            CL_UNORM_INT8},     GMM_FORMAT_R8G8B8A8_UNORM_TYPE,      GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_UNORM          , 0, 4, 1, 4},
    {{CL_RGBA,            CL_UNORM_INT16},    GMM_FORMAT_R16G16B16A16_UNORM_TYPE,  GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UNORM      , 0, 4, 2, 8},
    {{CL_RGBA,            CL_SIGNED_INT8},    GMM_FORMAT_R8G8B8A8_SINT_TYPE,       GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_SINT           , 0, 4, 1, 4},
    {{CL_RGBA,            CL_SIGNED_INT16},   GMM_FORMAT_R16G16B16A16_SINT_TYPE,   GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_SINT       , 0, 4, 2, 8},
    {{CL_RGBA,            CL_SIGNED_INT32},   GMM_FORMAT_R32G32B32A32_SINT_TYPE,   GFX3DSTATE_SURFACEFORMAT_R32G32B32A32_SINT       , 0, 4, 4, 16},
    {{CL_RGBA,            CL_UNSIGNED_INT8},  GMM_FORMAT_R8G8B8A8_UINT_TYPE,       GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_UINT           , 0, 4, 1, 4},
    {{CL_RGBA,            CL_UNSIGNED_INT16}, GMM_FORMAT_R16G16B16A16_UINT_TYPE,   GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UINT       , 0, 4, 2, 8},
    {{CL_RGBA,            CL_UNSIGNED_INT32}, GMM_FORMAT_R32G32B32A32_UINT_TYPE,   GFX3DSTATE_SURFACEFORMAT_R32G32B32A32_UINT       , 0, 4, 4, 16},
    {{CL_RGBA,            CL_HALF_FLOAT},     GMM_FORMAT_R16G16B16A16_FLOAT_TYPE,  GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_FLOAT      , 0, 4, 2, 8},
    {{CL_RGBA,            CL_FLOAT},          GMM_FORMAT_R32G32B32A32_FLOAT_TYPE,  GFX3DSTATE_SURFACEFORMAT_R32G32B32A32_FLOAT      , 0, 4, 4, 16},
    {{CL_BGRA,            CL_UNORM_INT8},     GMM_FORMAT_B8G8R8A8_UNORM_TYPE,      GFX3DSTATE_SURFACEFORMAT_B8G8R8A8_UNORM          , 0, 4, 1, 4},
    {{CL_R,               CL_FLOAT},          GMM_FORMAT_R32_FLOAT_TYPE,           GFX3DSTATE_SURFACEFORMAT_R32_FLOAT               , 0, 1, 4, 4},
    {{CL_R,               CL_UNORM_INT8},     GMM_FORMAT_R8_UNORM_TYPE,            GFX3DSTATE_SURFACEFORMAT_R8_UNORM                , 0, 1, 1, 1},
    {{CL_R,               CL_UNORM_INT16},    GMM_FORMAT_R16_UNORM_TYPE,           GFX3DSTATE_SURFACEFORMAT_R16_UNORM               , 0, 1, 2, 2},
    {{CL_R,               CL_SIGNED_INT8},    GMM_FORMAT_R8_SINT_TYPE,             GFX3DSTATE_SURFACEFORMAT_R8_SINT                 , 0, 1, 1, 1},
    {{CL_R,               CL_SIGNED_INT16},   GMM_FORMAT_R16_SINT_TYPE,            GFX3DSTATE_SURFACEFORMAT_R16_SINT                , 0, 1, 2, 2},
    {{CL_R,               CL_SIGNED_INT32},   GMM_FORMAT_R32_SINT_TYPE,            GFX3DSTATE_SURFACEFORMAT_R32_SINT                , 0, 1, 4, 4},
    {{CL_R,               CL_UNSIGNED_INT8},  GMM_FORMAT_R8_UINT_TYPE,             GFX3DSTATE_SURFACEFORMAT_R8_UINT                 , 0, 1, 1, 1},
    {{CL_R,               CL_UNSIGNED_INT16}, GMM_FORMAT_R16_UINT_TYPE,            GFX3DSTATE_SURFACEFORMAT_R16_UINT                , 0, 1, 2, 2},
    {{CL_R,               CL_UNSIGNED_INT32}, GMM_FORMAT_R32_UINT_TYPE,            GFX3DSTATE_SURFACEFORMAT_R32_UINT                , 0, 1, 4, 4},
    {{CL_R,               CL_HALF_FLOAT},     GMM_FORMAT_R16_FLOAT_TYPE,           GFX3DSTATE_SURFACEFORMAT_R16_FLOAT               , 0, 1, 2, 2},
    {{CL_A,               CL_UNORM_INT8},     GMM_FORMAT_A8_UNORM_TYPE,            GFX3DSTATE_SURFACEFORMAT_A8_UNORM                , 0, 1, 1, 1},
    {{CL_RG,              CL_UNORM_INT8},     GMM_FORMAT_R8G8_UNORM_TYPE,          GFX3DSTATE_SURFACEFORMAT_R8G8_UNORM              , 0, 2, 1, 2},
    {{CL_RG,              CL_UNORM_INT16},    GMM_FORMAT_R16G16_UNORM_TYPE,        GFX3DSTATE_SURFACEFORMAT_R16G16_UNORM            , 0, 2, 2, 4},
    {{CL_RG,              CL_SIGNED_INT8},    GMM_FORMAT_R8G8_SINT_TYPE,           GFX3DSTATE_SURFACEFORMAT_R8G8_SINT               , 0, 2, 1, 2},
    {{CL_RG,              CL_SIGNED_INT16},   GMM_FORMAT_R16G16_SINT_TYPE,         GFX3DSTATE_SURFACEFORMAT_R16G16_SINT             , 0, 2, 2, 4},
    {{CL_RG,              CL_SIGNED_INT32},   GMM_FORMAT_R32G32_SINT_TYPE,         GFX3DSTATE_SURFACEFORMAT_R32G32_SINT             , 0, 2, 4, 8},
    {{CL_RG,              CL_UNSIGNED_INT8},  GMM_FORMAT_R8G8_UINT_TYPE,           GFX3DSTATE_SURFACEFORMAT_R8G8_UINT               , 0, 2, 1, 2},
    {{CL_RG,              CL_UNSIGNED_INT16}, GMM_FORMAT_R16G16_UINT_TYPE,         GFX3DSTATE_SURFACEFORMAT_R16G16_UINT             , 0, 2, 2, 4},
    {{CL_RG,              CL_UNSIGNED_INT32}, GMM_FORMAT_R32G32_UINT_TYPE,         GFX3DSTATE_SURFACEFORMAT_R32G32_UINT             , 0, 2, 4, 8},
    {{CL_RG,              CL_HALF_FLOAT},     GMM_FORMAT_R16G16_FLOAT_TYPE,        GFX3DSTATE_SURFACEFORMAT_R16G16_FLOAT            , 0, 2, 2, 4},
    {{CL_RG,              CL_FLOAT},          GMM_FORMAT_R32G32_FLOAT_TYPE,        GFX3DSTATE_SURFACEFORMAT_R32G32_FLOAT            , 0, 2, 4, 8},
    // Write-only format: CL_LUMINANCE
    {{CL_LUMINANCE,       CL_UNORM_INT8},     GMM_FORMAT_GENERIC_8BIT,             GFX3DSTATE_SURFACEFORMAT_R8_UNORM                , 0, 1, 1, 1},
    {{CL_LUMINANCE,       CL_UNORM_INT16},    GMM_FORMAT_GENERIC_16BIT,            GFX3DSTATE_SURFACEFORMAT_R16_UNORM               , 0, 1, 2, 2},
    {{CL_LUMINANCE,       CL_HALF_FLOAT},     GMM_FORMAT_GENERIC_16BIT,            GFX3DSTATE_SURFACEFORMAT_R16_FLOAT               , 0, 1, 2, 2},
    {{CL_LUMINANCE,       CL_FLOAT},          GMM_FORMAT_GENERIC_32BIT,            GFX3DSTATE_SURFACEFORMAT_R32_FLOAT               , 0, 1, 4, 4},
    // SNORM formats
    {{CL_R,               CL_SNORM_INT8},     GMM_FORMAT_R8_SNORM_TYPE,            GFX3DSTATE_SURFACEFORMAT_R8_SNORM                , 0, 1, 1, 1},
    {{CL_R,               CL_SNORM_INT16},    GMM_FORMAT_R16_SNORM_TYPE,           GFX3DSTATE_SURFACEFORMAT_R16_SNORM               , 0, 1, 2, 2},
    {{CL_RG,              CL_SNORM_INT8},     GMM_FORMAT_R8G8_SNORM_TYPE,          GFX3DSTATE_SURFACEFORMAT_R8G8_SNORM              , 0, 2, 1, 2},
    {{CL_RG,              CL_SNORM_INT16},    GMM_FORMAT_R16G16_SNORM_TYPE,        GFX3DSTATE_SURFACEFORMAT_R16G16_SNORM            , 0, 2, 2, 4},
    {{CL_RGBA,            CL_SNORM_INT8},     GMM_FORMAT_R8G8B8A8_SNORM_TYPE,      GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_SNORM          , 0, 4, 1, 4},
    {{CL_RGBA,            CL_SNORM_INT16},    GMM_FORMAT_R16G16B16A16_SNORM_TYPE,  GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_SNORM      , 0, 4, 2, 8},
};

const SurfaceFormatInfo readWriteSurfaceFormats[] = {
    {{CL_RGBA,            CL_UNORM_INT8},     GMM_FORMAT_R8G8B8A8_UNORM_TYPE,      GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_UNORM          , 0, 4, 1, 4},
    {{CL_RGBA,            CL_UNORM_INT16},    GMM_FORMAT_R16G16B16A16_UNORM_TYPE,  GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UNORM      , 0, 4, 2, 8},
    {{CL_RGBA,            CL_SIGNED_INT8},    GMM_FORMAT_R8G8B8A8_SINT_TYPE,       GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_SINT           , 0, 4, 1, 4},
    {{CL_RGBA,            CL_SIGNED_INT16},   GMM_FORMAT_R16G16B16A16_SINT_TYPE,   GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_SINT       , 0, 4, 2, 8},
    {{CL_RGBA,            CL_SIGNED_INT32},   GMM_FORMAT_R32G32B32A32_SINT_TYPE,   GFX3DSTATE_SURFACEFORMAT_R32G32B32A32_SINT       , 0, 4, 4, 16},
    {{CL_RGBA,            CL_UNSIGNED_INT8},  GMM_FORMAT_R8G8B8A8_UINT_TYPE,       GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_UINT           , 0, 4, 1, 4},
    {{CL_RGBA,            CL_UNSIGNED_INT16}, GMM_FORMAT_R16G16B16A16_UINT_TYPE,   GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UINT       , 0, 4, 2, 8},
    {{CL_RGBA,            CL_UNSIGNED_INT32}, GMM_FORMAT_R32G32B32A32_UINT_TYPE,   GFX3DSTATE_SURFACEFORMAT_R32G32B32A32_UINT       , 0, 4, 4, 16},
    {{CL_RGBA,            CL_HALF_FLOAT},     GMM_FORMAT_R16G16B16A16_FLOAT_TYPE,  GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_FLOAT      , 0, 4, 2, 8},
    {{CL_RGBA,            CL_FLOAT},          GMM_FORMAT_R32G32B32A32_FLOAT_TYPE,  GFX3DSTATE_SURFACEFORMAT_R32G32B32A32_FLOAT      , 0, 4, 4, 16},
    {{CL_BGRA,            CL_UNORM_INT8},     GMM_FORMAT_B8G8R8A8_UNORM_TYPE,      GFX3DSTATE_SURFACEFORMAT_B8G8R8A8_UNORM          , 0, 4, 1, 4},
    {{CL_R,               CL_FLOAT},          GMM_FORMAT_R32_FLOAT_TYPE,           GFX3DSTATE_SURFACEFORMAT_R32_FLOAT               , 0, 1, 4, 4},
    {{CL_R,               CL_UNORM_INT8},     GMM_FORMAT_R8_UNORM_TYPE,            GFX3DSTATE_SURFACEFORMAT_R8_UNORM                , 0, 1, 1, 1},
    {{CL_R,               CL_UNORM_INT16},    GMM_FORMAT_R16_UNORM_TYPE,           GFX3DSTATE_SURFACEFORMAT_R16_UNORM               , 0, 1, 2, 2},
    {{CL_R,               CL_SIGNED_INT8},    GMM_FORMAT_R8_SINT_TYPE,             GFX3DSTATE_SURFACEFORMAT_R8_SINT                 , 0, 1, 1, 1},
    {{CL_R,               CL_SIGNED_INT16},   GMM_FORMAT_R16_SINT_TYPE,            GFX3DSTATE_SURFACEFORMAT_R16_SINT                , 0, 1, 2, 2},
    {{CL_R,               CL_SIGNED_INT32},   GMM_FORMAT_R32_SINT_TYPE,            GFX3DSTATE_SURFACEFORMAT_R32_SINT                , 0, 1, 4, 4},
    {{CL_R,               CL_UNSIGNED_INT8},  GMM_FORMAT_R8_UINT_TYPE,             GFX3DSTATE_SURFACEFORMAT_R8_UINT                 , 0, 1, 1, 1},
    {{CL_R,               CL_UNSIGNED_INT16}, GMM_FORMAT_R16_UINT_TYPE,            GFX3DSTATE_SURFACEFORMAT_R16_UINT                , 0, 1, 2, 2},
    {{CL_R,               CL_UNSIGNED_INT32}, GMM_FORMAT_R32_UINT_TYPE,            GFX3DSTATE_SURFACEFORMAT_R32_UINT                , 0, 1, 4, 4},
    {{CL_R,               CL_HALF_FLOAT},     GMM_FORMAT_R16_FLOAT_TYPE,           GFX3DSTATE_SURFACEFORMAT_R16_FLOAT               , 0, 1, 2, 2},
    {{CL_A,               CL_UNORM_INT8},     GMM_FORMAT_A8_UNORM_TYPE,            GFX3DSTATE_SURFACEFORMAT_A8_UNORM                , 0, 1, 1, 1},
    {{CL_RG,              CL_UNORM_INT8},     GMM_FORMAT_R8G8_UNORM_TYPE,          GFX3DSTATE_SURFACEFORMAT_R8G8_UNORM              , 0, 2, 1, 2},
    {{CL_RG,              CL_UNORM_INT16},    GMM_FORMAT_R16G16_UNORM_TYPE,        GFX3DSTATE_SURFACEFORMAT_R16G16_UNORM            , 0, 2, 2, 4},
    {{CL_RG,              CL_SIGNED_INT8},    GMM_FORMAT_R8G8_SINT_TYPE,           GFX3DSTATE_SURFACEFORMAT_R8G8_SINT               , 0, 2, 1, 2},
    {{CL_RG,              CL_SIGNED_INT16},   GMM_FORMAT_R16G16_SINT_TYPE,         GFX3DSTATE_SURFACEFORMAT_R16G16_SINT             , 0, 2, 2, 4},
    {{CL_RG,              CL_SIGNED_INT32},   GMM_FORMAT_R32G32_SINT_TYPE,         GFX3DSTATE_SURFACEFORMAT_R32G32_SINT             , 0, 2, 4, 8},
    {{CL_RG,              CL_UNSIGNED_INT8},  GMM_FORMAT_R8G8_UINT_TYPE,           GFX3DSTATE_SURFACEFORMAT_R8G8_UINT               , 0, 2, 1, 2},
    {{CL_RG,              CL_UNSIGNED_INT16}, GMM_FORMAT_R16G16_UINT_TYPE,         GFX3DSTATE_SURFACEFORMAT_R16G16_UINT             , 0, 2, 2, 4},
    {{CL_RG,              CL_UNSIGNED_INT32}, GMM_FORMAT_R32G32_UINT_TYPE,         GFX3DSTATE_SURFACEFORMAT_R32G32_UINT             , 0, 2, 4, 8},
    {{CL_RG,              CL_HALF_FLOAT},     GMM_FORMAT_R16G16_FLOAT_TYPE,        GFX3DSTATE_SURFACEFORMAT_R16G16_FLOAT            , 0, 2, 2, 4},
    {{CL_RG,              CL_FLOAT},          GMM_FORMAT_R32G32_FLOAT_TYPE,        GFX3DSTATE_SURFACEFORMAT_R32G32_FLOAT            , 0, 2, 4, 8},
    // Read-Write format: CL_LUMINANCE
    {{CL_LUMINANCE,       CL_UNORM_INT8},     GMM_FORMAT_GENERIC_8BIT,             GFX3DSTATE_SURFACEFORMAT_R8_UNORM                , 0, 1, 1, 1},
    {{CL_LUMINANCE,       CL_UNORM_INT16},    GMM_FORMAT_GENERIC_16BIT,            GFX3DSTATE_SURFACEFORMAT_R16_UNORM               , 0, 1, 2, 2},
    {{CL_LUMINANCE,       CL_HALF_FLOAT},     GMM_FORMAT_GENERIC_16BIT,            GFX3DSTATE_SURFACEFORMAT_R16_FLOAT               , 0, 1, 2, 2},
    {{CL_LUMINANCE,       CL_FLOAT},          GMM_FORMAT_GENERIC_32BIT,            GFX3DSTATE_SURFACEFORMAT_R32_FLOAT               , 0, 1, 4, 4},
    // SNORM formats
    {{CL_R,               CL_SNORM_INT8},     GMM_FORMAT_R8_SNORM_TYPE,            GFX3DSTATE_SURFACEFORMAT_R8_SNORM                , 0, 1, 1, 1},
    {{CL_R,               CL_SNORM_INT16},    GMM_FORMAT_R16_SNORM_TYPE,           GFX3DSTATE_SURFACEFORMAT_R16_SNORM               , 0, 1, 2, 2},
    {{CL_RG,              CL_SNORM_INT8},     GMM_FORMAT_R8G8_SNORM_TYPE,          GFX3DSTATE_SURFACEFORMAT_R8G8_SNORM              , 0, 2, 1, 2},
    {{CL_RG,              CL_SNORM_INT16},    GMM_FORMAT_R16G16_SNORM_TYPE,        GFX3DSTATE_SURFACEFORMAT_R16G16_SNORM            , 0, 2, 2, 4},
    {{CL_RGBA,            CL_SNORM_INT8},     GMM_FORMAT_R8G8B8A8_SNORM_TYPE,      GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_SNORM          , 0, 4, 1, 4},
    {{CL_RGBA,            CL_SNORM_INT16},    GMM_FORMAT_R16G16B16A16_SNORM_TYPE,  GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_SNORM      , 0, 4, 2, 8},
};

#if SUPPORT_YUV
const SurfaceFormatInfo packedYuvSurfaceFormats[] = {
    {{CL_YUYV_INTEL,      CL_UNORM_INT8},     GMM_FORMAT_YUY2,                     GFX3DSTATE_SURFACEFORMAT_YCRCB_NORMAL            , 0, 2, 1, 2},
    {{CL_UYVY_INTEL,      CL_UNORM_INT8},     GMM_FORMAT_UYVY,                     GFX3DSTATE_SURFACEFORMAT_YCRCB_SWAPY             , 0, 2, 1, 2},
    {{CL_YVYU_INTEL,      CL_UNORM_INT8},     GMM_FORMAT_YVYU,                     GFX3DSTATE_SURFACEFORMAT_YCRCB_SWAPUV            , 0, 2, 1, 2},
    {{CL_VYUY_INTEL,      CL_UNORM_INT8},     GMM_FORMAT_VYUY,                     GFX3DSTATE_SURFACEFORMAT_YCRCB_SWAPUVY           , 0, 2, 1, 2}
};

const SurfaceFormatInfo planarYuvSurfaceFormats[] = {
    {{CL_NV12_INTEL,      CL_UNORM_INT8},     GMM_FORMAT_NV12,                     GFX3DSTATE_SURFACEFORMAT_NV12                    , 0, 1, 1, 1}
};

const size_t numPackedYuvSurfaceFormats = ARRAY_COUNT(packedYuvSurfaceFormats);
const size_t numPlanarYuvSurfaceFormats = ARRAY_COUNT(planarYuvSurfaceFormats);
#endif

const SurfaceFormatInfo readOnlyDepthSurfaceFormats[] = {
    {{ CL_DEPTH,          CL_FLOAT},         GMM_FORMAT_R32_FLOAT_TYPE,           GFX3DSTATE_SURFACEFORMAT_R32_FLOAT               , 0, 1, 4, 4},
    {{ CL_DEPTH,          CL_UNORM_INT16},   GMM_FORMAT_R16_UNORM_TYPE,           GFX3DSTATE_SURFACEFORMAT_R16_UNORM               , 0, 1, 2, 2},
    {{ CL_DEPTH_STENCIL,  CL_UNORM_INT24},   GMM_FORMAT_GENERIC_32BIT,            GFX3DSTATE_SURFACEFORMAT_R24_UNORM_X8_TYPELESS   , 0, 1, 4, 4},
    {{ CL_DEPTH_STENCIL,  CL_FLOAT},         GMM_FORMAT_R32G32_FLOAT_TYPE,        GFX3DSTATE_SURFACEFORMAT_R32_FLOAT_X8X24_TYPELESS, 0, 2, 4, 8}
};

const SurfaceFormatInfo readWriteDepthSurfaceFormats[] = {
    {{ CL_DEPTH,          CL_FLOAT},         GMM_FORMAT_R32_FLOAT_TYPE,           GFX3DSTATE_SURFACEFORMAT_R32_FLOAT               , 0, 1, 4, 4},
    {{ CL_DEPTH,          CL_UNORM_INT16},   GMM_FORMAT_R16_UNORM_TYPE,           GFX3DSTATE_SURFACEFORMAT_R16_UNORM               , 0, 1, 2, 2}
};

const size_t numReadOnlyDepthSurfaceFormats  = ARRAY_COUNT(readOnlyDepthSurfaceFormats);
const size_t numReadWriteDepthSurfaceFormats = ARRAY_COUNT(readWriteDepthSurfaceFormats);


const size_t numReadOnlySurfaceFormats = ARRAY_COUNT(readOnlySurfaceFormats);
const size_t numWriteOnlySurfaceFormats = ARRAY_COUNT(writeOnlySurfaceFormats);
const size_t numReadWriteSurfaceFormats = ARRAY_COUNT(readWriteSurfaceFormats);

// clang-format on
}
