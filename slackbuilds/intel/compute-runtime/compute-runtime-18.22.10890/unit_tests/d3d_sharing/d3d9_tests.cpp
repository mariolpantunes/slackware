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

#include "runtime/api/api.h"
#include "runtime/helpers/options.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/os_interface/windows/d3d_sharing_functions.h"
#include "runtime/sharings/d3d/cl_d3d_api.h"
#include "runtime/sharings/d3d/d3d_surface.h"
#include "test.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/mocks/mock_d3d_objects.h"
#include "unit_tests/mocks/mock_gmm.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"

namespace OCLRT {
template <>
uint32_t MockD3DSharingFunctions<D3DTypesHelper::D3D9>::getDxgiDescCalled = 0;
template <>
DXGI_ADAPTER_DESC MockD3DSharingFunctions<D3DTypesHelper::D3D9>::mockDxgiDesc = {{0}};
template <>
IDXGIAdapter *MockD3DSharingFunctions<D3DTypesHelper::D3D9>::getDxgiDescAdapterRequested = nullptr;

class D3D9Tests : public PlatformFixture, public ::testing::Test {
  public:
    typedef typename D3DTypesHelper::D3D9 D3D9;
    typedef typename D3D9::D3DDevice D3DDevice;
    typedef typename D3D9::D3DQuery D3DQuery;
    typedef typename D3D9::D3DQueryDesc D3DQueryDesc;
    typedef typename D3D9::D3DResource D3DResource;
    typedef typename D3D9::D3DTexture2dDesc D3DTexture2dDesc;
    typedef typename D3D9::D3DTexture2d D3DTexture2d;

    class MockMM : public OsAgnosticMemoryManager {
      public:
        GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, bool requireSpecificBitness, bool /*reuseBO*/) override {
            auto alloc = OsAgnosticMemoryManager::createGraphicsAllocationFromSharedHandle(handle, requireSpecificBitness, false);
            alloc->gmm = forceGmm;
            gmmOwnershipPassed = true;
            return alloc;
        }
        GraphicsAllocation *allocateGraphicsMemoryForImage(ImageInfo &imginfo, Gmm *gmm) override {
            delete gmm;
            auto alloc = OsAgnosticMemoryManager::createGraphicsAllocationFromSharedHandle(1, false);
            alloc->gmm = forceGmm;
            gmmOwnershipPassed = true;
            return alloc;
        }

        MOCK_METHOD1(lockResource, void *(GraphicsAllocation *allocation));
        MOCK_METHOD1(unlockResource, void(GraphicsAllocation *allocation));

        Gmm *forceGmm = nullptr;
        bool gmmOwnershipPassed = false;
    };

    void setupMockGmm() {
        cl_image_desc imgDesc = {};
        imgDesc.image_height = 10;
        imgDesc.image_width = 10;
        imgDesc.image_depth = 1;
        imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
        auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
        gmm = MockGmm::queryImgParams(imgInfo).release();
        mockGmmResInfo = reinterpret_cast<NiceMock<MockGmmResourceInfo> *>(gmm->gmmResourceInfo.get());

        mockMM.forceGmm = gmm;
    }

    void SetUp() override {
        dbgRestore = new DebugManagerStateRestore();
        PlatformFixture::SetUp(numPlatformDevices, platformDevices);
        context = new MockContext(pPlatform->getDevice(0));
        context->forcePreferD3dSharedResources(true);
        context->setMemoryManager(&mockMM);

        mockSharingFcns = new NiceMock<MockD3DSharingFunctions<D3D9>>();
        context->setSharingFunctions(mockSharingFcns);
        cmdQ = new MockCommandQueue(context, context->getDevice(0), 0);
        DebugManager.injectFcn = &mockSharingFcns->mockGetDxgiDesc;

        surfaceInfo.resource = (IDirect3DSurface9 *)&dummyD3DSurface;

        mockSharingFcns->mockTexture2dDesc.Format = D3DFMT_R32F;
        mockSharingFcns->mockTexture2dDesc.Height = 10;
        mockSharingFcns->mockTexture2dDesc.Width = 10;

        setupMockGmm();
    }

    void TearDown() override {
        delete cmdQ;
        delete context;
        if (!mockMM.gmmOwnershipPassed) {
            delete gmm;
        }
        PlatformFixture::TearDown();
        delete dbgRestore;
    }

    NiceMock<MockD3DSharingFunctions<D3D9>> *mockSharingFcns;
    MockContext *context;
    MockCommandQueue *cmdQ;
    DebugManagerStateRestore *dbgRestore;
    char dummyD3DSurface;
    char dummyD3DSurfaceStaging;
    cl_dx9_surface_info_khr surfaceInfo = {};

    Gmm *gmm = nullptr;
    NiceMock<MockGmmResourceInfo> *mockGmmResInfo = nullptr;
    NiceMock<MockMM> mockMM;
};

TEST_F(D3D9Tests, givenD3DDeviceParamWhenContextCreationThenSetProperValues) {
    cl_device_id deviceID = context->getDevice(0);
    cl_platform_id pid[1] = {pPlatform};
    char expectedDevice;

    cl_context_properties validAdapters[6] = {CL_CONTEXT_ADAPTER_D3D9_KHR, CL_CONTEXT_ADAPTER_D3D9EX_KHR, CL_CONTEXT_ADAPTER_DXVA_KHR,
                                              CL_CONTEXT_D3D9_DEVICE_INTEL, CL_CONTEXT_D3D9EX_DEVICE_INTEL, CL_CONTEXT_DXVA_DEVICE_INTEL};

    cl_context_properties validProperties[5] = {CL_CONTEXT_PLATFORM, (cl_context_properties)pid[0],
                                                CL_CONTEXT_ADAPTER_D3D9_KHR, (cl_context_properties)&expectedDevice, 0};

    std::unique_ptr<MockContext> ctx(nullptr);
    cl_int retVal = CL_SUCCESS;

    for (int i = 0; i < 6; i++) {
        validProperties[2] = validAdapters[i];
        ctx.reset(Context::create<MockContext>(validProperties, DeviceVector(&deviceID, 1), nullptr, nullptr, retVal));

        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, ctx.get());
        EXPECT_NE(nullptr, ctx->getSharing<D3DSharingFunctions<D3D9>>());
        EXPECT_TRUE((D3DDevice *)&expectedDevice == ctx->getSharing<D3DSharingFunctions<D3D9>>()->getDevice());
    }
}

TEST_F(D3D9Tests, getDeviceIdPartialImplementation) {
    cl_device_id expectedDevice = *devices;
    cl_device_id device = 0;
    cl_uint numDevices = 0;

    auto retVal = clGetDeviceIDsFromDX9MediaAdapterKHR(platform(), 1, nullptr, nullptr, 1, 1, &device, &numDevices);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedDevice, device);
    EXPECT_EQ(1u, numDevices);

    retVal = clGetDeviceIDsFromDX9INTEL(platform(), 1, nullptr, 1, 1, &device, &numDevices);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedDevice, device);
    EXPECT_EQ(1u, numDevices);
}

