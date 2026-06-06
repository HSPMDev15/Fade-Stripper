#pragma once

#include "7zTypes.h"
#include "BSPTypes.h"
#include "LzmaDec.h"
#include "LzmaEnc.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// Compress raw data into a Valve format LZMA blob
unsigned char* LZMA_Compress (const unsigned char* pInput,
unsigned int inputSize,
unsigned int* pOutputSize);

// Decompress a Valve format LZMA blob
bool LZMA_Uncompress (const unsigned char* pInBuffer,
unsigned char** ppOutBuffer,
unsigned int* pOutSize);

// Returns true if the buffer starts with a valid Valve LZMA header.
bool LZMA_IsCompressed (const unsigned char* pInput, size_t inputSize);

// Returns the uncompressed size from the Valve LZMA header (0 if not LZMA).
unsigned int LZMA_GetActualSize (const unsigned char* pInput, size_t inputSize);
