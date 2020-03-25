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

#include "runtime/command_stream/aub_command_stream_receiver.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/os_interface/os_inc_base.h"
#include <algorithm>
#include <cstring>
#include <sstream>

namespace OCLRT {
AubCommandStreamReceiverCreateFunc aubCommandStreamReceiverFactory[IGFX_MAX_CORE] = {};

CommandStreamReceiver *AUBCommandStreamReceiver::create(const HardwareInfo &hwInfo, const std::string &baseName, bool standalone) {
    std::string hwPrefix = hardwarePrefix[hwInfo.pPlatform->eProductFamily];

    // Generate the full filename
    const auto &gtSystemInfo = *hwInfo.pSysInfo;
    std::stringstream strfilename;
    strfilename << hwPrefix << "_" << gtSystemInfo.SliceCount << "x" << gtSystemInfo.SubSliceCount << "x" << gtSystemInfo.MaxEuPerSubSlice << "_" << baseName << ".aub";

    // clean-up any fileName issues because of the file system incompatibilities
    auto fileName = strfilename.str();
    for (char &i : fileName) {
        i = i == '/' ? '_' : i;
    }

    std::string filePath(folderAUB);
    filePath.append(Os::fileSeparator);
    filePath.append(fileName);

    if (hwInfo.pPlatform->eRenderCoreFamily >= IGFX_MAX_CORE) {
        DEBUG_BREAK_IF(!false);
        return nullptr;
    }

    auto pCreate = aubCommandStreamReceiverFactory[hwInfo.pPlatform->eRenderCoreFamily];
    return pCreate ? pCreate(hwInfo, filePath, standalone) : nullptr;
}
} // namespace OCLRT