TEST_F(D3D9Tests, createSurface) {
    cl_int retVal;
    cl_image_format expectedImgFormat = {};
    OCLPlane oclPlane = OCLPlane::NO_PLANE;
    D3DSurface::findImgFormat(mockSharingFcns->mockTexture2dDesc.Format, expectedImgFormat, 0, oclPlane);

    EXPECT_CALL(*mockSharingFcns, updateDevice((IDirect3DSurface9 *)&dummyD3DSurface)).Times(1);
    EXPECT_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));

    auto memObj = clCreateFromDX9MediaSurfaceKHR(context, CL_MEM_READ_WRITE, 0, &surfaceInfo, 0, &retVal);
    ASSERT_NE(nullptr, memObj);
    EXPECT_EQ(0u, mockGmmResInfo->getOffsetCalled);

    auto image = castToObject<Image>(memObj);
    EXPECT_NE(nullptr, image->getSharingHandler());

    EXPECT_TRUE(CL_MEM_READ_WRITE == image->getFlags());

    EXPECT_TRUE(expectedImgFormat.image_channel_data_type == image->getImageFormat().image_channel_data_type);
    EXPECT_TRUE(expectedImgFormat.image_channel_order == image->getImageFormat().image_channel_order);

    EXPECT_TRUE(CL_MEM_OBJECT_IMAGE2D == image->getImageDesc().image_type);
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Width, image->getImageDesc().image_width);
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Height, image->getImageDesc().image_height);

    clReleaseMemObject(memObj);
}

TEST_F(D3D9Tests, createSurfaceIntel) {
    cl_int retVal;
    cl_image_format expectedImgFormat = {};
    OCLPlane oclPlane = OCLPlane::NO_PLANE;
    D3DSurface::findImgFormat(mockSharingFcns->mockTexture2dDesc.Format, expectedImgFormat, 0, oclPlane);

    EXPECT_CALL(*mockSharingFcns, updateDevice((IDirect3DSurface9 *)&dummyD3DSurface)).Times(1);
    EXPECT_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));

    auto memObj = clCreateFromDX9MediaSurfaceINTEL(context, CL_MEM_READ_WRITE, surfaceInfo.resource, surfaceInfo.shared_handle, 0, &retVal);
    ASSERT_NE(nullptr, memObj);

    auto image = castToObject<Image>(memObj);
    EXPECT_NE(nullptr, image->getSharingHandler());

    EXPECT_TRUE(CL_MEM_READ_WRITE == image->getFlags());

    EXPECT_TRUE(expectedImgFormat.image_channel_data_type == image->getImageFormat().image_channel_data_type);
    EXPECT_TRUE(expectedImgFormat.image_channel_order == image->getImageFormat().image_channel_order);

    EXPECT_TRUE(CL_MEM_OBJECT_IMAGE2D == image->getImageDesc().image_type);
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Width, image->getImageDesc().image_width);
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Height, image->getImageDesc().image_height);

    clReleaseMemObject(memObj);
}

TEST_F(D3D9Tests, givenUPlaneWhenCreateSurfaceThenChangeWidthHeightAndPitch) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2');
    surfaceInfo.shared_handle = (HANDLE)1;

    EXPECT_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 2, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Width / 2, sharedImg->getImageDesc().image_width);
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Height / 2, sharedImg->getImageDesc().image_height);
    size_t expectedRowPitch = static_cast<size_t>(mockGmmResInfo->getRenderPitch()) / 2;
    EXPECT_EQ(expectedRowPitch, sharedImg->getImageDesc().image_row_pitch);
}

TEST_F(D3D9Tests, givenVPlaneWhenCreateSurfaceThenChangeWidthHeightAndPitch) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2');
    surfaceInfo.shared_handle = (HANDLE)1;

    EXPECT_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 1, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Width / 2, sharedImg->getImageDesc().image_width);
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Height / 2, sharedImg->getImageDesc().image_height);
    size_t expectedRowPitch = static_cast<size_t>(mockGmmResInfo->getRenderPitch()) / 2;
    EXPECT_EQ(expectedRowPitch, sharedImg->getImageDesc().image_row_pitch);
}

TEST_F(D3D9Tests, givenUVPlaneWhenCreateSurfaceThenChangeWidthHeightAndPitch) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2');
    surfaceInfo.shared_handle = (HANDLE)1;

    EXPECT_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 1, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Width / 2, sharedImg->getImageDesc().image_width);
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Height / 2, sharedImg->getImageDesc().image_height);
    size_t expectedRowPitch = static_cast<size_t>(mockGmmResInfo->getRenderPitch());
    EXPECT_EQ(expectedRowPitch, sharedImg->getImageDesc().image_row_pitch);
}

TEST_F(D3D9Tests, givenYPlaneWhenCreateSurfaceThenDontChangeWidthHeightAndPitch) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2');
    surfaceInfo.shared_handle = (HANDLE)1;

    EXPECT_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Width, sharedImg->getImageDesc().image_width);
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Height, sharedImg->getImageDesc().image_height);
    size_t expectedRowPitch = static_cast<size_t>(mockGmmResInfo->getRenderPitch());
    EXPECT_EQ(expectedRowPitch, sharedImg->getImageDesc().image_row_pitch);
}

TEST_F(D3D9Tests, givenUPlaneWhenCreateNonSharedSurfaceThenChangeWidthHeightAndPitch) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2');
    surfaceInfo.shared_handle = (HANDLE)0;

    EXPECT_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 2, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Width / 2, sharedImg->getImageDesc().image_width);
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Height / 2, sharedImg->getImageDesc().image_height);
    size_t expectedRowPitch = static_cast<size_t>(mockGmmResInfo->getRenderPitch());
    EXPECT_EQ(expectedRowPitch, sharedImg->getImageDesc().image_row_pitch);
}

TEST_F(D3D9Tests, givenVPlaneWhenCreateNonSharedSurfaceThenChangeWidthHeightAndPitch) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2');
    surfaceInfo.shared_handle = (HANDLE)0;

    EXPECT_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 1, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Width / 2, sharedImg->getImageDesc().image_width);
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Height / 2, sharedImg->getImageDesc().image_height);
    size_t expectedRowPitch = static_cast<size_t>(mockGmmResInfo->getRenderPitch());
    EXPECT_EQ(expectedRowPitch, sharedImg->getImageDesc().image_row_pitch);
}

