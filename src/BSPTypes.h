#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

inline constexpr uint32_t IDBSPHEADER = 0x50534256u; // "VBSP" LE
inline constexpr int HEADER_LUMPS     = 64;
inline constexpr int MINBSPVERSION    = 19;
inline constexpr int BSPVERSION       = 21;

// LZMA_ID = (('A'<<24)|('M'<<16)|('Z'<<8)|'L') ts aint x360
inline constexpr uint32_t LZMA_ID    = 0x414D5A4Cu;
inline constexpr int LZMA_PROPS_SIZE = 5;

#pragma pack(push, 1)
struct lzma_header_t {
    uint32_t id;
    uint32_t actualSize; // always little-endian
    uint32_t lzmaSize;   // compressed payload size
    unsigned char properties[LZMA_PROPS_SIZE];
};
static_assert (sizeof (lzma_header_t) == 17);
#pragma pack(pop)

enum BSPLumpIndex : int {
    LUMP_ENTITIES  = 0,
    LUMP_GAME_LUMP = 35,
    LUMP_PAKFILE   = 40,
    LUMP_MAP_FLAGS = 59,
};

#pragma pack(push, 1)
struct lump_t {
    int fileofs;
    int filelen;
    int version;
    char fourCC[4];
};
static_assert (sizeof (lump_t) == 16);

struct BSPHeader_t {
    int ident;
    int m_nVersion;
    lump_t lumps[HEADER_LUMPS];
    int mapRevision;
};
static_assert (sizeof (BSPHeader_t) == 1036);

inline constexpr uint16_t GAMELUMPFLAG_COMPRESSED = 0x0001;

struct dgamelumpheader_t {
    int lumpCount;
};

struct dgamelump_t {
    int id;         // four-CC as int LE
    uint16_t flags; // GAMELUMPFLAG_COMPRESSED
    uint16_t version;
    int fileofs;
    int filelen;
};
static_assert (sizeof (dgamelump_t) == 16);
#pragma pack(pop)

inline constexpr int GAMELUMP_STATIC_PROPS =
(0x73 << 24) | (0x70 << 16) | (0x72 << 8) | 0x70; // 'sprp' = 0x73707270


inline constexpr size_t SPROP_OFF_FLAGS        = 31; // uint8 on layout standard
inline constexpr size_t SPROP_FADE_MIN_OFF     = 36; // float m_FadeMinDist
inline constexpr size_t SPROP_FADE_MAX_OFF     = 40; // float m_FadeMaxDist
inline constexpr size_t SPROP_OFF_FLAGS_V7STAR = 64; // uint32 on v7*

// Flag automatically set when a static prop has fade distances (regardless of their values)
inline constexpr uint8_t STATIC_PROP_FLAG_FADES = 0x01;

// Stride of StaticPropLump_t per version of sizeof of each struct in gamebspfile.h
inline constexpr size_t sprpStride (uint16_t version) noexcept {
    switch (version) {
    case 4: return 56;
    case 5: return 60;
    case 6: return 64;
    case 7: return 68;
    case 8: return 68;
    case 9: return 72;
    case 10: return 76;
    case 11: return 80;
    default: return 0;
    }
}
inline constexpr size_t SPROP_STRIDE_V7STAR = 72;

// never do fade
inline constexpr float FADE_NEVER_MIN = -1.0f;
inline constexpr float FADE_NEVER_MAX = 0.0f;