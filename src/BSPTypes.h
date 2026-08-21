#pragma once
#include <cstddef>
#include <cstdint>

// #define IDBSPHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'V') litte endian
inline constexpr uint32_t IDBSPHEADER = 0x50534256u; 
inline constexpr int HEADER_LUMPS     = 64;
inline constexpr int MINBSPVERSION    = 19;
inline constexpr int BSPVERSION       = 21;

enum BSPLumpIndex : int {
    LUMP_ENTITIES             = 0,
    LUMP_GAME_LUMP            = 35,
    LUMP_PAKFILE              = 40,
    LUMP_TEXDATA_STRING_DATA  = 43,
    LUMP_TEXDATA_STRING_TABLE = 44,
    LUMP_OVERLAYS             = 45,
    LUMP_MAP_FLAGS            = 59,
    LUMP_OVERLAY_FADES        = 60,
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

inline constexpr int GAMELUMP_STATIC_PROPS = (0x73 << 24) | (0x70 << 16) | (0x72 << 8) | 0x70; // 'sprp' = 0x73707270