TEST_F(D3D9Tests, givenUVPlaneWhenCreateNonSharedSurfaceThenChangeWidthHeightAndPitch) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2');
    surfaceInfo.shared_handle = (HANDLE)0;

    EXPECT_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 1, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Width / 2, sharedImg->getImageDesc().image_width);
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Height / 2, sharedImg->getImageDesc().image_height);
    size_t expectedRowPitch = static_cast<size_t>(mockGmmResInfo->getRenderPitch());
    EXPECT_EQ(expectedRowPitch, sharedImg->getImageDesc().image_row_pitch);
}

TEST_F(D3D9Tests, givenYPlaneWhenCreateNonSharedSurfaceThenDontChangeWidthHeightAndPitch) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2');
    surfaceInfo.shared_handle = (HANDLE)0;

    EXPECT_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Width, sharedImg->getImageDesc().image_width);
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Height, sharedImg->getImageDesc().image_height);
    size_t expectedRowPitch = static_cast<size_t>(mockGmmResInfo->getRenderPitch());
    EXPECT_EQ(expectedRowPitch, sharedImg->getImageDesc().image_row_pitch);
}

TEST_F(D3D9Tests, givenNV12FormatAndInvalidPlaneWhenSurfaceIsCreatedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2');
    surfaceInfo.shared_handle = (HANDLE)0;
    ON_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).WillByDefault(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));

    auto img = D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 2, &retVal);
    EXPECT_EQ(nullptr, img);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(D3D9Tests, givenYV12FormatAndInvalidPlaneWhenSurfaceIsCreatedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2');
    surfaceInfo.shared_handle = (HANDLE)0;
    ON_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).WillByDefault(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));

    auto img = D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 3, &retVal);
    EXPECT_EQ(nullptr, img);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(D3D9Tests, givenNonPlaneFormatAndNonZeroPlaneWhenSurfaceIsCreatedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    mockSharingFcns->mockTexture2dDesc.Format = D3DFORMAT::D3DFMT_A16B16G16R16;
    surfaceInfo.shared_handle = (HANDLE)0;
    ON_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).WillByDefault(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));

    auto img = D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 1, &retVal);
    EXPECT_EQ(nullptr, img);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(D3D9Tests, givenNullResourceWhenSurfaceIsCreatedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    auto img = clCreateFromDX9MediaSurfaceINTEL(context, CL_MEM_READ_WRITE, nullptr, 0, 0, &retVal);
    EXPECT_EQ(nullptr, img);
    EXPECT_EQ(CL_INVALID_DX9_RESOURCE_INTEL, retVal);
}

TEST_F(D3D9Tests, givenNonDefaultPoolWhenSurfaceIsCreatedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    mockSharingFcns->mockTexture2dDesc.Pool = D3DPOOL_SYSTEMMEM;
    ON_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).WillByDefault(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));

    auto img = D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 1, &retVal);
    EXPECT_EQ(nullptr, img);
    EXPECT_EQ(CL_INVALID_DX9_RESOURCE_INTEL, retVal);
}

TEST_F(D3D9Tests, givenAlreadyUsedSurfaceWhenSurfaceIsCreatedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    surfaceInfo.resource = (IDirect3DSurface9 *)1;
    ON_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).WillByDefault(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));

    std::unique_ptr<Image> img(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, &retVal));
    EXPECT_NE(nullptr, img.get());

    img.reset(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, &retVal));
    EXPECT_EQ(nullptr, img.get());
    EXPECT_EQ(CL_INVALID_DX9_RESOURCE_INTEL, retVal);
}

TEST_F(D3D9Tests, givenNotSupportedFormatWhenSurfaceIsCreatedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('I', '4', '2', '0');
    surfaceInfo.shared_handle = (HANDLE)0;
    ON_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).WillByDefault(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));

    auto img = D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, &retVal);
    EXPECT_EQ(nullptr, img);
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
}

TEST_F(D3D9Tests, getMemObjInfo) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2');
    cl_dx9_media_adapter_type_khr adapterType = 0;
    cl_dx9_media_adapter_type_khr expectedAdapterType = 5;
    cl_uint plane = 0;
    cl_uint expectedPlane = 2;
    cl_dx9_surface_info_khr getSurfaceInfo = {};
    surfaceInfo.shared_handle = (HANDLE)1;
    size_t retSize = 0;

    EXPECT_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE,
                                                               expectedAdapterType, expectedPlane, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());

    sharedImg->getMemObjectInfo(CL_MEM_DX9_MEDIA_SURFACE_INFO_KHR, sizeof(cl_dx9_surface_info_khr), &getSurfaceInfo, &retSize);
    EXPECT_EQ(getSurfaceInfo.resource, surfaceInfo.resource);
    EXPECT_EQ(getSurfaceInfo.shared_handle, surfaceInfo.shared_handle);
    EXPECT_EQ(sizeof(cl_dx9_surface_info_khr), retSize);

    retSize = 0u;
    getSurfaceInfo = {};
    sharedImg->getMemObjectInfo(CL_MEM_DX9_RESOURCE_INTEL, sizeof(IDirect3DSurface9 *), &getSurfaceInfo.resource, &retSize);
    EXPECT_EQ(getSurfaceInfo.resource, surfaceInfo.resource);
    EXPECT_EQ(sizeof(IDirect3DSurface9 *), retSize);

    retSize = 0u;
    sharedImg->getMemObjectInfo(CL_MEM_DX9_SHARED_HANDLE_INTEL, sizeof(IDirect3DSurface9 *), &getSurfaceInfo.shared_handle, &retSize);
    EXPECT_EQ(getSurfaceInfo.shared_handle, surfaceInfo.shared_handle);
    EXPECT_EQ(sizeof(IDirect3DSurface9 *), retSize);

    retSize = 0u;
    sharedImg->getMemObjectInfo(CL_MEM_DX9_MEDIA_ADAPTER_TYPE_KHR, sizeof(cl_dx9_media_adapter_type_khr), &adapterType, &retSize);
    EXPECT_EQ(expectedAdapterType, adapterType);
    EXPECT_EQ(sizeof(cl_dx9_media_adapter_type_khr), retSize);

    retSize = 0u;
    sharedImg->getImageInfo(CL_IMAGE_DX9_MEDIA_PLANE_KHR, sizeof(cl_uint), &plane, &retSize);
    EXPECT_EQ(expectedPlane, plane);
    EXPECT_EQ(sizeof(cl_uint), retSize);

    retSize = 0u;
    sharedImg->getImageInfo(CL_IMAGE_DX9_PLANE_INTEL, sizeof(cl_uint), &plane, &retSize);
    EXPECT_EQ(expectedPlane, plane);
    EXPECT_EQ(sizeof(cl_uint), retSize);
}

