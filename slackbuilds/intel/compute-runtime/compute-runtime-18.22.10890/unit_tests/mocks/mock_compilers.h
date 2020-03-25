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

#pragma once

#include "runtime/compiler_interface/compiler_interface.h"
#include "unit_tests/mocks/mock_cif.h"

#include "ocl_igc_interface/igc_ocl_device_ctx.h"
#include "ocl_igc_interface/fcl_ocl_device_ctx.h"

#include <functional>
#include <map>
#include <string>

namespace OCLRT {

struct MockCompilerDebugVars {
    bool forceBuildFailure = false;
    bool forceCreateFailure = false;
    bool forceRegisterFail = false;
    bool internalOptionsExpected = false;
    bool appendOptionsToFileName = true;
    void *debugDataToReturn = nullptr;
    size_t debugDataToReturnSize = 0;
    bool failCreatePlatformInterface = false;
    bool failCreateGtSystemInfoInterface = false;
    bool failCreateIgcFeWaInterface = false;
    std::string *receivedInternalOptionsOutput = nullptr;
    std::string *receivedInput = nullptr;

    std::string fileName;
};

struct MockCompilerEnableGuard {
    MockCompilerEnableGuard(bool autoEnable = false);
    ~MockCompilerEnableGuard();

    void Enable();
    void Disable();

    const char *oldFclDllName;
    const char *oldIgcDllName;

    bool enabled = false;
};

void setFclDebugVars(MockCompilerDebugVars &dbgv);
void setIgcDebugVars(MockCompilerDebugVars &dbgv);
void clearFclDebugVars();
void clearIgcDebugVars();

MockCompilerDebugVars getFclDebugVars();
MockCompilerDebugVars getIgcDebugVars();

struct MockPlatform : MockCIF<IGC::PlatformTagOCL> {
};

struct MockGTSystemInfo : MockCIF<IGC::GTSystemInfoTagOCL> {
};

struct MockIgcFeaturesAndWorkarounds : MockCIF<IGC::IgcFeaturesAndWorkaroundsTagOCL> {
};

struct MockIgcOclTranslationCtx : MockCIF<IGC::IgcOclTranslationCtxTagOCL> {
    MockIgcOclTranslationCtx();
    ~MockIgcOclTranslationCtx();

    IGC::OclTranslationOutputBase *TranslateImpl(
        CIF::Version_t outVersion,
        CIF::Builtins::BufferSimple *src,
        CIF::Builtins::BufferSimple *options,
        CIF::Builtins::BufferSimple *internalOptions,
        CIF::Builtins::BufferSimple *tracingOptions,
        uint32_t tracingOptionsCount) override;
};

struct MockOclTranslationOutput : MockCIF<IGC::OclTranslationOutputTagOCL> {
    MockOclTranslationOutput();
    ~MockOclTranslationOutput() override;
    CIF::Builtins::BufferBase *GetBuildLogImpl(CIF::Version_t bufferVersion) override;
    CIF::Builtins::BufferBase *GetOutputImpl(CIF::Version_t bufferVersion) override;
    CIF::Builtins::BufferBase *GetDebugDataImpl(CIF::Version_t bufferVersion) override;

    bool Successful() const override {
        return failed == false;
    }

    void setError() {
        setError("");
    }
    void setError(const std::string &message);
    void setOutput(const void *data, size_t dataLen);
    void setDebugData(const void *data, size_t dataLen);

    bool failed = false;
    MockCIFBuffer *output = nullptr;
    MockCIFBuffer *log = nullptr;
    MockCIFBuffer *debugData = nullptr;
};

struct MockIgcOclDeviceCtx : MockCIF<IGC::IgcOclDeviceCtxTagOCL> {
    static CIF::ICIF *Create(CIF::InterfaceId_t intId, CIF::Version_t version);

    MockIgcOclDeviceCtx();
    ~MockIgcOclDeviceCtx() override;

    IGC::PlatformBase *GetPlatformHandleImpl(CIF::Version_t ver) override {
        if (getIgcDebugVars().failCreatePlatformInterface) {
            return nullptr;
        }
        return platform;
    }

    IGC::GTSystemInfoBase *GetGTSystemInfoHandleImpl(CIF::Version_t ver) override {
        if (getIgcDebugVars().failCreateGtSystemInfoInterface) {
            return nullptr;
        }
        return gtSystemInfo;
    }

    IGC::IgcFeaturesAndWorkaroundsBase *GetIgcFeaturesAndWorkaroundsHandleImpl(CIF::Version_t ver) override {
        if (getIgcDebugVars().failCreateIgcFeWaInterface) {
            return nullptr;
        }
        return igcFeWa;
    }

    IGC::IgcOclTranslationCtxBase *CreateTranslationCtxImpl(CIF::Version_t ver,
                                                            IGC::CodeType::CodeType_t inType,
                                                            IGC::CodeType::CodeType_t outType) override;

    void SetDebugVars(MockCompilerDebugVars &debugVars) {
        this->debugVars = debugVars;
    }

    MockPlatform *platform = nullptr;
    MockGTSystemInfo *gtSystemInfo = nullptr;
    MockIgcFeaturesAndWorkarounds *igcFeWa = nullptr;
    MockCompilerDebugVars debugVars;
};

struct MockFclOclTranslationCtx : MockCIF<IGC::FclOclTranslationCtxTagOCL> {
    MockFclOclTranslationCtx();
    ~MockFclOclTranslationCtx() override;

