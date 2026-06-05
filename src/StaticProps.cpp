#include "StaticProps.h"
#include "BSPTypes.h"
#include "Formatting.h"
#include <algorithm>
#include <cstring>

namespace {

template<typename T>
T    readLE(const uint8_t* p)         { T v; memcpy(&v, p, sizeof(v)); return v; }
template<typename T>
void writeLE(uint8_t* p, const T& v) { memcpy(p, &v, sizeof(v)); }

bool resolveLayout(uint16_t version, uint32_t propCount, size_t blobRemaining,
                   size_t& stride, bool& isV7Star) {
    isV7Star = false;
    stride   = sprpStride(version);

    if (stride > 0 && static_cast<size_t>(propCount) * stride <= blobRemaining)
        return true;

    if (static_cast<size_t>(propCount) * SPROP_STRIDE_V7STAR <= blobRemaining) {
        stride   = SPROP_STRIDE_V7STAR;
        isV7Star = true;
        return true;
    }

    return false;
}

PatchResult patchBlob(std::vector<uint8_t>& blob, uint16_t version, const LogFn& log) {
    PatchResult res;
    size_t pos = 0;

    auto need = [&](size_t n) { return pos + n <= blob.size(); };

    if (!need(4)) { res.ok = false; res.error = "truncated in dictCount"; return res; }
    const uint32_t dictCount = readLE<uint32_t>(blob.data() + pos);
    pos += 4 + static_cast<size_t>(dictCount) * 128;

    if (!need(4)) { res.ok = false; res.error = "truncated in leafCount"; return res; }
    const uint32_t leafCount = readLE<uint32_t>(blob.data() + pos);
    pos += 4 + static_cast<size_t>(leafCount) * sizeof(uint16_t);

    if (!need(4)) { res.ok = false; res.error = "truncated in propCount"; return res; }
    const uint32_t propCount = readLE<uint32_t>(blob.data() + pos);
    pos += 4;

    res.total = static_cast<int>(propCount);

    if (propCount == 0) {
        log(" Son this map hasn`t any prop ");
        return res;
    }

    size_t stride;
    bool   isV7Star;
    if (!resolveLayout(version, propCount, blob.size() - pos, stride, isV7Star)) {
        res.ok    = false;
        res.error = formatting::format(
            " sprp version {} unknown and no layout fits in the blob", version);
        return res;
    }

    log(formatting::format("  sprp v{}{} | stride={} bytes | {} props",
                           version,
                           isV7Star ? " (v7* layout TF2SDK branch)" : "",
                           stride, propCount).c_str());

    const bool   flagIsU32 = isV7Star;
    const size_t flagOff   = isV7Star ? SPROP_OFF_FLAGS_V7STAR : SPROP_OFF_FLAGS;

    for (uint32_t i = 0; i < propCount; ++i) {
        uint8_t* prop = blob.data() + pos + i * stride;

        const float fadeMin    = readLE<float>(prop + SPROP_FADE_MIN_OFF);
        const float fadeMax    = readLE<float>(prop + SPROP_FADE_MAX_OFF);
        const bool hasFadeFlag = flagIsU32
            ? (readLE<uint32_t>(prop + flagOff) & STATIC_PROP_FLAG_FADES) != 0
            : (readLE<uint8_t> (prop + flagOff) & STATIC_PROP_FLAG_FADES) != 0;

        if (hasFadeFlag) ++res.hasFade;

        const bool alreadyOk = (fadeMin == FADE_NEVER_MIN) &&
                               (fadeMax == FADE_NEVER_MAX) &&
                               !hasFadeFlag;
        if (alreadyOk) { ++res.alreadyOk; continue; }

        writeLE(prop + SPROP_FADE_MIN_OFF, FADE_NEVER_MIN);
        writeLE(prop + SPROP_FADE_MAX_OFF, FADE_NEVER_MAX);

        if (flagIsU32) {
            uint32_t f = readLE<uint32_t>(prop + flagOff);
            f &= ~static_cast<uint32_t>(STATIC_PROP_FLAG_FADES);
            writeLE(prop + flagOff, f);
        } else {
            uint8_t f = readLE<uint8_t>(prop + flagOff);
            f &= ~STATIC_PROP_FLAG_FADES;
            writeLE(prop + flagOff, f);
        }

        ++res.patched;
    }

    return res;
}

} // namespace

PatchResult patchStaticPropFades(BSP& bsp, const LogFn& log) {
    auto& lumps = bsp.gameLumps();

    const auto it = std::ranges::find_if(lumps, [](const GameLump& gl) {
        return gl.id == GAMELUMP_STATIC_PROPS;
    });

    if (it == lumps.end()) {
        PatchResult res;
        log(" there is no game lump 'sprp' (map without static props)");
        return res;
    }

    log(formatting::format("  'sprp' found | version={} | compressed={}",
                           it->version,
                           (it->flags & GAMELUMPFLAG_COMPRESSED) ? "yes" : "no").c_str());

    return patchBlob(it->data, it->version, log);
}