namespace AubMemDump {
using CmdServicesMemTraceMemoryCompare = AubMemDump::CmdServicesMemTraceMemoryCompare;
using CmdServicesMemTraceMemoryWrite = AubMemDump::CmdServicesMemTraceMemoryWrite;
using CmdServicesMemTraceRegisterPoll = AubMemDump::CmdServicesMemTraceRegisterPoll;
using CmdServicesMemTraceRegisterWrite = AubMemDump::CmdServicesMemTraceRegisterWrite;
using CmdServicesMemTraceVersion = AubMemDump::CmdServicesMemTraceVersion;

static auto sizeMemoryWriteHeader = sizeof(CmdServicesMemTraceMemoryWrite) - sizeof(CmdServicesMemTraceMemoryWrite::data);

extern const size_t g_dwordCountMax;

void AubFileStream::open(const char *filePath) {
    fileHandle.open(filePath, std::ofstream::binary);
}

void AubFileStream::close() {
    fileHandle.close();
}

void AubFileStream::write(const char *data, size_t size) {
    fileHandle.write(data, size);
}

bool AubFileStream::init(uint32_t stepping, uint32_t device) {
    CmdServicesMemTraceVersion header;
    memset(&header, 0, sizeof(header));

    header.setHeader();
    header.dwordCount = (sizeof(header) / sizeof(uint32_t)) - 1;
    header.stepping = stepping;
    header.metal = 0;
    header.device = device;
    header.csxSwizzling = CmdServicesMemTraceVersion::CsxSwizzlingValues::Disabled;
    //Which recording method used:
    // Phys is required for GGTT memory to be written directly to phys vs through aperture.
    header.recordingMethod = CmdServicesMemTraceVersion::RecordingMethodValues::Phy;
    header.pch = CmdServicesMemTraceVersion::PchValues::Default;
    header.captureTool = CmdServicesMemTraceVersion::CaptureToolValues::GenKmdCapture;
    header.primaryVersion = 0;
    header.secondaryVersion = 0;
    header.commandLine[0] = 'N';
    header.commandLine[1] = 'E';
    header.commandLine[2] = 'O';
    header.commandLine[3] = 0;

    write(reinterpret_cast<char *>(&header), sizeof(header));
    return true;
}

void AubFileStream::writeMemory(uint64_t physAddress, const void *memory, size_t size, uint32_t addressSpace, uint32_t hint) {
    writeMemoryWriteHeader(physAddress, size, addressSpace, hint);

    // Copy the contents from source to destination.
    write(reinterpret_cast<const char *>(memory), size);

    auto sizeRemainder = size % sizeof(uint32_t);
    if (sizeRemainder) {
        //if input size is not 4 byte aligned, write extra zeros to AUB
        uint32_t zero = 0;
        write(reinterpret_cast<char *>(&zero), sizeof(uint32_t) - sizeRemainder);
    }
}

void AubFileStream::writeMemoryWriteHeader(uint64_t physAddress, size_t size, uint32_t addressSpace, uint32_t hint) {
    CmdServicesMemTraceMemoryWrite header;
    auto alignedBlockSize = (size + sizeof(uint32_t) - 1) & ~(sizeof(uint32_t) - 1);
    auto dwordCount = (sizeMemoryWriteHeader + alignedBlockSize) / sizeof(uint32_t);
    DEBUG_BREAK_IF(dwordCount > AubMemDump::g_dwordCountMax);

    memset(&header, 0, sizeof(header));
    header.setHeader();
    header.dwordCount = static_cast<uint32_t>(dwordCount - 1);
    header.address = physAddress;
    header.repeatMemory = CmdServicesMemTraceMemoryWrite::RepeatMemoryValues::NoRepeat;
    header.tiling = CmdServicesMemTraceMemoryWrite::TilingValues::NoTiling;
    header.dataTypeHint = hint;
    header.addressSpace = addressSpace;
    header.dataSizeInBytes = static_cast<uint32_t>(size);

    write(reinterpret_cast<const char *>(&header), sizeMemoryWriteHeader);
}

void AubFileStream::writeGTT(uint32_t gttOffset, uint64_t entry) {
    fileHandle.write(reinterpret_cast<char *>(&entry), sizeof(entry));
}

void AubFileStream::writePTE(uint64_t physAddress, uint64_t entry) {
    write(reinterpret_cast<char *>(&entry), sizeof(entry));
}

void AubFileStream::writeMMIO(uint32_t offset, uint32_t value) {
    CmdServicesMemTraceRegisterWrite header;
    memset(&header, 0, sizeof(header));
    header.setHeader();
    header.dwordCount = (sizeof(header) / sizeof(uint32_t)) - 1;
    header.registerOffset = offset;
    header.messageSourceId = MessageSourceIdValues::Ia;
    header.registerSize = RegisterSizeValues::Dword;
    header.registerSpace = RegisterSpaceValues::Mmio;
    header.writeMaskLow = 0xffffffff;
    header.writeMaskHigh = 0x00000000;
    header.data[0] = value;

    write(reinterpret_cast<char *>(&header), sizeof(header));
}

void AubFileStream::registerPoll(uint32_t registerOffset, uint32_t mask, uint32_t value, bool pollNotEqual, uint32_t timeoutAction) {
    CmdServicesMemTraceRegisterPoll header;
    memset(&header, 0, sizeof(header));
    header.setHeader();
    header.registerOffset = registerOffset;
    header.timeoutAction = timeoutAction;
    header.pollNotEqual = pollNotEqual;
    header.operationType = CmdServicesMemTraceRegisterPoll::OperationTypeValues::Normal;
    header.registerSize = CmdServicesMemTraceRegisterPoll::RegisterSizeValues::Dword;
    header.registerSpace = CmdServicesMemTraceRegisterPoll::RegisterSpaceValues::Mmio;
    header.pollMaskLow = mask;
    header.data[0] = value;
    header.dwordCount = (sizeof(header) / sizeof(uint32_t)) - 1;

    write(reinterpret_cast<char *>(&header), sizeof(header));
}

void AubFileStream::expectMemory(uint64_t physAddress, const void *memory, size_t sizeRemaining) {
    using CmdServicesMemTraceMemoryCompare = AubMemDump::CmdServicesMemTraceMemoryCompare;
    CmdServicesMemTraceMemoryCompare header;
    memset(&header, 0, sizeof(header));
    header.setHeader();

    header.noReadExpect = CmdServicesMemTraceMemoryCompare::NoReadExpectValues::ReadExpect;
    header.repeatMemory = CmdServicesMemTraceMemoryCompare::RepeatMemoryValues::NoRepeat;
    header.tiling = CmdServicesMemTraceMemoryCompare::TilingValues::NoTiling;
    header.crcCompare = CmdServicesMemTraceMemoryCompare::CrcCompareValues::NoCrc;
    header.dataTypeHint = CmdServicesMemTraceMemoryCompare::DataTypeHintValues::TraceNotype;
    header.addressSpace = CmdServicesMemTraceMemoryCompare::AddressSpaceValues::TraceNonlocal;

    auto headerSize = sizeof(CmdServicesMemTraceMemoryCompare) - sizeof(CmdServicesMemTraceMemoryCompare::data);
    auto blockSizeMax = g_dwordCountMax * sizeof(uint32_t) - headerSize;

    // We have to decompose memory into chunks that can be streamed per iteration
    while (sizeRemaining > 0) {
        AubMemDump::setAddress(header, physAddress);

        auto sizeThisIteration = std::min(sizeRemaining, blockSizeMax);

        // Round up to the number of dwords
        auto dwordCount = (headerSize + sizeThisIteration + sizeof(uint32_t) - 1) / sizeof(uint32_t);

        header.dwordCount = static_cast<uint32_t>(dwordCount - 1);
        header.dataSizeInBytes = static_cast<uint32_t>(sizeThisIteration);

        // Write the header
        write(reinterpret_cast<char *>(&header), headerSize);

        // Copy the contents from source to destination.
        write(reinterpret_cast<const char *>(memory), sizeThisIteration);

        sizeRemaining -= sizeThisIteration;
        memory = (uint8_t *)memory + sizeThisIteration;
        physAddress += sizeThisIteration;

        auto remainder = sizeThisIteration & (sizeof(uint32_t) - 1);
        if (remainder) {
            //if size is not 4 byte aligned, write extra zeros to AUB
            uint32_t zero = 0;
            write(reinterpret_cast<char *>(&zero), sizeof(uint32_t) - remainder);
        }
    }
}

void AubFileStream::createContext(const AubPpgttContextCreate &cmd) {
    write(reinterpret_cast<const char *>(&cmd), sizeof(cmd));
}

bool AubFileStream::addComment(const char *message) {
    using CmdServicesMemTraceComment = AubMemDump::CmdServicesMemTraceComment;
    CmdServicesMemTraceComment cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.setHeader();
    cmd.syncOnComment = false;
    cmd.syncOnSimulatorDisplay = false;

    auto messageLen = strlen(message) + 1;
    auto dwordLen = ((messageLen + sizeof(uint32_t) - 1) & ~(sizeof(uint32_t) - 1)) / sizeof(uint32_t);
    cmd.dwordCount = static_cast<uint32_t>(dwordLen + 1);

    write(reinterpret_cast<char *>(&cmd), sizeof(cmd) - sizeof(cmd.comment));
    write(message, messageLen);
    auto remainder = messageLen & (sizeof(uint32_t) - 1);
    if (remainder) {
        //if size is not 4 byte aligned, write extra zeros to AUB
        uint32_t zero = 0;
        write(reinterpret_cast<char *>(&zero), sizeof(uint32_t) - remainder);
    }
    return true;
}

} // namespace AubMemDump
