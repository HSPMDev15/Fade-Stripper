#pragma once

#include "7zTypes.h"
#include "LzmaDec.h"
#include "LzmaEnc.h"
#include <vector>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// LZMA_ID = (('A'<<24)|('M'<<16)|('Z'<<8)|'L')
inline constexpr uint32_t LZMA_ID    = 0x414D5A4Cu;
#define LZMA_PROPS_SIZE 5

#pragma pack(push, 1)
struct lzma_header_t {
    uint32_t id;
    uint32_t actualSize; // always little-endian
    uint32_t lzmaSize;   // compressed payload size
    unsigned char properties[LZMA_PROPS_SIZE];
};
static_assert (sizeof (lzma_header_t) == 17);
#pragma pack(pop)

// Compress raw data into a Valve format LZMA blob
unsigned char* LZMA_Compress (const unsigned char* pInput, unsigned int inputSize,unsigned int* pOutputSize);

// Decompress a Valve format LZMA blob
bool LZMA_Uncompress (const unsigned char* pInBuffer,unsigned char** ppOutBuffer,unsigned int* pOutSize);

// Returns true if the buffer starts with a valid Valve LZMA header.
bool LZMA_IsCompressed (const unsigned char* pInput, size_t inputSize);

// Returns the uncompressed size from the Valve LZMA header (0 if not LZMA).
unsigned int LZMA_GetActualSize (const unsigned char* pInput, size_t inputSize);

// ZIP entry LZMA (aka method 14) format from https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT
// this is different from Valve lzma_header_t (used for the game lump)
// i still use same lzma_sdk but with a different header layout
bool LZMA_CompressZipEntry(const unsigned char* src, size_t srcLen, std::vector<unsigned char>& out);

bool LZMA_DecompressZipEntry(const unsigned char* src, size_t srcLen, size_t uncompressedSize, std::vector<unsigned char>& out);