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
#include "sku_info.h"
#include "runtime/os_interface/windows/windows_wrapper.h"
#include "umKmInc/sharedata.h"

namespace OCLRT {
class SkuInfoReceiver {
  public:
    static void receiveFtrTableFromAdapterInfo(FeatureTable *ftrTable, _ADAPTER_INFO *adapterInfo);
    static void receiveWaTableFromAdapterInfo(WorkaroundTable *waTable, _ADAPTER_INFO *adapterInfo);

  protected:
    static void receiveFtrTableFromAdapterInfoBase(FeatureTable *ftrTable, _ADAPTER_INFO *adapterInfo) {
#define RECEIVE_FTR(VAL_NAME) ftrTable->ftr##VAL_NAME = adapterInfo->SkuTable.Ftr##VAL_NAME
        RECEIVE_FTR(Desktop);
        RECEIVE_FTR(ChannelSwizzlingXOREnabled);

        RECEIVE_FTR(GtBigDie);
        RECEIVE_FTR(GtMediumDie);
        RECEIVE_FTR(GtSmallDie);

        RECEIVE_FTR(GT1);
        RECEIVE_FTR(GT1_5);
        RECEIVE_FTR(GT2);
        RECEIVE_FTR(GT2_5);
        RECEIVE_FTR(GT3);
        RECEIVE_FTR(GT4);

        RECEIVE_FTR(IVBM0M1Platform);
        RECEIVE_FTR(SGTPVSKUStrapPresent);
        RECEIVE_FTR(GTA);
        RECEIVE_FTR(GTC);
        RECEIVE_FTR(GTX);
        RECEIVE_FTR(5Slice);

        RECEIVE_FTR(GpGpuMidBatchPreempt);
        RECEIVE_FTR(GpGpuThreadGroupLevelPreempt);
        RECEIVE_FTR(GpGpuMidThreadLevelPreempt);

        RECEIVE_FTR(IoMmuPageFaulting);
        RECEIVE_FTR(Wddm2Svm);
        RECEIVE_FTR(PooledEuEnabled);

        RECEIVE_FTR(ResourceStreamer);

        RECEIVE_FTR(PPGTT);
        RECEIVE_FTR(SVM);
        RECEIVE_FTR(EDram);
        RECEIVE_FTR(L3IACoherency);
        RECEIVE_FTR(IA32eGfxPTEs);

        RECEIVE_FTR(3dMidBatchPreempt);
        RECEIVE_FTR(3dObjectLevelPreempt);
        RECEIVE_FTR(PerCtxtPreemptionGranularityControl);

        RECEIVE_FTR(DisplayYTiling);
        RECEIVE_FTR(TranslationTable);
        RECEIVE_FTR(UserModeTranslationTable);

        RECEIVE_FTR(EnableGuC);

        RECEIVE_FTR(Fbc);
        RECEIVE_FTR(Fbc2AddressTranslation);
        RECEIVE_FTR(FbcBlitterTracking);
        RECEIVE_FTR(FbcCpuTracking);

        RECEIVE_FTR(Vcs2);
        RECEIVE_FTR(VEBOX);
        RECEIVE_FTR(SingleVeboxSlice);
        RECEIVE_FTR(ULT);
        RECEIVE_FTR(LCIA);
        RECEIVE_FTR(GttCacheInvalidation);
        RECEIVE_FTR(TileMappedResource);
        RECEIVE_FTR(AstcHdr2D);
        RECEIVE_FTR(AstcLdr2D);

        RECEIVE_FTR(StandardMipTailFormat);
        RECEIVE_FTR(FrameBufferLLC);
        RECEIVE_FTR(Crystalwell);
        RECEIVE_FTR(LLCBypass);
        RECEIVE_FTR(DisplayEngineS3d);
        RECEIVE_FTR(VERing);
        RECEIVE_FTR(Wddm2GpuMmu);
        RECEIVE_FTR(Wddm2_1_64kbPages);

        RECEIVE_FTR(KmdDaf);
#undef RECEIVE_FTR
    }

    static void receiveWaTableFromAdapterInfoBase(WorkaroundTable *waTable, _ADAPTER_INFO *adapterInfo) {
#define RECEIVE_WA(VAL_NAME) waTable->wa##VAL_NAME = adapterInfo->WaTable.Wa##VAL_NAME
        RECEIVE_WA(DoNotUseMIReportPerfCount);

        RECEIVE_WA(EnablePreemptionGranularityControlByUMD);
        RECEIVE_WA(SendMIFLUSHBeforeVFE);
        RECEIVE_WA(ReportPerfCountUseGlobalContextID);
        RECEIVE_WA(DisableLSQCROPERFforOCL);
        RECEIVE_WA(Msaa8xTileYDepthPitchAlignment);
        RECEIVE_WA(LosslessCompressionSurfaceStride);
        RECEIVE_WA(FbcLinearSurfaceStride);
        RECEIVE_WA(4kAlignUVOffsetNV12LinearSurface);
        RECEIVE_WA(EncryptedEdramOnlyPartials);
        RECEIVE_WA(DisableEdramForDisplayRT);
        RECEIVE_WA(ForcePcBbFullCfgRestore);
        RECEIVE_WA(CompressedResourceRequiresConstVA21);
        RECEIVE_WA(DisablePerCtxtPreemptionGranularityControl);
        RECEIVE_WA(LLCCachingUnsupported);
        RECEIVE_WA(UseVAlign16OnTileXYBpp816);
        RECEIVE_WA(ModifyVFEStateAfterGPGPUPreemption);
        RECEIVE_WA(CSRUncachable);
        RECEIVE_WA(SamplerCacheFlushBetweenRedescribedSurfaceReads);
#undef RECEIVE_WA
    }
};

} // namespace OCLRT
