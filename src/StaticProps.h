#pragma once

#include "BSP.h"
#include <functional>
#include <string>

struct PatchResult {
    int total     = 0; // props found in the lump
    int hasFade   = 0;
    int patched   = 0;
    int alreadyOk = 0;
    int errors    = 0;
    bool ok       = true;
    std::string error;
};

using LogFn = std::function<void (const char*)>;

// Locate the game lump 'sprp' and patch m_FadeMinDist=-1 and m_FadeMaxDist=0
PatchResult patchStaticPropFades (BSP& bsp, const LogFn& log);