TEST_F(D3D9Tests, givenSharedHandleWhenCreateThenDontCreateStagingSurface) {
    surfaceInfo.shared_handle = (HANDLE)1;

    ::testing::InSequence is;
    EXPECT_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));
    EXPECT_CALL(*mockSharingFcns, createTexture2d(_, _, _)).Times(0);
    EXPECT_CALL(*mockSharingFcns, addRef(_)).Times(1);

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(1u, mockGmmResInfo->getOffsetCalled);
    EXPECT_EQ(0u, mockGmmResInfo->arrayIndexPassedToGetOffset);

    auto surface = static_cast<D3DSurface *>(sharedImg->getSharingHandler().get());
    EXPECT_TRUE(surface->isSharedResource());
    EXPECT_EQ(nullptr, surface->getResourceStaging());
}

TEST_F(D3D9Tests, givenZeroSharedHandleAndLockableFlagWhenCreateThenDontCreateStagingSurface) {
    surfaceInfo.shared_handle = (HANDLE)0;
    mockSharingFcns->mockTexture2dDesc.Usage = 0;

    EXPECT_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));
    EXPECT_CALL(*mockSharingFcns, createTexture2d(_, _, _)).Times(0);
    EXPECT_CALL(*mockSharingFcns, addRef(_)).Times(1);

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(0u, mockGmmResInfo->getOffsetCalled);

    auto surface = static_cast<D3DSurface *>(sharedImg->getSharingHandler().get());
    EXPECT_FALSE(surface->isSharedResource());
    EXPECT_EQ(nullptr, surface->getResourceStaging());
    EXPECT_TRUE(surface->lockable);
}

TEST_F(D3D9Tests, givenZeroSharedHandleAndNonLockableFlagWhenCreateThenCreateStagingSurface) {
    surfaceInfo.shared_handle = (HANDLE)0;
    mockSharingFcns->mockTexture2dDesc.Usage = D3DResourceFlags::USAGE_RENDERTARGET;

    EXPECT_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));
    EXPECT_CALL(*mockSharingFcns, createTexture2d(_, _, _)).Times(1).WillOnce(SetArgPointee<0>((D3DTexture2d *)&dummyD3DSurfaceStaging));
    EXPECT_CALL(*mockSharingFcns, addRef(_)).Times(1);

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(0u, mockGmmResInfo->getOffsetCalled);

    auto surface = static_cast<D3DSurface *>(sharedImg->getSharingHandler().get());
    EXPECT_FALSE(surface->isSharedResource());
    EXPECT_NE(nullptr, surface->getResourceStaging());
    EXPECT_FALSE(surface->lockable);
}

