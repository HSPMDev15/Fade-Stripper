#include "BSP.h"
#include "LZMA.h"
#include "log.h"
#include <algorithm>
#include <fstream>

static std::vector<uint8_t> readFile (const std::string& path) {
    std::ifstream f (path, std::ios::binary | std::ios::ate);
    if (!f)
        return {};
    const auto sz = static_cast<size_t> (f.tellg ());
    f.seekg (0);
    std::vector<uint8_t> buf (sz);
    f.read (reinterpret_cast<char*> (buf.data ()), static_cast<std::streamsize> (sz));
    return f ? buf : std::vector<uint8_t>{};
}

static uint32_t zipCrc32(const uint8_t* data, size_t len) {
    static uint32_t table[256];
    static bool     ready = false;
    if (!ready) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        ready = true;
    }
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

bool BSP::load (std::string_view path) {
    rawFile_ = readFile (std::string{ path });
    if (rawFile_.size () < sizeof (BSPHeader_t))
        return false;

    memcpy (&header_, rawFile_.data (), sizeof (BSPHeader_t));

    if (static_cast<uint32_t> (header_.ident) != IDBSPHEADER)
        return false;
    if (header_.m_nVersion < MINBSPVERSION || header_.m_nVersion > BSPVERSION)
        return false;

    parseGameLumps ();
    return true;
}

BSP::BSP (std::string_view path) : path_{ path } {
    loaded_ = load (path);
}

void BSP::parseGameLumps () {
    const lump_t& gl = header_.lumps[LUMP_GAME_LUMP];
    if (gl.filelen <= 0 || gl.fileofs <= 0)
        return;
    if (static_cast<size_t> (gl.fileofs + gl.filelen) > rawFile_.size ())
        return;

    const uint8_t* base = rawFile_.data () + gl.fileofs;
    const int len       = gl.filelen;
    if (len < static_cast<int> (sizeof (dgamelumpheader_t)))
        return;

    dgamelumpheader_t hdr;
    memcpy (&hdr, base, sizeof (hdr));
    if (hdr.lumpCount <= 0 || hdr.lumpCount > 64)
        return;

    const size_t dirSize = sizeof (dgamelumpheader_t) +
    static_cast<size_t> (hdr.lumpCount) * sizeof (dgamelump_t);
    if (dirSize > static_cast<size_t> (len))
        return;

    std::vector<dgamelump_t> entries (static_cast<size_t> (hdr.lumpCount));
    memcpy (entries.data (), base + sizeof (dgamelumpheader_t),
    static_cast<size_t> (hdr.lumpCount) * sizeof (dgamelump_t));

    for (int i = 0; i < hdr.lumpCount; ++i) {
        const dgamelump_t& e = entries[i];
        if (e.id == 0)
            continue;

        GameLump gl2{};
        gl2.id      = e.id;
        gl2.flags   = e.flags;
        gl2.version = e.version;
        gl2.fileofs = e.fileofs;
        gl2.filelen = e.filelen;

        const bool compressed = (e.flags & GAMELUMPFLAG_COMPRESSED) != 0;

        if (e.fileofs <= 0 || static_cast<size_t> (e.fileofs) >= rawFile_.size ()) {
            gameLumps_.push_back (std::move (gl2));
            continue;
        }

        int diskLen;
        if (compressed) {
            int nextOfs = gl.fileofs + gl.filelen;
            if (i + 1 < hdr.lumpCount)
                nextOfs = entries[i + 1].fileofs;
            diskLen = nextOfs - e.fileofs;
        } else {
            diskLen = e.filelen;
        }

        if (diskLen <= 0 || static_cast<size_t> (e.fileofs + diskLen) > rawFile_.size ()) {
            gameLumps_.push_back (std::move (gl2));
            continue;
        }

        const uint8_t* src = rawFile_.data () + e.fileofs;
        if (compressed) {
            unsigned char* dec = nullptr;
            unsigned int decSz = 0;
            if (LZMA_Uncompress (src, &dec, &decSz) && dec) {
                gl2.data.assign (dec, dec + decSz);
                free (dec);
            }
        } else {
            gl2.data.assign (src, src + diskLen);
        }

        gameLumps_.push_back (std::move (gl2));
    }
}
// Writes the full BSP to the disk.
// renameMapReferences() always leaves rawFile_ and header_ consistent
// with each other before this runs, so a single rewrite path covers
// every case (fade-only patch or full rename).
bool BSP::bake(std::string_view outputPath) const {
    if (!loaded_) return false;
    const std::string dest = outputPath.empty() ? path_ : std::string{outputPath};

    Info("Writing BSP {}...", dest);

    std::ofstream f(dest, std::ios::binary | std::ios::trunc);
    if (!f) return false;

    const lump_t& pk = header_.lumps[LUMP_PAKFILE];
    f.write(reinterpret_cast<const char*>(&header_), sizeof(header_));

    const size_t headerEnd = sizeof(header_);
    const size_t pakOffset = static_cast<size_t>(pk.fileofs);

    if (pakOffset > headerEnd && pakOffset <= rawFile_.size()) {
        f.write(reinterpret_cast<const char*>(rawFile_.data() + headerEnd), pakOffset - headerEnd);
    }

    // Renamed pakfile if present, otherwise the untouched original.
    if (const auto it = pendingLumps_.find(LUMP_PAKFILE); it != pendingLumps_.end()) {

        f.write(reinterpret_cast<const char*>(it->second.data()), it->second.size());

    } else if (pk.filelen > 0 &&  static_cast<size_t>(pk.fileofs + pk.filelen) <= rawFile_.size()) {

        f.write(reinterpret_cast<const char*>(rawFile_.data() + pk.fileofs), static_cast<size_t>(pk.filelen));
    }

    const bool ok = f.good();
    if (ok) Info("Done!");
    return ok;
}