    IGC::OclTranslationOutputBase *TranslateImpl(
        CIF::Version_t outVersion,
        CIF::Builtins::BufferSimple *src,
        CIF::Builtins::BufferSimple *options,
        CIF::Builtins::BufferSimple *internalOptions,
        CIF::Builtins::BufferSimple *tracingOptions,
        uint32_t tracingOptionsCount) override;
};

struct MockFclOclDeviceCtx : MockCIF<IGC::FclOclDeviceCtxTagOCL> {
    MockFclOclDeviceCtx();
    ~MockFclOclDeviceCtx();

    static CIF::ICIF *Create(CIF::InterfaceId_t intId, CIF::Version_t version);
    void SetOclApiVersion(uint32_t version) override {
        oclApiVersion = version;
    }

    IGC::FclOclTranslationCtxBase *CreateTranslationCtxImpl(CIF::Version_t ver,
                                                            IGC::CodeType::CodeType_t inType,
                                                            IGC::CodeType::CodeType_t outType) override;

    uint32_t oclApiVersion = 120;
};

class MockCompilerInterface : public CompilerInterface {
  public:
    ~MockCompilerInterface() {
        CompilerInterface::pInstance = originalGlobalCompilerInterface;
    }

    bool isCompilerAvailable() const {
        return CompilerInterface::isCompilerAvailable();
    }

    void setFclDeviceCtx(const Device &d, IGC::FclOclDeviceCtxTagOCL *ctx) {
        this->fclDeviceContexts[&d] = CIF::RAII::RetainAndPack<IGC::FclOclDeviceCtxTagOCL>(ctx);
    }

    std::map<const Device *, fclDevCtxUptr> &getFclDeviceContexts() {
        return this->fclDeviceContexts;
    }

    void setIgcDeviceCtx(const Device &d, IGC::IgcOclDeviceCtxTagOCL *ctx) {
        this->igcDeviceContexts[&d] = CIF::RAII::RetainAndPack<IGC::IgcOclDeviceCtxTagOCL>(ctx);
    }

    std::map<const Device *, igcDevCtxUptr> &getIgcDeviceContexts() {
        return this->igcDeviceContexts;
    }

    void setDeviceCtx(const Device &d, IGC::IgcOclDeviceCtxTagOCL *ctx) {
        setIgcDeviceCtx(d, ctx);
    }

    void setDeviceCtx(const Device &d, IGC::FclOclDeviceCtxTagOCL *ctx) {
        setFclDeviceCtx(d, ctx);
    }

    template <typename DeviceCtx>
    std::map<const Device *, CIF::RAII::UPtr_t<DeviceCtx>> &getDeviceContexts();

    std::unique_lock<std::mutex> lock() override {
        if (lockListener != nullptr) {
            lockListener(*this);
        }

        return std::unique_lock<std::mutex>(mtx);
    }

    bool initialize() {
        return CompilerInterface::initialize();
    }

    CIF::CIFMain *GetIgcMain() {
        return this->igcMain.get();
    }

    CIF::CIFMain *GetFclMain() {
        return this->fclMain.get();
    }

    void SetIgcMain(CIF::CIFMain *main) {
        this->igcMain.release();
        this->igcMain.reset(main);
    }

    void SetFclMain(CIF::CIFMain *main) {
        this->fclMain.release();
        this->fclMain.reset(main);
    }

    CIF::RAII::UPtr_t<IGC::FclOclTranslationCtxTagOCL> createFclTranslationCtx(const Device &device,
                                                                               IGC::CodeType::CodeType_t inType,
                                                                               IGC::CodeType::CodeType_t outType) override {
        if (failCreateFclTranslationCtx) {
            return nullptr;
        }

        return CompilerInterface::createFclTranslationCtx(device, inType, outType);
    }

    CIF::RAII::UPtr_t<IGC::IgcOclTranslationCtxTagOCL> createIgcTranslationCtx(const Device &device,
                                                                               IGC::CodeType::CodeType_t inType,
                                                                               IGC::CodeType::CodeType_t outType) override {
        if (failCreateIgcTranslationCtx) {
            return nullptr;
        }

        return CompilerInterface::createIgcTranslationCtx(device, inType, outType);
    }

    IGC::FclOclTranslationCtxTagOCL *getFclBaseTranslationCtx() {
        return this->fclBaseTranslationCtx.get();
    }

    void overrideGlobalCompilerInterface() {
        originalGlobalCompilerInterface = CompilerInterface::pInstance;
        CompilerInterface::pInstance = this;
    }

    cl_int getSipKernelBinary(SipKernelType type, const Device &device, std::vector<char> &retBinary) override {
        if (this->sipKernelBinaryOverride.size() > 0) {
            retBinary = this->sipKernelBinaryOverride;
            this->requestedSipKernel = type;
            return 0;
        } else {
            return CompilerInterface::getSipKernelBinary(type, device, retBinary);
        }
    }

    static std::vector<char> getDummyGenBinary();

    CompilerInterface *originalGlobalCompilerInterface = nullptr;

    void (*lockListener)(MockCompilerInterface &compInt) = nullptr;
    void *lockListenerData = nullptr;
    bool failCreateFclTranslationCtx = false;
    bool failCreateIgcTranslationCtx = false;

    std::vector<char> sipKernelBinaryOverride;
    SipKernelType requestedSipKernel = SipKernelType::COUNT;
};

template <>
inline std::map<const Device *, MockCompilerInterface::igcDevCtxUptr> &MockCompilerInterface::getDeviceContexts<IGC::IgcOclDeviceCtxTagOCL>() {
    return getIgcDeviceContexts();
}

template <>
inline std::map<const Device *, MockCompilerInterface::fclDevCtxUptr> &MockCompilerInterface::getDeviceContexts<IGC::FclOclDeviceCtxTagOCL>() {
    return getFclDeviceContexts();
}
} // namespace OCLRT
