#pragma once

#include "BSPTypes.h"

struct GameLump {
    int id;
    uint16_t flags;
    uint16_t version;
    int fileofs;               // absolute offset in the original BSP
    int filelen;               // uncompressed length
    std::vector<uint8_t> data; // always uncompressed data blob (empty if fileofs/filelen invalid or missing in the BSP)
};

class BSP {
    public:
    explicit BSP (std::string_view path);
    explicit operator bool () const {
        return loaded_;
    }

    int version () const {
        return header_.m_nVersion;
    }
    int mapRevision () const {
        return header_.mapRevision;
    }

    std::vector<GameLump>& gameLumps () {
        return gameLumps_;
    }
    const std::vector<GameLump>& gameLumps () const {
        return gameLumps_;
    }

    // writes modified BSP to disk (If outputPath is empty overwrites the original file)
    bool bake (std::string_view outputPath = {}) const;

    private:
    bool load (std::string_view path);
    void parseGameLumps ();

    bool bakeFast (const std::string& dest) const;

    bool bakeFullRewrite (const std::string& dest) const;

    std::string path_;
    bool loaded_ = false;
    BSPHeader_t header_{};
    std::vector<uint8_t> rawFile_;
    std::vector<GameLump> gameLumps_;
};