std::vector<uint8_t> BSP::readLump(int lumpIndex) const {
    const lump_t& l = header_.lumps[lumpIndex];
    if (l.filelen <= 0) return {};

    uint32_t magic = 0;
    if (l.filelen >= 4)
        memcpy(&magic, rawFile_.data() + l.fileofs, sizeof(magic));

    if (magic == LZMA_ID) {
        unsigned char* dec = nullptr; unsigned int decSz = 0;
        if (LZMA_Uncompress(rawFile_.data() + l.fileofs, &dec, &decSz) && dec) {
            std::vector<uint8_t> out(dec, dec + decSz);
            free(dec);
            return out;
        }
        return {};
    }
    return std::vector<uint8_t>(rawFile_.data() + l.fileofs,
                                rawFile_.data() + l.fileofs + l.filelen);
}

void BSP::writeLump(int lumpIndex, const uint8_t* data, size_t size) {
    const lump_t& l = header_.lumps[lumpIndex];

    uint32_t magic = 0;
    if (l.filelen >= 4)
        memcpy(&magic, rawFile_.data() + l.fileofs, sizeof(magic));
    const bool compress = (magic == LZMA_ID);

    std::vector<uint8_t> payload;
    if (compress) {
        unsigned int cSz = 0;
        unsigned char* comp = LZMA_Compress(data, static_cast<unsigned int>(size), &cSz);
        if (!comp) return;
        payload.assign(comp, comp + cSz);
        free(comp);
    } else {
        payload.assign(data, data + size);
    }

    lump_t& lref         = header_.lumps[lumpIndex];
    const int diff       = static_cast<int>(payload.size()) - lref.filelen;
    const int alignDiff  = diff > 0 ? ((diff + 3) & ~3) : 0;

    if (alignDiff > 0) {
        const size_t oldSz     = rawFile_.size();
        const int    moveStart = lref.fileofs + lref.filelen;
        rawFile_.resize(oldSz + static_cast<size_t>(alignDiff));
        uint8_t* base = rawFile_.data();
        memmove(base + moveStart + alignDiff, base + moveStart,
                oldSz - static_cast<size_t>(moveStart));
        for (lump_t& ml : header_.lumps)
            if (ml.fileofs > lref.fileofs) ml.fileofs += alignDiff;
        const lump_t& gl = header_.lumps[LUMP_GAME_LUMP];
        if (gl.filelen > 0) {
            auto* glHdr     = reinterpret_cast<dgamelumpheader_t*>(rawFile_.data() + gl.fileofs);
            auto* glEntries = reinterpret_cast<dgamelump_t*>(rawFile_.data() + gl.fileofs + sizeof(dgamelumpheader_t));
            for (int j = 0; j < glHdr->lumpCount; ++j)
                if (glEntries[j].fileofs > lref.fileofs)
                    glEntries[j].fileofs += alignDiff;
        }
    }

    memcpy(rawFile_.data() + header_.lumps[lumpIndex].fileofs,
           payload.data(), payload.size());
    header_.lumps[lumpIndex].filelen = static_cast<int>(payload.size());
}
// Writes patched uncompressed game lumps back to rawFile_.
// Compressed lumps are handled separately in recompressGameLumps().
void BSP::burnPatchedGameLumps() {
    for (const GameLump& lump : gameLumps_) {
        if (lump.fileofs <= 0 || lump.data.empty()) continue;

        if (lump.flags & GAMELUMPFLAG_COMPRESSED) continue;
        if (static_cast<size_t>(lump.fileofs) + lump.data.size() > rawFile_.size()) continue;
        memcpy(rawFile_.data() + lump.fileofs, lump.data.data(), lump.data.size());
    }
}

