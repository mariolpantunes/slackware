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

#include "reader.h"
#include <string.h>

namespace CLElfLib {

/******************************************************************************\
 Constructor: CElfReader::CElfReader
\******************************************************************************/
CElfReader::CElfReader(
    const char *pElfBinary,
    const size_t elfBinarySize) {
    m_pNameTable = NULL;
    m_nameTableSize = 0;
    m_pElfHeader = (SElf64Header *)pElfBinary;
    m_pBinary = pElfBinary;

    // get a pointer to the string table
    if (m_pElfHeader) {
        getSectionData(
            m_pElfHeader->SectionNameTableIndex,
            m_pNameTable, m_nameTableSize);
    }
}

/******************************************************************************\
 Destructor: CElfReader::~CElfReader
\******************************************************************************/
CElfReader::~CElfReader() {
}

/******************************************************************************\
 Member Function: CElfReader::Create
\******************************************************************************/
CElfReader *CElfReader::create(
    const char *pElfBinary,
    const size_t elfBinarySize) {
    CElfReader *pNewReader = NULL;

    if (isValidElf64(pElfBinary, elfBinarySize)) {
        pNewReader = new CElfReader(pElfBinary, elfBinarySize);
    }

    return pNewReader;
}

/******************************************************************************\
 Member Function: CElfReader::Delete
\******************************************************************************/
void CElfReader::destroy(
    CElfReader *&pElfReader) {
    if (pElfReader) {
        delete pElfReader;
        pElfReader = NULL;
    }
}

/******************************************************************************\
 Member Function: IsValidElf64
 Description:     Determines if a binary is in the ELF64 format checks for
                  invalid offsets.
\******************************************************************************/
bool CElfReader::isValidElf64(
    const void *pBinary,
    const size_t binarySize) {
    bool retVal = false;
    SElf64Header *pElf64Header = NULL;
    SElf64SectionHeader *pSectionHeader = NULL;
    char *pNameTable = NULL;
    char *pEnd = NULL;
    size_t ourSize = 0;
    size_t entrySize = 0;
    size_t indexedSectionHeaderOffset = 0;

    // validate header
    if (pBinary && (binarySize >= sizeof(SElf64Header))) {
        // calculate a pointer to the end
        pEnd = (char *)pBinary + binarySize;
        pElf64Header = (SElf64Header *)pBinary;

        if ((pElf64Header->Identity[ID_IDX_MAGIC0] == ELF_MAG0) &&
            (pElf64Header->Identity[ID_IDX_MAGIC1] == ELF_MAG1) &&
            (pElf64Header->Identity[ID_IDX_MAGIC2] == ELF_MAG2) &&
            (pElf64Header->Identity[ID_IDX_MAGIC3] == ELF_MAG3) &&
            (pElf64Header->Identity[ID_IDX_CLASS] == EH_CLASS_64)) {
            ourSize += pElf64Header->ElfHeaderSize;
            retVal = true;
        }
    }

    // validate sections
    if (retVal == true) {
        // get the section entry size
        entrySize = pElf64Header->SectionHeaderEntrySize;

        // get an offset to the name table
        if (pElf64Header->SectionNameTableIndex <
            pElf64Header->NumSectionHeaderEntries) {
            indexedSectionHeaderOffset =
                (size_t)pElf64Header->SectionHeadersOffset +
                (pElf64Header->SectionNameTableIndex * entrySize);

            if (((char *)pBinary + indexedSectionHeaderOffset) <= pEnd) {
                pNameTable = (char *)pBinary + indexedSectionHeaderOffset;
            }
        }

        for (unsigned int i = 0; i < pElf64Header->NumSectionHeaderEntries; i++) {
            indexedSectionHeaderOffset = (size_t)pElf64Header->SectionHeadersOffset +
                                         (i * entrySize);

            // check section header offset
            if (((char *)pBinary + indexedSectionHeaderOffset) > pEnd) {
                retVal = false;
                break;
            }

            pSectionHeader = (SElf64SectionHeader *)((char *)pBinary + indexedSectionHeaderOffset);

            // check section data
            if (((char *)pBinary + pSectionHeader->DataOffset + pSectionHeader->DataSize) > pEnd) {
                retVal = false;
                break;
            }

            // check section name index
            if ((pNameTable + pSectionHeader->Name) > pEnd) {
                retVal = false;
                break;
            }

            // tally up the sizes
            ourSize += (size_t)pSectionHeader->DataSize;
            ourSize += (size_t)entrySize;
        }

        if (ourSize != binarySize) {
            retVal = false;
        }
    }

    return retVal;
}

/******************************************************************************\
 Member Function: GetElfHeader
 Description:     Returns a pointer to the requested section header
\******************************************************************************/
const SElf64Header *CElfReader::getElfHeader() {
    return m_pElfHeader;
}

/******************************************************************************\
 Member Function: GetSectionHeader
 Description:     Returns a pointer to the requested section header
\******************************************************************************/
const SElf64SectionHeader *CElfReader::getSectionHeader(
    unsigned int sectionIndex) {
    SElf64SectionHeader *pSectionHeader = NULL;
    size_t indexedSectionHeaderOffset = 0;
    size_t entrySize = m_pElfHeader->SectionHeaderEntrySize;

    if (sectionIndex < m_pElfHeader->NumSectionHeaderEntries) {
        indexedSectionHeaderOffset = (size_t)m_pElfHeader->SectionHeadersOffset +
                                     (sectionIndex * entrySize);

        pSectionHeader = (SElf64SectionHeader *)((char *)m_pElfHeader + indexedSectionHeaderOffset);
    }

    return pSectionHeader;
}

/******************************************************************************\
 Member Function: GetSectionData
 Description:     Returns a pointer to and size of the requested section's 
                  data
\******************************************************************************/
bool CElfReader::getSectionData(
    const unsigned int sectionIndex,
    char *&pData,
    size_t &dataSize) {
    const SElf64SectionHeader *pSectionHeader = getSectionHeader(sectionIndex);

    if (pSectionHeader) {
        pData = (char *)m_pBinary + pSectionHeader->DataOffset;
        dataSize = (size_t)pSectionHeader->DataSize;
        return true;
    }

    return false;
}

/******************************************************************************\
 Member Function: GetSectionData
 Description:     Returns a pointer to and size of the requested section's 
                  data
\******************************************************************************/
bool CElfReader::getSectionData(
    const char *pName,
    char *&pData,
    size_t &dataSize) {
    const char *pSectionName = NULL;

    for (unsigned int i = 1; i < m_pElfHeader->NumSectionHeaderEntries; i++) {
        pSectionName = getSectionName(i);

        if (pSectionName && (strcmp(pName, pSectionName) == 0)) {
            getSectionData(i, pData, dataSize);
            return true;
            ;
        }
    }

    return false;
}

/******************************************************************************\
 Member Function: GetSectionName
 Description:     Returns a pointer to a NULL terminated string
\******************************************************************************/
const char *CElfReader::getSectionName(
    unsigned int sectionIndex) {
    char *pName = NULL;
    const SElf64SectionHeader *pSectionHeader = getSectionHeader(sectionIndex);

    if (pSectionHeader) {
        pName = m_pNameTable + pSectionHeader->Name;
    }

    return pName;
}

} // namespace OclElfLib
