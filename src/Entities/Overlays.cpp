#include "../BSP.h"
#include "../BSPTypes.h"
#include "../log.h"

namespace {

// doverlay_t stride confirmed to be exactly 352 bytes per entry (134816 bytes / 383 overlays)
// tested with cp_dustbowl.bsp
inline constexpr size_t OVERLAY_STRIDE = 352;

inline constexpr float OVERLAY_FADE_NEVER_MIN = -1.0f;
inline constexpr float OVERLAY_FADE_NEVER_MAX = 0.0f;

#pragma pack(push, 1)
struct overlayfade_t {
    float fadeDistMinSq;
    float fadeDistMaxSq;
};
static_assert(sizeof(overlayfade_t) == 8);
#pragma pack(pop)

} // namespace

PatchResult patchOverlayFades(BSP& bsp) {
    PatchResult res;

    const std::vector<uint8_t> overlaysBlob = bsp.readLump(LUMP_OVERLAYS);
    const int overlayCount = overlaysBlob.empty() 
                                        ? 0 : static_cast<int>(overlaysBlob.size() / OVERLAY_STRIDE);

    std::vector<uint8_t> fadeBlob = bsp.readLump(LUMP_OVERLAY_FADES);

    if (fadeBlob.empty()) {
        if (overlayCount == 0) {
            Info("No 'info_overlay' entities on this map");
        } else {
            // Overlays exist but the fade lump is missing/empty 
            // VBSP either never wrote this lump for this map, or something else stripped it...
            // Either way there is nothing to patch here so skip it.
            Warning("{} overlay(s) present but LUMP_OVERLAY_FADES is empty, nothing to patch...", overlayCount);
        }
        return res;
    }

    const int fadeCount = static_cast<int>(fadeBlob.size() / sizeof(overlayfade_t));
    res.total = fadeCount;

    if (fadeCount != overlayCount) {
        Warning("Overlay entities count mismatch: {} in LUMP_OVERLAYS vs {} in LUMP_OVERLAY_FADES" 
                "patching only by fade lump count, results may be incomplete!",
                overlayCount, fadeCount);
    }

    auto* fades = reinterpret_cast<overlayfade_t*>(fadeBlob.data());

    for (int i = 0; i < fadeCount; ++i) {
        const bool alreadyOk = (fades[i].fadeDistMinSq == OVERLAY_FADE_NEVER_MIN) &&
                               (fades[i].fadeDistMaxSq == OVERLAY_FADE_NEVER_MAX);
        if (alreadyOk) { ++res.alreadyOk; continue; }

        ++res.hasFade;

        fades[i].fadeDistMinSq = OVERLAY_FADE_NEVER_MIN;
        fades[i].fadeDistMaxSq = OVERLAY_FADE_NEVER_MAX;
        ++res.patched;
    }

    bsp.writeLump(LUMP_OVERLAY_FADES, fadeBlob.data(), fadeBlob.size());
    return res;
}