// Recompresses patched static prop lumps that were originally LZMA compressed
// Only sprp is ever modified; other compressed sub-lumps are left as is
void BSP::recompressGameLumps() {
    const lump_t& glLump = header_.lumps[LUMP_GAME_LUMP];
    if (glLump.filelen <= 0) return;

    uint8_t* base   = rawFile_.data();
    auto* glHdr     = reinterpret_cast<dgamelumpheader_t*>(base + glLump.fileofs);
    auto* glEntries = reinterpret_cast<dgamelump_t*>(
                          base + glLump.fileofs + sizeof(dgamelumpheader_t));
    const int count = glHdr->lumpCount;

    for (int i = 0; i < static_cast<int>(gameLumps_.size()) && i < count; ++i) {
        const GameLump& gl = gameLumps_[i];
        if (!(gl.flags & GAMELUMPFLAG_COMPRESSED)) continue;
        if (gl.id != GAMELUMP_STATIC_PROPS)         continue;
        if (gl.data.empty())                         continue;

        const int thisOfs = glEntries[i].fileofs;
        if (thisOfs <= 0) continue;

        // Compressed slot size is bounded by the next sublump offset,
        // not by the uncompressed size stored in the entry.
        int nextOfs = glLump.fileofs + glLump.filelen;
        for (int j = i + 1; j < count; ++j) {
            if (glEntries[j].fileofs > thisOfs) {
                nextOfs = glEntries[j].fileofs;
                break;
            }
        }
        const int slotSize = nextOfs - thisOfs;
        if (slotSize <= 0) continue;

        unsigned int compSz = 0;
        unsigned char* comp = LZMA_Compress(
            gl.data.data(), static_cast<unsigned int>(gl.data.size()), &compSz);
        if (!comp) {
            Error("LZMA recompression failed for game lump 0x{:08X}", gl.id);
            continue;
        }

        if (static_cast<int>(compSz) > slotSize) {
            Warning("Recompressed sprp ({} bytes) exceeds slot ({} bytes)\n Static Props will not be patched!!!", compSz, slotSize);
            free(comp);
            continue;
        }

        memcpy(base + thisOfs, comp, compSz);
        if (static_cast<int>(compSz) < slotSize)
            memset(base + thisOfs + compSz, 0, static_cast<size_t>(slotSize - compSz));
        free(comp);
    }
}

