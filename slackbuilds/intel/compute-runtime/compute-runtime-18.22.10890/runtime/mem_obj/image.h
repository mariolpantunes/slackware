/*
 * Copyright (c) 2018, Intel Corporation
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
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/helpers/string.h"
#include "runtime/helpers/surface_formats.h"
#include "runtime/helpers/validators.h"

namespace OCLRT {
class Image;
struct KernelInfo;
struct SurfaceFormatInfo;

struct SurfaceOffsets {
    uint64_t offset;
    uint32_t xOffset;
    uint32_t yOffset;
    uint32_t yOffsetForUVplane;
};

typedef Image *(*ImageCreatFunc)(Context *context,
                                 cl_mem_flags flags,
                                 size_t size,
                                 void *hostPtr,
                                 const cl_image_format &imageFormat,
                                 const cl_image_desc &imageDesc,
                                 bool zeroCopy,
                                 GraphicsAllocation *graphicsAllocation,
                                 bool isImageRedescribed,
                                 bool createTiledImage,
                                 uint32_t baseMipLevel,
                                 uint32_t mipCount,
                                 const SurfaceFormatInfo *surfaceFormatInfo,
                                 const SurfaceOffsets *surfaceOffsets);

typedef struct {
    ImageCreatFunc createImageFunction;
} ImageFuncs;

class Image : public MemObj {
  public:
    const static cl_ulong maskMagic = 0xFFFFFFFFFFFFFFFFLL;
    static const cl_ulong objectMagic = MemObj::objectMagic | 0x01;

    ~Image() override;

    static Image *create(Context *context,
                         cl_mem_flags flags,
                         const SurfaceFormatInfo *surfaceFormat,
                         const cl_image_desc *imageDesc,
                         const void *hostPtr,
                         cl_int &errcodeRet);

    static Image *validateAndCreateImage(Context *context,
                                         cl_mem_flags flags,
                                         const cl_image_format *imageFormat,
                                         const cl_image_desc *imageDesc,
                                         const void *hostPtr,
                                         cl_int &errcodeRet);

    static Image *createImageHw(Context *context, cl_mem_flags flags, size_t size, void *hostPtr,
                                const cl_image_format &imageFormat, const cl_image_desc &imageDesc,
                                bool zeroCopy, GraphicsAllocation *graphicsAllocation,
                                bool isObjectRedescribed, bool createTiledImage, uint32_t baseMipLevel, uint32_t mipCount, const SurfaceFormatInfo *surfaceFormatInfo = nullptr);

    static Image *createSharedImage(Context *context, SharingHandler *sharingHandler, McsSurfaceInfo &mcsSurfaceInfo,
                                    GraphicsAllocation *graphicsAllocation, GraphicsAllocation *mcsAllocation,
                                    cl_mem_flags flags, ImageInfo &imgInfo, uint32_t cubeFaceIndex, uint32_t baseMipLevel, uint32_t mipCount);

    static cl_int validate(Context *context,
                           cl_mem_flags flags,
                           const SurfaceFormatInfo *surfaceFormat,
                           const cl_image_desc *imageDesc,
                           const void *hostPtr);
    static cl_int validateImageFormat(const cl_image_format *imageFormat);

    static int32_t validatePlanarYUV(Context *context,
                                     cl_mem_flags flags,
                                     const cl_image_desc *imageDesc,
                                     const void *hostPtr);

    static int32_t validatePackedYUV(cl_mem_flags flags, const cl_image_desc *imageDesc);

    static cl_int validateImageTraits(Context *context, cl_mem_flags flags, const cl_image_format *imageFormat, const cl_image_desc *imageDesc, const void *hostPtr);

    static size_t calculateHostPtrSize(size_t *region, size_t rowPitch, size_t slicePitch, size_t pixelSize, uint32_t imageType);

    static void calculateHostPtrOffset(size_t *imageOffset, const size_t *origin, const size_t *region, size_t rowPitch, size_t slicePitch, uint32_t imageType, size_t bytesPerPixel);

    static cl_int getImageParams(Context *context,
                                 cl_mem_flags flags,
                                 const SurfaceFormatInfo *surfaceFormat,
                                 const cl_image_desc *imageDesc,
                                 size_t *imageRowPitch,
                                 size_t *imageSlicePitch);

    static bool isImage2d(cl_mem_object_type imageType);

    static bool isImage2dOr2dArray(cl_mem_object_type imageType);

    static bool isDepthFormat(const cl_image_format &imageFormat);

    static bool hasSlices(cl_mem_object_type type) {
        return (type == CL_MEM_OBJECT_IMAGE3D) || (type == CL_MEM_OBJECT_IMAGE1D_ARRAY) || (type == CL_MEM_OBJECT_IMAGE2D_ARRAY);
    }

    cl_int getImageInfo(cl_image_info paramName,
                        size_t paramValueSize,
                        void *paramValue,
                        size_t *paramValueSizeRet);

    virtual void setImageArg(void *memory, bool isMediaBlockImage, uint32_t mipLevel) = 0;
    virtual void setMediaImageArg(void *memory) = 0;
    virtual void setMediaSurfaceRotation(void *memory) = 0;
    virtual void setSurfaceMemoryObjectControlStateIndexToMocsTable(void *memory, uint32_t value) = 0;

    const cl_image_desc &getImageDesc() const;
    const cl_image_format &getImageFormat() const;
    const SurfaceFormatInfo &getSurfaceFormatInfo() const;

    void transferDataToHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) override;
    void transferDataFromHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) override;

    Image *redescribe();
    Image *redescribeFillImage();
    ImageCreatFunc createFunction;

    uint32_t getQPitch() { return qPitch; }
    void setQPitch(uint32_t qPitch) { this->qPitch = qPitch; }
    size_t getHostPtrRowPitch() const { return hostPtrRowPitch; }
    void setHostPtrRowPitch(size_t pitch) { this->hostPtrRowPitch = pitch; }
    size_t getHostPtrSlicePitch() const { return hostPtrSlicePitch; }
    void setHostPtrSlicePitch(size_t pitch) { this->hostPtrSlicePitch = pitch; }
    size_t getImageCount() const { return imageCount; }
    void setImageCount(size_t imageCount) { this->imageCount = imageCount; }
    bool allowTiling() const override { return this->isTiledImage; }
    void setImageRowPitch(size_t rowPitch) { imageDesc.image_row_pitch = rowPitch; }
    void setImageSlicePitch(size_t slicePitch) { imageDesc.image_slice_pitch = slicePitch; }
    void setSurfaceOffsets(uint64_t offset, uint32_t xOffset, uint32_t yOffset, uint32_t yOffsetForUVPlane) {
        surfaceOffsets.offset = offset;
        surfaceOffsets.xOffset = xOffset;
        surfaceOffsets.yOffset = yOffset;
        surfaceOffsets.yOffsetForUVplane = yOffsetForUVPlane;
    }
    void getSurfaceOffsets(SurfaceOffsets &surfaceOffsetsOut) { surfaceOffsetsOut = this->surfaceOffsets; }

    void setCubeFaceIndex(uint32_t index) { cubeFaceIndex = index; }
    uint32_t getCubeFaceIndex() { return cubeFaceIndex; }
    void setMediaPlaneType(cl_uint type) { mediaPlaneType = type; }
    cl_uint getMediaPlaneType() const { return mediaPlaneType; }
    int peekBaseMipLevel() { return baseMipLevel; }
    void setBaseMipLevel(int level) { this->baseMipLevel = level; }

    uint32_t peekMipCount() { return mipCount; }
    void setMipCount(uint32_t mipCountNew) { this->mipCount = mipCountNew; }

    static const SurfaceFormatInfo *getSurfaceFormatFromTable(cl_mem_flags flags, const cl_image_format *imageFormat);
    static bool validateRegionAndOrigin(const size_t *origin, const size_t *region, const cl_image_desc &imgDesc);

    cl_int writeNV12Planes(const void *hostPtr, size_t hostPtrRowPitch);
    void setMcsSurfaceInfo(McsSurfaceInfo &info) { mcsSurfaceInfo = info; }
    const McsSurfaceInfo &getMcsSurfaceInfo() { return mcsSurfaceInfo; }
    size_t calculateOffsetForMapping(const MemObjOffsetArray &origin) const override;

    virtual void transformImage2dArrayTo3d(void *memory) = 0;
    virtual void transformImage3dTo2dArray(void *memory) = 0;

    virtual size_t getHostPtrRowPitchForMap(uint32_t mipLevel) = 0;
    virtual size_t getHostPtrSlicePitchForMap(uint32_t mipLevel) = 0;

    const bool isTiledImage;

    bool hasSameDescriptor(const cl_image_desc &imageDesc) const;
    bool hasValidParentImageFormat(const cl_image_format &imageFormat) const;

  protected:
    Image(Context *context,
          cl_mem_flags flags,
          size_t size,
          void *hostPtr,
          cl_image_format imageFormat,
          const cl_image_desc &imageDesc,
          bool zeroCopy,
          GraphicsAllocation *graphicsAllocation,
          bool isObjectRedescribed,
          bool createTiledImage,
          uint32_t baseMipLevel,
          uint32_t mipCount,
          const SurfaceFormatInfo &surfaceFormatInfo,
          const SurfaceOffsets *surfaceOffsets = nullptr);

    void getOsSpecificImageInfo(const cl_mem_info &paramName, size_t *srcParamSize, void **srcParam);

    void transferData(void *dst, size_t dstRowPitch, size_t dstSlicePitch,
                      void *src, size_t srcRowPitch, size_t srcSlicePitch,
                      std::array<size_t, 3> copyRegion, std::array<size_t, 3> copyOrigin);

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    SurfaceFormatInfo surfaceFormatInfo;
    McsSurfaceInfo mcsSurfaceInfo = {};
    uint32_t qPitch = 0;
    size_t hostPtrRowPitch = 0;
    size_t hostPtrSlicePitch = 0;
    size_t imageCount = 0;
    uint32_t cubeFaceIndex;
    cl_uint mediaPlaneType;
    SurfaceOffsets surfaceOffsets = {0};
    uint32_t baseMipLevel = 0;
    uint32_t mipCount = 1;

    static bool isValidSingleChannelFormat(const cl_image_format *imageFormat);
    static bool isValidIntensityFormat(const cl_image_format *imageFormat);
    static bool isValidLuminanceFormat(const cl_image_format *imageFormat);
    static bool isValidDepthFormat(const cl_image_format *imageFormat);
    static bool isValidDoubleChannelFormat(const cl_image_format *imageFormat);
    static bool isValidTripleChannelFormat(const cl_image_format *imageFormat);
    static bool isValidRGBAFormat(const cl_image_format *imageFormat);
    static bool isValidSRGBFormat(const cl_image_format *imageFormat);
    static bool isValidARGBFormat(const cl_image_format *imageFormat);
    static bool isValidDepthStencilFormat(const cl_image_format *imageFormat);
    static bool isValidYUVFormat(const cl_image_format *imageFormat);
    static bool hasAlphaChannel(const cl_image_format *imageFormat);
};

template <typename GfxFamily>
class ImageHw : public Image {
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

  public:
    ImageHw(Context *context,
            cl_mem_flags flags,
            size_t size,
            void *hostPtr,
            const cl_image_format &imageFormat,
            const cl_image_desc &imageDesc,
            bool zeroCopy,
            GraphicsAllocation *graphicsAllocation,
            bool isObjectRedescribed,
            bool createTiledImage,
            uint32_t baseMipLevel,
            uint32_t mipCount,
            const SurfaceFormatInfo &surfaceFormatInfo,
            const SurfaceOffsets *surfaceOffsets = nullptr)
        : Image(context, flags, size, hostPtr, imageFormat, imageDesc,
                zeroCopy, graphicsAllocation, isObjectRedescribed, createTiledImage, baseMipLevel, mipCount, surfaceFormatInfo, surfaceOffsets) {
        if (getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D ||
            getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER ||
            getImageDesc().image_type == CL_MEM_OBJECT_IMAGE2D ||
            getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY ||
            getImageDesc().image_type == CL_MEM_OBJECT_IMAGE2D_ARRAY) {
            this->imageDesc.image_depth = 0;
        }
    }

    size_t getHostPtrRowPitchForMap(uint32_t mipLevel) override;
    size_t getHostPtrSlicePitchForMap(uint32_t mipLevel) override;
    void setImageArg(void *memory, bool setAsMediaBlockImage, uint32_t mipLevel) override;
    void setAuxParamsForMultisamples(RENDER_SURFACE_STATE *surfaceState);
    void setAuxParamsForCCS(RENDER_SURFACE_STATE *surfaceState, Gmm *gmm);
    void setMediaImageArg(void *memory) override;
    void setMediaSurfaceRotation(void *memory) override;
    void setSurfaceMemoryObjectControlStateIndexToMocsTable(void *memory, uint32_t value) override;
    void appendSurfaceStateParams(RENDER_SURFACE_STATE *surfaceState);
    void transformImage2dArrayTo3d(void *memory) override;
    void transformImage3dTo2dArray(void *memory) override;
    static Image *create(Context *context,
                         cl_mem_flags flags,
                         size_t size,
                         void *hostPtr,
                         const cl_image_format &imageFormat,
                         const cl_image_desc &imageDesc,
                         bool zeroCopy,
                         GraphicsAllocation *graphicsAllocation,
                         bool isObjectRedescribed,
                         bool createTiledImage,
                         uint32_t baseMipLevel,
                         uint32_t mipCount,
                         const SurfaceFormatInfo *surfaceFormatInfo,
                         const SurfaceOffsets *surfaceOffsets) {
        UNRECOVERABLE_IF(surfaceFormatInfo == nullptr);
        auto image = new ImageHw<GfxFamily>(context,
                                            flags,
                                            size,
                                            hostPtr,
                                            imageFormat,
                                            imageDesc,
                                            zeroCopy,
                                            graphicsAllocation,
                                            isObjectRedescribed,
                                            createTiledImage,
                                            baseMipLevel,
                                            mipCount,
                                            *surfaceFormatInfo,
                                            surfaceOffsets);

        switch (imageDesc.image_type) {
        case CL_MEM_OBJECT_IMAGE1D:
        case CL_MEM_OBJECT_IMAGE1D_BUFFER:
        case CL_MEM_OBJECT_IMAGE1D_ARRAY:
            image->surfaceType = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_1D;
            break;
        default:
        case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        case CL_MEM_OBJECT_IMAGE2D:
            image->surfaceType = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_2D;
            break;
        case CL_MEM_OBJECT_IMAGE3D:
            image->surfaceType = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_3D;
            break;
        }
        return image;
    }

    static int getShaderChannelValue(int inputShaderChannel, cl_channel_order imageChannelOrder) {
        if (imageChannelOrder == CL_A) {
            if (inputShaderChannel == RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_RED ||
                inputShaderChannel == RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_GREEN ||
                inputShaderChannel == RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_BLUE) {
                return RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ZERO;
            }
        } else if (imageChannelOrder == CL_R ||
                   imageChannelOrder == CL_RA ||
                   imageChannelOrder == CL_Rx) {
            if (inputShaderChannel == RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_GREEN ||
                inputShaderChannel == RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_BLUE) {
                return RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ZERO;
            }
        } else if (imageChannelOrder == CL_RG ||
                   imageChannelOrder == CL_RGx) {
            if (inputShaderChannel == RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_BLUE) {
                return RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED_ZERO;
            }
        }
        return inputShaderChannel;
    }
    typename RENDER_SURFACE_STATE::SURFACE_TYPE surfaceType;
};
} // namespace OCLRT
