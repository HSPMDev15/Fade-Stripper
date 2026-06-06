
#include "LZMA.h" //shared

static void* SzAlloc (void*, size_t size) {
    return malloc (size);
}
static void SzFree (void*, void* addr) {
    free (addr);
}
static ISzAlloc g_Alloc = { SzAlloc, SzFree };

struct CInStreamRam {
    ISeqInStream vt;
    const Byte* data;
    size_t size;
    size_t pos;
};

static SRes InStreamRead (void* p, void* buf, size_t* sz) {
    auto* s      = static_cast<CInStreamRam*> (p);
    size_t avail = s->size - s->pos;
    if (*sz > avail)
        *sz = avail;
    memcpy (buf, s->data + s->pos, *sz);
    s->pos += *sz;
    return SZ_OK;
}

static void InStreamInit (CInStreamRam* s, const Byte* data, size_t size) {
    s->vt.Read = InStreamRead;
    s->data    = data;
    s->size    = size;
    s->pos     = 0;
}

struct COutStreamRam {
    ISeqOutStream vt;
    Byte* data;
    size_t capacity;
    size_t pos;
    bool overflow;
};

static size_t OutStreamWrite (void* p, const void* buf, size_t sz) {
    auto* s        = static_cast<COutStreamRam*> (p);
    size_t written = 0;
    while (written < sz && s->pos < s->capacity)
        s->data[s->pos++] = static_cast<const Byte*> (buf)[written++];
    if (written != sz)
        s->overflow = true;
    return written;
}

static void OutStreamInit (COutStreamRam* s, Byte* data, size_t capacity) {
    s->vt.Write = OutStreamWrite;
    s->data     = data;
    s->capacity = capacity;
    s->pos      = 0;
    s->overflow = false;
}


unsigned char* LZMA_Compress (const unsigned char* pInput,
unsigned int inputSize,
unsigned int* pOutputSize) {
    *pOutputSize = 0;

    // work buffer 105% + 64KB margin for worst case scenarios (incompressible data)
    const unsigned int outSize = inputSize / 20 * 21 + (1 << 16);
    auto* pOut                 = static_cast<unsigned char*> (malloc (outSize));
    if (!pOut)
        return nullptr;

    CLzmaEncHandle enc = LzmaEnc_Create (&g_Alloc);
    if (!enc) {
        free (pOut);
        return nullptr;
    }

    CLzmaEncProps props;
    LzmaEncProps_Init (&props);
    if (LzmaEnc_SetProps (enc, &props) != SZ_OK) {
        LzmaEnc_Destroy (enc, &g_Alloc, &g_Alloc);
        free (pOut);
        return nullptr;
    }

    // The encoder writes after the slot reserved for the Valve header
    COutStreamRam outStream;
    OutStreamInit (
    &outStream, pOut + sizeof (lzma_header_t), outSize - sizeof (lzma_header_t));

    // standard 13 bytes header (5 bytes props + 8 bytes uncompressed size (little-endian))
    Byte stdHdr[LZMA_PROPS_SIZE + 8];
    size_t propsSize = LZMA_PROPS_SIZE;
    if (LzmaEnc_WriteProperties (enc, stdHdr, &propsSize) != SZ_OK) {
        LzmaEnc_Destroy (enc, &g_Alloc, &g_Alloc);
        free (pOut);
        return nullptr;
    }
    for (int i = 0; i < 8; ++i)
        stdHdr[LZMA_PROPS_SIZE + i] = static_cast<Byte> (inputSize >> (8 * i));
    outStream.vt.Write (&outStream, stdHdr, LZMA_PROPS_SIZE + 8);

    CInStreamRam inStream;
    InStreamInit (&inStream, reinterpret_cast<const Byte*> (pInput), inputSize);

    const SRes res =
    LzmaEnc_Encode (enc, &outStream.vt, &inStream.vt, nullptr, &g_Alloc, &g_Alloc);
    LzmaEnc_Destroy (enc, &g_Alloc, &g_Alloc);

    if (res != SZ_OK || outStream.overflow) {
        free (pOut);
        return nullptr;
    }

    const size_t compSz = outStream.pos; // includes the 13 bytes of std header

    // Build Valve header over the reserved slot
    auto* hdr       = reinterpret_cast<lzma_header_t*> (pOut);
    hdr->id         = LZMA_ID;
    hdr->actualSize = inputSize;
    hdr->lzmaSize   = static_cast<uint32_t> (compSz - 13);
    memcpy (hdr->properties, pOut + sizeof (lzma_header_t), LZMA_PROPS_SIZE);

    // Displace payload
    memmove (pOut + sizeof (lzma_header_t), pOut + sizeof (lzma_header_t) + 13, compSz - 13);

    *pOutputSize = static_cast<unsigned int> (sizeof (lzma_header_t) + compSz - 13);
    return pOut;
}


bool LZMA_Uncompress (const unsigned char* pInBuffer,
unsigned char** ppOutBuffer,
unsigned int* pOutSize) {
    *ppOutBuffer = nullptr;
    *pOutSize    = 0;

    if (!LZMA_IsCompressed (pInBuffer, sizeof (lzma_header_t) + 1))
        return false;

    const auto* hdr = reinterpret_cast<const lzma_header_t*> (pInBuffer);

    CLzmaDec state;
    LzmaDec_Construct (&state);
    if (LzmaDec_Allocate (&state, hdr->properties, LZMA_PROPS_SIZE, &g_Alloc) != SZ_OK)
        return false;

    auto* pOut = static_cast<unsigned char*> (malloc (hdr->actualSize));
    if (!pOut) {
        LzmaDec_Free (&state, &g_Alloc);
        return false;
    }

    SizeT outProcessed = hdr->actualSize;
    SizeT inProcessed  = hdr->lzmaSize;
    ELzmaStatus status;

    const SRes result = LzmaDecode (reinterpret_cast<Byte*> (pOut), &outProcessed,
    reinterpret_cast<const Byte*> (pInBuffer + sizeof (lzma_header_t)), &inProcessed,
    hdr->properties, LZMA_PROPS_SIZE, LZMA_FINISH_END, &status, &g_Alloc);

    LzmaDec_Free (&state, &g_Alloc);

    if (result != SZ_OK || hdr->actualSize != outProcessed) {
        free (pOut);
        return false;
    }

    *ppOutBuffer = pOut;
    *pOutSize    = hdr->actualSize;
    return true;
}

bool LZMA_IsCompressed (const unsigned char* pInput, size_t inputSize) {
    if (!pInput || inputSize < sizeof (lzma_header_t))
        return false;
    uint32_t id;
    memcpy (&id, pInput, sizeof (id));
    return id == LZMA_ID;
}

unsigned int LZMA_GetActualSize (const unsigned char* pInput, size_t inputSize) {
    if (!LZMA_IsCompressed (pInput, inputSize))
        return 0;
    return reinterpret_cast<const lzma_header_t*> (pInput)->actualSize;
}