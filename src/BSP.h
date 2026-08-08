#pragma once

#include "BSPTypes.h"
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <array>
#include <cctype>
#include <optional>

struct GameLump {
    int id;
    uint16_t flags;
    uint16_t version;
    int fileofs;               // absolute offset in the original BSP
    int filelen;               // uncompressed length
    std::vector<uint8_t> data; // always uncompressed data blob (empty if fileofs/filelen invalid or missing in the BSP)
};

struct RenameResult {
    bool ok         = true;
    int  pakEntries = 0;
    int  pakRenamed = 0;
    bool pakIsLzma  = false;
};

class BSP {
public:
    explicit BSP(std::string_view path);
    explicit operator bool() const { return loaded_; }

    int version()     const { return header_.m_nVersion; }
    int mapRevision() const { return header_.mapRevision; }

    std::vector<GameLump>&       gameLumps()       { return gameLumps_; }
    const std::vector<GameLump>& gameLumps() const { return gameLumps_; }

    // writes modified BSP to disk (If outputPath is empty overwrites the original file)
    bool bake(std::string_view outputPath = {}) const;

    RenameResult renameMapReferences(std::string_view oldStem);

private:
    bool load(std::string_view path);
    void parseGameLumps();

    void burnPatchedGameLumps();
    void recompressGameLumps();
    RenameResult patchPakfile(std::string_view oldStem, std::string_view newStem);
    void patchTexdata(std::string_view oldStem, std::string_view newStem);

    std::string           path_;
    bool                  loaded_ = false;
    BSPHeader_t           header_{};
    std::vector<uint8_t>  rawFile_;
    std::vector<GameLump> gameLumps_;

    std::unordered_map<int, std::vector<uint8_t>> pendingLumps_;
};

// ZIP Pakfile (same format from zip_uncompressed.h)
// The bspzip reader explicitly rejects compressionMethod != 0 and never reads data descriptors  
// and as i learned with minizip it dont replicate this by deafult so 
// its better implementing exact format to do it correctly
inline constexpr uint32_t pkid(uint8_t a, uint8_t b) {
    return (static_cast<uint32_t>(b) << 24) | (static_cast<uint32_t>(a) << 16) |
           (static_cast<uint32_t>('K') << 8) | 'P';
}
inline constexpr uint32_t ZIP_SIG_LOCAL   = pkid(3, 4);
inline constexpr uint32_t ZIP_SIG_CENTRAL = pkid(1, 2);
inline constexpr uint32_t ZIP_SIG_EOCD    = pkid(5, 6);
inline constexpr int      XZIP_COMMENT_LENGTH = 32;

#pragma pack(push, 1)
struct ZipLocalHeader {
    uint32_t signature;
    uint16_t versionNeeded;
    uint16_t flags;
    uint16_t compressionMethod;
    uint16_t modTime;
    uint16_t modDate;
    uint32_t crc32;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint16_t fileNameLength;
    uint16_t extraFieldLength;
};
static_assert(sizeof(ZipLocalHeader) == 30);

struct ZipCentralHeader {
    uint32_t signature;
    uint16_t versionMadeBy;
    uint16_t versionNeeded;
    uint16_t flags;
    uint16_t compressionMethod;
    uint16_t modTime;
    uint16_t modDate;
    uint32_t crc32;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint16_t fileNameLength;
    uint16_t extraFieldLength;
    uint16_t fileCommentLength;
    uint16_t diskNumberStart;
    uint16_t internalAttribs;
    uint32_t externalAttribs;
    uint32_t localHeaderOffset;
};
static_assert(sizeof(ZipCentralHeader) == 46);

struct ZipEOCD {
    uint32_t signature;
    uint16_t diskNumber;
    uint16_t centralDirDisk;
    uint16_t centralDirEntriesThisDisk;
    uint16_t centralDirEntriesTotal;
    uint32_t centralDirSize;
    uint32_t centralDirOffset;
    uint16_t commentLength;
};
static_assert(sizeof(ZipEOCD) == 22);
#pragma pack(pop)