TEST_F(D3D9Tests, acquireReleaseOnSharedResourceSurfaceAndEnabledInteropUserSync) {
    context->setInteropUserSyncEnabled(true);
    surfaceInfo.shared_handle = (HANDLE)1;

    ::testing::InSequence is;
    EXPECT_CALL(*mockSharingFcns, updateDevice((IDirect3DSurface9 *)&dummyD3DSurface)).Times(1);
    EXPECT_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));
    EXPECT_CALL(*mockSharingFcns, getRenderTargetData(_, _)).Times(0);
    EXPECT_CALL(*mockSharingFcns, lockRect(_, _, _)).Times(0);
    EXPECT_CALL(mockMM, lockResource(_)).Times(0);
    EXPECT_CALL(*mockGmmResInfo, cpuBlt(_)).Times(0);
    EXPECT_CALL(mockMM, unlockResource(_)).Times(0);
    EXPECT_CALL(*mockSharingFcns, unlockRect(_)).Times(0);
    EXPECT_CALL(*mockSharingFcns, flushAndWait(_)).Times(0);
    EXPECT_CALL(*mockSharingFcns, updateSurface(_, _)).Times(0);

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg);
    EXPECT_EQ(1u, mockGmmResInfo->getOffsetCalled);
    EXPECT_EQ(0u, mockGmmResInfo->arrayIndexPassedToGetOffset);
    cl_mem clMem = sharedImg.get();

    auto retVal = clEnqueueAcquireDX9MediaSurfacesKHR(cmdQ, 1, &clMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueReleaseDX9MediaSurfacesKHR(cmdQ, 1, &clMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(D3D9Tests, acquireReleaseOnSharedResourceSurfaceAndDisabledInteropUserSync) {
    context->setInteropUserSyncEnabled(false);
    surfaceInfo.shared_handle = (HANDLE)1;

    ::testing::InSequence is;
    EXPECT_CALL(*mockSharingFcns, updateDevice((IDirect3DSurface9 *)&dummyD3DSurface)).Times(1);
    EXPECT_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));
    EXPECT_CALL(*mockSharingFcns, getRenderTargetData(_, _)).Times(0);
    EXPECT_CALL(*mockSharingFcns, lockRect(_, _, _)).Times(0);
    EXPECT_CALL(mockMM, lockResource(_)).Times(0);
    EXPECT_CALL(*mockGmmResInfo, cpuBlt(_)).Times(0);
    EXPECT_CALL(mockMM, unlockResource(_)).Times(0);
    EXPECT_CALL(*mockSharingFcns, unlockRect(_)).Times(0);
    EXPECT_CALL(*mockSharingFcns, flushAndWait(_)).Times(1);
    EXPECT_CALL(*mockSharingFcns, updateSurface(_, _)).Times(0);

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(1u, mockGmmResInfo->getOffsetCalled);
    cl_mem clMem = sharedImg.get();

    auto retVal = clEnqueueAcquireDX9MediaSurfacesKHR(cmdQ, 1, &clMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueReleaseDX9MediaSurfacesKHR(cmdQ, 1, &clMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(D3D9Tests, acquireReleaseOnSharedResourceSurfaceAndDisabledInteropUserSyncIntel) {
    context->setInteropUserSyncEnabled(false);
    surfaceInfo.shared_handle = (HANDLE)1;

    ::testing::InSequence is;
    EXPECT_CALL(*mockSharingFcns, updateDevice((IDirect3DSurface9 *)&dummyD3DSurface)).Times(1);
    EXPECT_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));
    EXPECT_CALL(*mockSharingFcns, getRenderTargetData(_, _)).Times(0);
    EXPECT_CALL(*mockSharingFcns, lockRect(_, _, _)).Times(0);
    EXPECT_CALL(mockMM, lockResource(_)).Times(0);
    EXPECT_CALL(*mockGmmResInfo, cpuBlt(_)).Times(0);
    EXPECT_CALL(mockMM, unlockResource(_)).Times(0);
    EXPECT_CALL(*mockSharingFcns, unlockRect(_)).Times(0);
    EXPECT_CALL(*mockSharingFcns, flushAndWait(_)).Times(1);
    EXPECT_CALL(*mockSharingFcns, updateSurface(_, _)).Times(0);

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    cl_mem clMem = sharedImg.get();

    auto retVal = clEnqueueAcquireDX9ObjectsINTEL(cmdQ, 1, &clMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueReleaseDX9ObjectsINTEL(cmdQ, 1, &clMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(D3D9Tests, acquireReleaseOnNonSharedResourceSurfaceAndLockable) {
    surfaceInfo.shared_handle = (HANDLE)0;
    mockSharingFcns->mockTexture2dDesc.Usage = 0;
    D3DLOCKED_RECT lockedRect = {10u, (void *)100};

    ::testing::InSequence is;
    EXPECT_CALL(*mockSharingFcns, updateDevice((IDirect3DSurface9 *)&dummyD3DSurface)).Times(1);
    EXPECT_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(0u, mockGmmResInfo->getOffsetCalled);
    cl_mem clMem = sharedImg.get();
    auto imgHeight = static_cast<ULONG>(sharedImg->getImageDesc().image_height);
    void *returnedLockedRes = (void *)100;

    EXPECT_CALL(*mockSharingFcns, getRenderTargetData(_, _)).Times(0);
    EXPECT_CALL(*mockSharingFcns, lockRect((IDirect3DSurface9 *)&dummyD3DSurface, _, D3DLOCK_READONLY)).Times(1).WillOnce(SetArgPointee<1>(lockedRect));
    EXPECT_CALL(mockMM, lockResource(sharedImg->getGraphicsAllocation())).Times(1).WillOnce(::testing::Return(returnedLockedRes));

    GMM_RES_COPY_BLT requestedResCopyBlt = {};
    GMM_RES_COPY_BLT expectedResCopyBlt = {};
    expectedResCopyBlt.Sys.pData = lockedRect.pBits;
    expectedResCopyBlt.Gpu.pData = returnedLockedRes;
    expectedResCopyBlt.Sys.RowPitch = lockedRect.Pitch;
    expectedResCopyBlt.Blt.Upload = 1;
    expectedResCopyBlt.Sys.BufferSize = lockedRect.Pitch * imgHeight;

    EXPECT_CALL(*mockGmmResInfo, cpuBlt(_)).Times(1).WillOnce(::testing::Invoke([&](GMM_RES_COPY_BLT *arg) {requestedResCopyBlt = *arg; return 1; }));
    EXPECT_CALL(mockMM, unlockResource(sharedImg->getGraphicsAllocation())).Times(1);
    EXPECT_CALL(*mockSharingFcns, unlockRect((IDirect3DSurface9 *)&dummyD3DSurface)).Times(1);
    EXPECT_CALL(*mockSharingFcns, flushAndWait(_)).Times(1);

    auto retVal = clEnqueueAcquireDX9MediaSurfacesKHR(cmdQ, 1, &clMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(memcmp(&requestedResCopyBlt, &expectedResCopyBlt, sizeof(GMM_RES_COPY_BLT)) == 0);

    EXPECT_CALL(*mockSharingFcns, lockRect((IDirect3DSurface9 *)&dummyD3DSurface, _, 0u)).Times(1).WillOnce(SetArgPointee<1>(lockedRect));
    EXPECT_CALL(mockMM, lockResource(sharedImg->getGraphicsAllocation())).Times(1).WillOnce(::testing::Return(returnedLockedRes));

    requestedResCopyBlt = {};
    expectedResCopyBlt.Blt.Upload = 0;

    EXPECT_CALL(*mockGmmResInfo, cpuBlt(_)).Times(1).WillOnce(::testing::Invoke([&](GMM_RES_COPY_BLT *arg) {requestedResCopyBlt = *arg; return 1; }));
    EXPECT_CALL(mockMM, unlockResource(sharedImg->getGraphicsAllocation())).Times(1);
    EXPECT_CALL(*mockSharingFcns, unlockRect((IDirect3DSurface9 *)&dummyD3DSurface)).Times(1);
    EXPECT_CALL(*mockSharingFcns, updateSurface(_, _)).Times(0);

    retVal = clEnqueueReleaseDX9MediaSurfacesKHR(cmdQ, 1, &clMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(memcmp(&requestedResCopyBlt, &expectedResCopyBlt, sizeof(GMM_RES_COPY_BLT)) == 0);
}

TEST_F(D3D9Tests, acquireReleaseOnNonSharedResourceSurfaceAndLockableIntel) {
    surfaceInfo.shared_handle = (HANDLE)0;
    mockSharingFcns->mockTexture2dDesc.Usage = 0;
    D3DLOCKED_RECT lockedRect = {10u, (void *)100};

    ::testing::InSequence is;
    EXPECT_CALL(*mockSharingFcns, updateDevice((IDirect3DSurface9 *)&dummyD3DSurface)).Times(1);
    EXPECT_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    cl_mem clMem = sharedImg.get();
    auto imgHeight = static_cast<ULONG>(sharedImg->getImageDesc().image_height);
    void *returnedLockedRes = (void *)100;

    EXPECT_CALL(*mockSharingFcns, getRenderTargetData(_, _)).Times(0);
    EXPECT_CALL(*mockSharingFcns, lockRect((IDirect3DSurface9 *)&dummyD3DSurface, _, D3DLOCK_READONLY)).Times(1).WillOnce(SetArgPointee<1>(lockedRect));
    EXPECT_CALL(mockMM, lockResource(sharedImg->getGraphicsAllocation())).Times(1).WillOnce(::testing::Return(returnedLockedRes));

    GMM_RES_COPY_BLT requestedResCopyBlt = {};
    GMM_RES_COPY_BLT expectedResCopyBlt = {};
    expectedResCopyBlt.Sys.pData = lockedRect.pBits;
    expectedResCopyBlt.Gpu.pData = returnedLockedRes;
    expectedResCopyBlt.Sys.RowPitch = lockedRect.Pitch;
    expectedResCopyBlt.Blt.Upload = 1;
    expectedResCopyBlt.Sys.BufferSize = lockedRect.Pitch * imgHeight;

    EXPECT_CALL(*mockGmmResInfo, cpuBlt(_)).Times(1).WillOnce(::testing::Invoke([&](GMM_RES_COPY_BLT *arg) {requestedResCopyBlt = *arg; return 1; }));
    EXPECT_CALL(mockMM, unlockResource(sharedImg->getGraphicsAllocation())).Times(1);
    EXPECT_CALL(*mockSharingFcns, unlockRect((IDirect3DSurface9 *)&dummyD3DSurface)).Times(1);
    EXPECT_CALL(*mockSharingFcns, flushAndWait(_)).Times(1);

    auto retVal = clEnqueueAcquireDX9ObjectsINTEL(cmdQ, 1, &clMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(memcmp(&requestedResCopyBlt, &expectedResCopyBlt, sizeof(GMM_RES_COPY_BLT)) == 0);

    EXPECT_CALL(*mockSharingFcns, lockRect((IDirect3DSurface9 *)&dummyD3DSurface, _, 0u)).Times(1).WillOnce(SetArgPointee<1>(lockedRect));
    EXPECT_CALL(mockMM, lockResource(sharedImg->getGraphicsAllocation())).Times(1).WillOnce(::testing::Return(returnedLockedRes));

    requestedResCopyBlt = {};
    expectedResCopyBlt.Blt.Upload = 0;

    EXPECT_CALL(*mockGmmResInfo, cpuBlt(_)).Times(1).WillOnce(::testing::Invoke([&](GMM_RES_COPY_BLT *arg) {requestedResCopyBlt = *arg; return 1; }));
    EXPECT_CALL(mockMM, unlockResource(sharedImg->getGraphicsAllocation())).Times(1);
    EXPECT_CALL(*mockSharingFcns, unlockRect((IDirect3DSurface9 *)&dummyD3DSurface)).Times(1);
    EXPECT_CALL(*mockSharingFcns, updateSurface(_, _)).Times(0);

    retVal = clEnqueueReleaseDX9ObjectsINTEL(cmdQ, 1, &clMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(memcmp(&requestedResCopyBlt, &expectedResCopyBlt, sizeof(GMM_RES_COPY_BLT)) == 0);
}

TEST_F(D3D9Tests, acquireReleaseOnNonSharedResourceSurfaceAndNonLockable) {
    surfaceInfo.shared_handle = (HANDLE)0;
    mockSharingFcns->mockTexture2dDesc.Usage = D3DResourceFlags::USAGE_RENDERTARGET;
    D3DLOCKED_RECT lockedRect = {10u, (void *)100};

    ::testing::InSequence is;
    EXPECT_CALL(*mockSharingFcns, updateDevice((IDirect3DSurface9 *)&dummyD3DSurface)).Times(1);
    EXPECT_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));
    EXPECT_CALL(*mockSharingFcns, createTexture2d(_, _, _)).Times(1).WillOnce(SetArgPointee<0>((D3DTexture2d *)&dummyD3DSurfaceStaging));

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(0u, mockGmmResInfo->getOffsetCalled);
    cl_mem clMem = sharedImg.get();
    auto imgHeight = static_cast<ULONG>(sharedImg->getImageDesc().image_height);
    void *returnedLockedRes = (void *)100;

    EXPECT_CALL(*mockSharingFcns, getRenderTargetData((IDirect3DSurface9 *)&dummyD3DSurface, (IDirect3DSurface9 *)&dummyD3DSurfaceStaging)).Times(1);
    EXPECT_CALL(*mockSharingFcns, lockRect((IDirect3DSurface9 *)&dummyD3DSurfaceStaging, _, D3DLOCK_READONLY)).Times(1).WillOnce(SetArgPointee<1>(lockedRect));
    EXPECT_CALL(mockMM, lockResource(sharedImg->getGraphicsAllocation())).Times(1).WillOnce(::testing::Return(returnedLockedRes));

    GMM_RES_COPY_BLT requestedResCopyBlt = {};
    GMM_RES_COPY_BLT expectedResCopyBlt = {};
    expectedResCopyBlt.Sys.pData = lockedRect.pBits;
    expectedResCopyBlt.Gpu.pData = returnedLockedRes;
    expectedResCopyBlt.Sys.RowPitch = lockedRect.Pitch;
    expectedResCopyBlt.Blt.Upload = 1;
    expectedResCopyBlt.Sys.BufferSize = lockedRect.Pitch * imgHeight;

    EXPECT_CALL(*mockGmmResInfo, cpuBlt(_)).Times(1).WillOnce(::testing::Invoke([&](GMM_RES_COPY_BLT *arg) {requestedResCopyBlt = *arg; return 1; }));
    EXPECT_CALL(mockMM, unlockResource(sharedImg->getGraphicsAllocation())).Times(1);
    EXPECT_CALL(*mockSharingFcns, unlockRect((IDirect3DSurface9 *)&dummyD3DSurfaceStaging)).Times(1);
    EXPECT_CALL(*mockSharingFcns, flushAndWait(_)).Times(1);

    auto retVal = clEnqueueAcquireDX9MediaSurfacesKHR(cmdQ, 1, &clMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(memcmp(&requestedResCopyBlt, &expectedResCopyBlt, sizeof(GMM_RES_COPY_BLT)) == 0);

    EXPECT_CALL(*mockSharingFcns, lockRect((IDirect3DSurface9 *)&dummyD3DSurfaceStaging, _, 0)).Times(1).WillOnce(SetArgPointee<1>(lockedRect));
    EXPECT_CALL(mockMM, lockResource(sharedImg->getGraphicsAllocation())).Times(1).WillOnce(::testing::Return(returnedLockedRes));

    requestedResCopyBlt = {};
    expectedResCopyBlt.Blt.Upload = 0;

    EXPECT_CALL(*mockGmmResInfo, cpuBlt(_)).Times(1).WillOnce(::testing::Invoke([&](GMM_RES_COPY_BLT *arg) {requestedResCopyBlt = *arg; return 1; }));
    EXPECT_CALL(mockMM, unlockResource(sharedImg->getGraphicsAllocation())).Times(1);
    EXPECT_CALL(*mockSharingFcns, unlockRect((IDirect3DSurface9 *)&dummyD3DSurfaceStaging)).Times(1);
    EXPECT_CALL(*mockSharingFcns, updateSurface((IDirect3DSurface9 *)&dummyD3DSurfaceStaging, (IDirect3DSurface9 *)&dummyD3DSurface)).Times(1);

    retVal = clEnqueueReleaseDX9MediaSurfacesKHR(cmdQ, 1, &clMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(memcmp(&requestedResCopyBlt, &expectedResCopyBlt, sizeof(GMM_RES_COPY_BLT)) == 0);
}

TEST_F(D3D9Tests, givenResourcesCreatedFromDifferentDevicesWhenAcquireReleaseCalledThenUpdateDevice) {
    EXPECT_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));

    auto createdResourceDevice = (D3DDevice *)123;
    mockSharingFcns->setDevice(createdResourceDevice); // create call will pick this device
    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());

    mockSharingFcns->setDevice(nullptr); // force device change
    sharedImg->getSharingHandler()->acquire(sharedImg.get());
    EXPECT_EQ(createdResourceDevice, mockSharingFcns->getDevice());

    mockSharingFcns->setDevice(nullptr); // force device change
    sharedImg->getSharingHandler()->release(sharedImg.get());
    EXPECT_EQ(createdResourceDevice, mockSharingFcns->getDevice());
}

TEST_F(D3D9Tests, givenNullD3dDeviceWhenContextIsCreatedThenReturnErrorOnSurfaceCreation) {
    cl_device_id deviceID = context->getDevice(0);
    cl_int retVal = CL_SUCCESS;

    cl_context_properties properties[5] = {CL_CONTEXT_PLATFORM, (cl_context_properties)(cl_platform_id)pPlatform,
                                           CL_CONTEXT_ADAPTER_D3D9_KHR, 0, 0};

    std::unique_ptr<MockContext> ctx(Context::create<MockContext>(properties, DeviceVector(&deviceID, 1), nullptr, nullptr, retVal));

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, ctx->getSharing<D3DSharingFunctions<D3D9>>()->getDevice());

    auto img = D3DSurface::create(ctx.get(), nullptr, CL_MEM_READ_WRITE, 0, 0, &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, img);
}

TEST_F(D3D9Tests, givenInvalidContextWhenSurfaceIsCreatedThenReturnError) {
    cl_device_id deviceID = context->getDevice(0);
    cl_int retVal = CL_SUCCESS;

    cl_context_properties properties[5] = {CL_CONTEXT_PLATFORM, (cl_context_properties)(cl_platform_id)pPlatform, 0};

    std::unique_ptr<MockContext> ctx(Context::create<MockContext>(properties, DeviceVector(&deviceID, 1), nullptr, nullptr, retVal));

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, ctx->getSharing<D3DSharingFunctions<D3D9>>());

    auto img = D3DSurface::create(ctx.get(), nullptr, CL_MEM_READ_WRITE, 0, 0, &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, img);

    img = D3DSurface::create(nullptr, nullptr, CL_MEM_READ_WRITE, 0, 0, &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, img);
}

TEST_F(D3D9Tests, givenInvalidFlagsWhenSurfaceIsCreatedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    auto img = clCreateFromDX9MediaSurfaceINTEL(context, CL_MEM_USE_HOST_PTR, surfaceInfo.resource, surfaceInfo.shared_handle, 0, &retVal);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, img);
}

TEST_F(D3D9Tests, givenTheSameResourceAndPlaneWhenSurfaceIsCreatedThenReturnError) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2');
    surfaceInfo.shared_handle = (HANDLE)1;
    cl_int retVal = CL_SUCCESS;
    cl_uint plane = 0;

    EXPECT_CALL(*mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(mockSharingFcns->mockTexture2dDesc));

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, plane, &retVal));
    EXPECT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto sharedImg2 = D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, plane, &retVal);
    EXPECT_EQ(nullptr, sharedImg2);
    EXPECT_EQ(CL_INVALID_DX9_RESOURCE_INTEL, retVal);
}

