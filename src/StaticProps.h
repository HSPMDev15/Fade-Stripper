#pragma once

#include "BSP.h"

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

struct PatchResult {
    int total     = 0; // props found in the lump
    int hasFade   = 0;
    int patched   = 0;
    int alreadyOk = 0;
    int errors    = 0;
    bool ok       = true;
    std::string error;
};

// Locate the game lump 'sprp' and patch m_FadeMinDist=-1 and m_FadeMaxDist=0
PatchResult patchStaticPropFades (BSP& bsp);