// Renames pakfile paths and content and implements the exact ZIP layout from
// Valve zip_uncompressed.h following bsp_rename.c four
// rename rules for materials, vgui menu photos, maps, and soundscapes.
RenameResult BSP::patchPakfile(std::string_view oldStem, std::string_view newStem) {
    RenameResult result;

    const lump_t& lump = header_.lumps[LUMP_PAKFILE];
    if (lump.filelen <= 0) return result;

    const uint8_t* zip    = rawFile_.data() + lump.fileofs;
    const size_t   zipLen = static_cast<size_t>(lump.filelen);
    if (zipLen < sizeof(ZipEOCD)) { result.ok = false; return result; }

    // Scan backwards for the EOCD signature
    size_t eocdPos = zipLen - sizeof(ZipEOCD);
    bool   found   = false;
    for (;;) {
        uint32_t sig; memcpy(&sig, zip + eocdPos, sizeof(sig));
        if (sig == ZIP_SIG_EOCD) { found = true; break; }
        if (eocdPos == 0) break;
        --eocdPos;
    }
    if (!found) {
        //Error("Pakfile EOCD not found");
        result.ok = false; return result;
    }
    ZipEOCD eocd; memcpy(&eocd, zip + eocdPos, sizeof(eocd));

    result.pakEntries = static_cast<int>(eocd.centralDirEntriesTotal);
    if (eocd.centralDirEntriesTotal > 0 && eocd.centralDirOffset + sizeof(ZipCentralHeader) <= zipLen) {
        ZipCentralHeader firstCh;
        memcpy(&firstCh, zip + eocd.centralDirOffset, sizeof(firstCh));
        if (firstCh.signature == ZIP_SIG_CENTRAL) {
            result.pakIsLzma = (firstCh.compressionMethod == 14);
        }
    }
    Info("Found {} files in pakfile", result.pakEntries);
    Info("Map is{} compressed", result.pakIsLzma ? "" : " NOT");
    //sleep(1); let user just for one second to see the program logs before the terminal fills with the renamed references logs
    struct Rule { std::string from, to, vmtFind, vmtReplace; bool isMaterial; };
    const std::array<Rule, 4> rules{{
        { "materials/maps/" + std::string{oldStem} + "/",
          "materials/maps/" + std::string{newStem} + "/",
          "maps/" + std::string{oldStem} + "/",
          "maps/" + std::string{newStem} + "/", true },
        { "materials/vgui/maps/menu_photos_" + std::string{oldStem},
          "materials/vgui/maps/menu_photos_" + std::string{newStem},
          "vgui/maps/menu_photos_" + std::string{oldStem},
          "vgui/maps/menu_photos_" + std::string{newStem}, true },
        { "maps/" + std::string{oldStem},
          "maps/" + std::string{newStem}, "", "", false },
        { "scripts/soundscapes_" + std::string{oldStem},
          "scripts/soundscapes_" + std::string{newStem}, "", "", false },
    }};

    struct Entry {
        std::string    name;
        uint16_t       method;
        uint32_t       crc, compSize, uncompSize;
        const uint8_t* origComp;
        std::vector<uint8_t> newData;
        bool           contentModified = false;
    };

    std::vector<Entry> entries;
    entries.reserve(eocd.centralDirEntriesTotal);
    std::optional<uint16_t> globalMethod;

    size_t cd = eocd.centralDirOffset;
    for (uint16_t i = 0; i < eocd.centralDirEntriesTotal; ++i) {
        /*if (cd + sizeof(ZipCentralHeader) > zipLen) {
            Error("Central directory truncated at entry {}", i);
            result.ok = false; return result;
        }*/
        ZipCentralHeader ch; memcpy(&ch, zip + cd, sizeof(ch));
        if (ch.signature != ZIP_SIG_CENTRAL || (ch.compressionMethod != 0 && ch.compressionMethod != 14)) {
            Error("Unsupported entry method={}", ch.compressionMethod); //99.99% impossible since engine wouldnt even load the map but lets keep it...
            result.ok = false; return result;
        }
        if (!globalMethod) globalMethod = ch.compressionMethod;
        cd += sizeof(ch);

        Entry e;
        e.name.assign(reinterpret_cast<const char*>(zip + cd), ch.fileNameLength);
        cd += static_cast<size_t>(ch.fileNameLength) + ch.extraFieldLength + ch.fileCommentLength;

        e.method     = ch.compressionMethod;
        e.crc        = ch.crc32;
        e.compSize   = ch.compressedSize;
        e.uncompSize = ch.uncompressedSize;

        /*if (static_cast<size_t>(ch.localHeaderOffset) + sizeof(ZipLocalHeader) > zipLen) {
            Error("Local header out of range for '{}'", e.name);
            result.ok = false; return result;
        }*/
        ZipLocalHeader lh; memcpy(&lh, zip + ch.localHeaderOffset, sizeof(lh));
        /*if (lh.signature != ZIP_SIG_LOCAL) {
            Error("Bad local header signature for '{}'", e.name);
            result.ok = false; return result;
        }*/
        const size_t dataPos = static_cast<size_t>(ch.localHeaderOffset) + sizeof(ZipLocalHeader) + lh.fileNameLength + lh.extraFieldLength;
        /*if (dataPos + ch.compressedSize > zipLen) {
            Error("Entry data out of range for '{}'", e.name);
            result.ok = false; return result;
        }*/
        e.origComp = zip + dataPos;

        for (char& c : e.name) if (c == '\\') c = '/';
        for (char& c : e.name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        for (const auto& rule : rules) {
            if (!e.name.starts_with(rule.from)) continue;

            const std::string oldName = e.name;
            e.name = rule.to + e.name.substr(rule.from.size());
            Info("Renaming file:\n\t{}\n\t{}", oldName, e.name);
            ++result.pakRenamed;

            // Entries under "materials/" also get their VMT content rewritten
            if (rule.isMaterial && e.name.ends_with(".vmt")) {
                Info("Fixing VMT:\n\t{}", e.name);

                std::vector<uint8_t> raw;
                if (ch.compressionMethod == 14) {
                    if (!LZMA_DecompressZipEntry(e.origComp, ch.compressedSize,ch.uncompressedSize, raw)) {
                        Error("LZMA decompress failed for '{}'", e.name);
                        result.ok = false; return result;
                    }
                } else {
                    raw.assign(e.origComp, e.origComp + ch.uncompressedSize);
                }

                std::string s(raw.begin(), raw.end());
                for (char& c : s) if (c == '\\') c = '/';
                int replacements = 0;
                for (size_t p = 0; (p = s.find(rule.vmtFind, p)) != std::string::npos; ) {
                    s.replace(p, rule.vmtFind.size(), rule.vmtReplace);
                    p += rule.vmtReplace.size();
                    ++replacements;
                }
                Info("\t-> Made {} replacements", replacements);

                e.newData.assign(s.begin(), s.end());
                e.contentModified = true;
            }
            break;
        }

        entries.push_back(std::move(e));
    }

    result.pakIsLzma  = globalMethod.value_or(0) == 14;
    result.pakEntries = static_cast<int>(entries.size());
    const bool useLzma = result.pakIsLzma;

    Info("Writing files...");

    struct Packed { uint32_t offset, crc, compSize, uncompSize; uint16_t method;
                    std::vector<uint8_t> payload; };
    std::vector<Packed> packed(entries.size());
    std::vector<uint8_t> out;
    out.reserve(zipLen + 4096);

    for (size_t i = 0; i < entries.size(); ++i) {
        Entry&  e = entries[i];
        Packed& p = packed[i];
        p.offset  = static_cast<uint32_t>(out.size());
        p.method  = e.contentModified ? (useLzma ? 14 : 0) : e.method;

        // Unmodified entries are copied as raw compressed bytes
        // decompress/recompress round trip its doesnt needed.
        if (e.contentModified) {
            p.crc        = zipCrc32(e.newData.data(), e.newData.size());
            p.uncompSize = static_cast<uint32_t>(e.newData.size());
            if (useLzma) {
                if (!LZMA_CompressZipEntry(e.newData.data(), e.newData.size(), p.payload)) {
                    Error("LZMA compress failed for '{}'", e.name);
                    result.ok = false; return result;
                }
                p.compSize = static_cast<uint32_t>(p.payload.size());
            } else {
                p.compSize = p.uncompSize;
            }
        } else {
            p.crc        = e.crc;
            p.compSize   = e.compSize;
            p.uncompSize = e.uncompSize;
        }

        ZipLocalHeader lh{};
        lh.signature         = ZIP_SIG_LOCAL;
        lh.versionNeeded     = 10;
        lh.flags             = 0;
        lh.compressionMethod = p.method;
        lh.crc32             = p.crc;
        lh.compressedSize    = p.compSize;
        lh.uncompressedSize  = p.uncompSize;
        lh.fileNameLength    = static_cast<uint16_t>(e.name.size());
        lh.extraFieldLength  = 0;
        const auto* lhb = reinterpret_cast<const uint8_t*>(&lh);
        out.insert(out.end(), lhb, lhb + sizeof(lh));
        out.insert(out.end(), e.name.begin(), e.name.end());

        if (e.contentModified) {
            if (useLzma) out.insert(out.end(), p.payload.begin(), p.payload.end());
            else         out.insert(out.end(), e.newData.begin(), e.newData.end());
        } else {
            out.insert(out.end(), e.origComp, e.origComp + e.compSize);
        }

        std::printf("Progress: %zu/%zu (%2.0f%%)           \r", i + 1, entries.size(),
                    entries.size() > 0 ? (static_cast<double>(i + 1) / entries.size()) * 100.0 : 0.0);
        std::fflush(stdout);
    }
    Info("OK                              ");

    const uint32_t cdStart = static_cast<uint32_t>(out.size());
    for (size_t i = 0; i < entries.size(); ++i) {
        const Entry&  e = entries[i];
        const Packed& p = packed[i];

        ZipCentralHeader ch{};
        ch.signature         = ZIP_SIG_CENTRAL;
        ch.versionMadeBy     = 20;
        ch.versionNeeded     = 10;
        ch.flags             = 0;
        ch.compressionMethod = p.method;
        ch.crc32             = p.crc;
        ch.compressedSize    = p.compSize;
        ch.uncompressedSize  = p.uncompSize;
        ch.fileNameLength    = static_cast<uint16_t>(e.name.size());
        ch.localHeaderOffset = p.offset;
        const auto* chb = reinterpret_cast<const uint8_t*>(&ch);
        out.insert(out.end(), chb, chb + sizeof(ch));
        out.insert(out.end(), e.name.begin(), e.name.end());
    }
    const uint32_t cdEnd = static_cast<uint32_t>(out.size());

    ZipEOCD eocdOut{};
    eocdOut.signature                 = ZIP_SIG_EOCD;
    eocdOut.centralDirEntriesThisDisk = static_cast<uint16_t>(entries.size());
    eocdOut.centralDirEntriesTotal    = static_cast<uint16_t>(entries.size());
    eocdOut.centralDirSize            = cdEnd - cdStart;
    eocdOut.centralDirOffset          = cdStart;
    eocdOut.commentLength             = XZIP_COMMENT_LENGTH;
    const auto* eb = reinterpret_cast<const uint8_t*>(&eocdOut);
    out.insert(out.end(), eb, eb + sizeof(eocdOut));

    char comment[XZIP_COMMENT_LENGTH] = {0};
    std::snprintf(comment, sizeof(comment), "XZP1 0");
    out.insert(out.end(), comment, comment + XZIP_COMMENT_LENGTH);

    header_.lumps[LUMP_PAKFILE].filelen = static_cast<int>(out.size());
    pendingLumps_[LUMP_PAKFILE] = std::move(out);
    return result;
}

// Renames texdata string table entries (materials)
// Uses BSPEntSpy load-rename-rebuild approach
void BSP::patchTexdata(std::string_view oldStem, std::string_view newStem) {
    const lump_t& lData  = header_.lumps[LUMP_TEXDATA_STRING_DATA];
    const lump_t& lTable = header_.lumps[LUMP_TEXDATA_STRING_TABLE];
    if (lData.filelen <= 0 || lTable.filelen <= 0) return;

    const std::string find = "maps/" + std::string{oldStem} + "/";
    const std::string repl = "maps/" + std::string{newStem} + "/";

    Info("Shifting texture table...");

    const std::vector<uint8_t> tdBlob = readLump(LUMP_TEXDATA_STRING_DATA);
    const std::vector<uint8_t> ttBlob = readLump(LUMP_TEXDATA_STRING_TABLE);
    if (tdBlob.empty() || ttBlob.empty()) return;

    const int numTex = static_cast<int>(ttBlob.size()) / static_cast<int>(sizeof(int32_t));

    // Load one string per table entry, in table order.
    std::vector<std::string> materials(static_cast<size_t>(numTex));
    for (int i = 0; i < numTex; ++i) {
        int32_t ofs;
        memcpy(&ofs, ttBlob.data() + i * 4, sizeof(ofs));
        if (ofs < 0 || static_cast<size_t>(ofs) >= tdBlob.size()) continue;

        const char* p = reinterpret_cast<const char*>(tdBlob.data() + ofs);
        const char* end = reinterpret_cast<const char*>(tdBlob.data() + tdBlob.size());
        const char* term = p;
        while (term < end && *term) ++term;
        materials[i].assign(p, term);
    }

    // Rename in place string content replacement, no offsets touched yet
    for (auto& mat : materials) {
        for (size_t p = 0; (p = mat.find(find, p)) != std::string::npos; )
            { mat.replace(p, find.size(), repl); p += repl.size(); }
    }

    // write each string and its offset in the same pass.
    std::vector<uint8_t> newTd;
    newTd.reserve(tdBlob.size() + 512);
    std::vector<int32_t> newTable(static_cast<size_t>(numTex));

    for (int i = 0; i < numTex; ++i) {
        newTable[i] = static_cast<int32_t>(newTd.size());
        newTd.insert(newTd.end(), materials[i].begin(), materials[i].end());
        newTd.push_back(0);
    }

    writeLump(LUMP_TEXDATA_STRING_DATA, newTd.data(), newTd.size());
    writeLump(LUMP_TEXDATA_STRING_TABLE, reinterpret_cast<const uint8_t*>(newTable.data()),
              static_cast<size_t>(numTex) * sizeof(int32_t));
}

// patches static prop fades in place, renames the pakfile,
// and renames texdata material references to match the output filename.
RenameResult BSP::renameMapReferences(std::string_view oldStem) {
    if (oldStem.ends_with("_no_fade")) return {};
    const std::string newStem = std::string{oldStem} + "_no_fade";

    burnPatchedGameLumps();
    recompressGameLumps();

    const RenameResult result = patchPakfile(oldStem, newStem);
    if (!result.ok) return result;

    patchTexdata(oldStem, newStem);
    return result;
}