TEST_F(D3D9Tests, fillBufferDesc) {
    D3D9::D3DBufferDesc requestedDesc = {};
    D3D9::D3DBufferDesc expectedDesc = {};

    mockSharingFcns->fillCreateBufferDesc(requestedDesc, 10);
    EXPECT_TRUE(memcmp(&requestedDesc, &expectedDesc, sizeof(D3D9::D3DBufferDesc)) == 0);
}

TEST_F(D3D9Tests, fillTexture2dDesc) {
    D3D9::D3DTexture2dDesc requestedDesc = {};
    D3D9::D3DTexture2dDesc expectedDesc = {};
    D3D9::D3DTexture2dDesc srcDesc = {};
    cl_uint subresource = 4;
    mockSharingFcns->fillCreateTexture2dDesc(requestedDesc, &srcDesc, subresource);
    EXPECT_TRUE(memcmp(&requestedDesc, &expectedDesc, sizeof(D3D9::D3DTexture2dDesc)) == 0);
}

TEST_F(D3D9Tests, fillTexture3dDesc) {
    D3D9::D3DTexture3dDesc requestedDesc = {};
    D3D9::D3DTexture3dDesc expectedDesc = {};
    D3D9::D3DTexture3dDesc srcDesc = {};
    cl_uint subresource = 4;
    mockSharingFcns->fillCreateTexture3dDesc(requestedDesc, &srcDesc, subresource);
    EXPECT_TRUE(memcmp(&requestedDesc, &expectedDesc, sizeof(D3D9::D3DTexture3dDesc)) == 0);
}

TEST_F(D3D9Tests, givenImproperPlatformWhenGettingDeviceIDsFromDX9ThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    retVal = clGetDeviceIDsFromDX9INTEL(nullptr, 1, nullptr, 1, 1, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
}

TEST_F(D3D9Tests, givenImproperCommandQueueWhenDX9ObjectsAreAcquiredThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    retVal = clEnqueueAcquireDX9ObjectsINTEL(nullptr, 1, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(D3D9Tests, givenImproperCommandQueueWhenDX9ObjectsAreReleasedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    retVal = clEnqueueReleaseDX9ObjectsINTEL(nullptr, 1, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(D3D9Tests, givenImproperPlatformWhenGettingDeviceIDsFromDX9MediaAdapterThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    retVal = clGetDeviceIDsFromDX9MediaAdapterKHR(nullptr, 1, nullptr, nullptr, 1, 1, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
}

TEST_F(D3D9Tests, givenImproperCommandQueueWhenDX9MediaSurfacesAreAcquiredThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    retVal = clEnqueueAcquireDX9MediaSurfacesKHR(nullptr, 1, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(D3D9Tests, givenImproperCommandQueueWhenDX9MediaSurfacesAreReleasedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    retVal = clEnqueueReleaseDX9MediaSurfacesKHR(nullptr, 1, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(D3D9Tests, givenImproperPlatformWhenGettingDeviceIDsFromD3D10ThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    retVal = clGetDeviceIDsFromD3D10KHR(nullptr, 0, nullptr, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
}

TEST_F(D3D9Tests, givenImproperContextWhenCreatingFromD3D10BufferThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    clCreateFromD3D10BufferKHR(nullptr, 0, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(D3D9Tests, givenImproperContextWhenCreatingFromD3D10Texture2DThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    clCreateFromD3D10Texture2DKHR(nullptr, 0, nullptr, 0u, &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(D3D9Tests, givenImproperContextWhenCreatingFromD3D10Texture3DThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    clCreateFromD3D10Texture3DKHR(nullptr, 0, nullptr, 0u, &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(D3D9Tests, givenImproperCommandQueueWhenD3D10ObjectsAreAcquiredThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    retVal = clEnqueueAcquireD3D10ObjectsKHR(nullptr, 0, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(D3D9Tests, givenImproperCommandQueueWhenD3D10ObjectsAreReleasedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    retVal = clEnqueueReleaseD3D10ObjectsKHR(nullptr, 0, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(D3D9Tests, givenImproperPlatformWhenGettingDeviceIDsFromD3D11ThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    retVal = clGetDeviceIDsFromD3D11KHR(nullptr, 0, nullptr, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
}

TEST_F(D3D9Tests, givenImproperContextWhenCreatingFromD3D11BufferThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    clCreateFromD3D11BufferKHR(nullptr, 0, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(D3D9Tests, givenImproperContextWhenCreatingFromD3D11Texture2DThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    clCreateFromD3D11Texture2DKHR(nullptr, 0, nullptr, 0u, &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(D3D9Tests, givenImproperContextWhenCreatingFromD3D11Texture3DThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    clCreateFromD3D11Texture3DKHR(nullptr, 0, nullptr, 0u, &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(D3D9Tests, givenImproperCommandQueueWhenD3D11ObjectsAreAcquiredThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    retVal = clEnqueueAcquireD3D11ObjectsKHR(nullptr, 0, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(D3D9Tests, givenImproperCommandQueueWhenD3D11ObjectsAreReleasedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    retVal = clEnqueueReleaseD3D11ObjectsKHR(nullptr, 0, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

namespace D3D9Formats {
static const std::tuple<uint32_t /*d3dFormat*/, uint32_t /*plane*/, uint32_t /*cl_channel_type*/, uint32_t /*cl_channel_order*/, OCLPlane> allImageFormats[] = {
    // input, input, output, output
    std::make_tuple(D3DFMT_R32F, 0, CL_R, CL_FLOAT, OCLPlane::NO_PLANE),
    std::make_tuple(D3DFMT_R16F, 0, CL_R, CL_HALF_FLOAT, OCLPlane::NO_PLANE),
    std::make_tuple(D3DFMT_L16, 0, CL_R, CL_UNORM_INT16, OCLPlane::NO_PLANE),
    std::make_tuple(D3DFMT_A8, 0, CL_A, CL_UNORM_INT8, OCLPlane::NO_PLANE),
    std::make_tuple(D3DFMT_L8, 0, CL_R, CL_UNORM_INT8, OCLPlane::NO_PLANE),
    std::make_tuple(D3DFMT_G32R32F, 0, CL_RG, CL_FLOAT, OCLPlane::NO_PLANE),
    std::make_tuple(D3DFMT_G16R16F, 0, CL_RG, CL_HALF_FLOAT, OCLPlane::NO_PLANE),
    std::make_tuple(D3DFMT_G16R16, 0, CL_RG, CL_UNORM_INT16, OCLPlane::NO_PLANE),
    std::make_tuple(D3DFMT_A8L8, 0, CL_RG, CL_UNORM_INT8, OCLPlane::NO_PLANE),
    std::make_tuple(D3DFMT_A32B32G32R32F, 0, CL_RGBA, CL_FLOAT, OCLPlane::NO_PLANE),
    std::make_tuple(D3DFMT_A16B16G16R16F, 0, CL_RGBA, CL_HALF_FLOAT, OCLPlane::NO_PLANE),
    std::make_tuple(D3DFMT_A16B16G16R16, 0, CL_RGBA, CL_UNORM_INT16, OCLPlane::NO_PLANE),
    std::make_tuple(D3DFMT_A8B8G8R8, 0, CL_RGBA, CL_UNORM_INT8, OCLPlane::NO_PLANE),
    std::make_tuple(D3DFMT_X8B8G8R8, 0, CL_RGBA, CL_UNORM_INT8, OCLPlane::NO_PLANE),
    std::make_tuple(D3DFMT_A8R8G8B8, 0, CL_BGRA, CL_UNORM_INT8, OCLPlane::NO_PLANE),
    std::make_tuple(D3DFMT_X8R8G8B8, 0, CL_BGRA, CL_UNORM_INT8, OCLPlane::NO_PLANE),
    std::make_tuple(MAKEFOURCC('N', 'V', '1', '2'), 0, CL_R, CL_UNORM_INT8, OCLPlane::PLANE_Y),
    std::make_tuple(MAKEFOURCC('N', 'V', '1', '2'), 1, CL_RG, CL_UNORM_INT8, OCLPlane::PLANE_UV),
    std::make_tuple(MAKEFOURCC('N', 'V', '1', '2'), 2, 0, 0, OCLPlane::NO_PLANE),
    std::make_tuple(MAKEFOURCC('Y', 'V', '1', '2'), 0, CL_R, CL_UNORM_INT8, OCLPlane::PLANE_Y),
    std::make_tuple(MAKEFOURCC('Y', 'V', '1', '2'), 1, CL_R, CL_UNORM_INT8, OCLPlane::PLANE_V),
    std::make_tuple(MAKEFOURCC('Y', 'V', '1', '2'), 2, CL_R, CL_UNORM_INT8, OCLPlane::PLANE_U),
    std::make_tuple(MAKEFOURCC('Y', 'V', '1', '2'), 3, 0, 0, OCLPlane::NO_PLANE),
    std::make_tuple(D3DFMT_YUY2, 0, CL_YUYV_INTEL, CL_UNORM_INT8, OCLPlane::NO_PLANE),
    std::make_tuple(D3DFMT_UYVY, 0, CL_UYVY_INTEL, CL_UNORM_INT8, OCLPlane::NO_PLANE),
    std::make_tuple(MAKEFOURCC('Y', 'V', 'Y', 'U'), 0, CL_YVYU_INTEL, CL_UNORM_INT8, OCLPlane::NO_PLANE),
    std::make_tuple(MAKEFOURCC('V', 'Y', 'U', 'Y'), 0, CL_VYUY_INTEL, CL_UNORM_INT8, OCLPlane::NO_PLANE),
    std::make_tuple(CL_INVALID_VALUE, 0, 0, 0, OCLPlane::NO_PLANE)};
}

struct D3D9ImageFormatTests
    : public ::testing::WithParamInterface<std::tuple<uint32_t /*d3dFormat*/, uint32_t /*plane*/, uint32_t /*cl_channel_type*/, uint32_t /*cl_channel_order*/, OCLPlane>>,
      public ::testing::Test {
};

INSTANTIATE_TEST_CASE_P(
    D3D9ImageFormatTests,
    D3D9ImageFormatTests,
    testing::ValuesIn(D3D9Formats::allImageFormats));

TEST_P(D3D9ImageFormatTests, validFormat) {
    cl_image_format imgFormat = {};
    auto format = std::get<0>(GetParam());
    auto plane = std::get<1>(GetParam());
    OCLPlane oclPlane = OCLPlane::NO_PLANE;
    auto expectedOclPlane = std::get<4>(GetParam());
    auto expectedClChannelType = static_cast<cl_channel_type>(std::get<3>(GetParam()));
    auto expectedClChannelOrder = static_cast<cl_channel_order>(std::get<2>(GetParam()));

    D3DSurface::findImgFormat((D3DFORMAT)format, imgFormat, plane, oclPlane);

    EXPECT_EQ(imgFormat.image_channel_data_type, expectedClChannelType);
    EXPECT_EQ(imgFormat.image_channel_order, expectedClChannelOrder);
    EXPECT_TRUE(oclPlane == expectedOclPlane);
}
} // namespace OCLRT
