#include "BSP.h"
#include "LZMA.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

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

struct ByteWriter {
    std::vector<uint8_t> buf;

    void reserve (size_t n) {
        buf.reserve (n);
    }
    size_t tell () const {
        return buf.size ();
    }

    template <typename T> void write (const T& v) {
        const auto* p = reinterpret_cast<const uint8_t*> (&v);
        buf.insert (buf.end (), p, p + sizeof (v));
    }
    void writeBytes (const uint8_t* src, size_t len) {
        buf.insert (buf.end (), src, src + len);
    }
    void align4 () {
        while (buf.size () % 4)
            buf.push_back (0);
    }
};

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

bool BSP::bake (std::string_view outputPath) const {
    if (!loaded_)
        return false;
    const std::string dest = outputPath.empty () ? path_ : std::string{ outputPath };

    const bool anyCompressed = std::ranges::any_of (gameLumps_,
    [] (const GameLump& gl) { return (gl.flags & GAMELUMPFLAG_COMPRESSED) != 0; });

    return anyCompressed ? bakeFullRewrite (dest) : bakeFast (dest);
}

bool BSP::bakeFast (const std::string& dest) const {
    std::error_code ec;
    fs::copy_file (fs::path{ path_ }, fs::path{ dest },
    fs::copy_options::overwrite_existing, ec);
    if (ec)
        return false;

    std::fstream f (dest, std::ios::binary | std::ios::in | std::ios::out);
    if (!f)
        return false;

    for (const GameLump& gl : gameLumps_) {
        if (gl.fileofs <= 0 || gl.data.empty ())
            continue;
        f.seekp (static_cast<std::streamoff> (gl.fileofs));
        f.write (reinterpret_cast<const char*> (gl.data.data ()),
        static_cast<std::streamsize> (gl.data.size ()));
        if (!f)
            return false;
    }

    return true;
}

// only for compressed lumps (otherwise bakeFast is much faster)
bool BSP::bakeFullRewrite (const std::string& dest) const {
    ByteWriter w;
    w.reserve (rawFile_.size ());

    BSPHeader_t newHdr = header_;
    w.write (newHdr);

    std::vector<int> order;
    if (header_.lumps[LUMP_MAP_FLAGS].filelen > 0)
        order.push_back (LUMP_MAP_FLAGS);

    std::vector<int> rest;
    for (int i = 0; i < HEADER_LUMPS; ++i) {
        if (i == LUMP_MAP_FLAGS || i == LUMP_PAKFILE)
            continue;
        if (header_.lumps[i].filelen > 0 || i == LUMP_GAME_LUMP)
            rest.push_back (i);
    }
    std::sort (rest.begin (), rest.end (), [&] (int a, int b) {
        return header_.lumps[a].fileofs < header_.lumps[b].fileofs;
    });
    for (int i : rest)
        order.push_back (i);
    if (header_.lumps[LUMP_PAKFILE].filelen > 0)
        order.push_back (LUMP_PAKFILE);

    for (const int idx : order) {
        w.align4 ();
        const size_t lumpStart = w.tell ();

        if (idx == LUMP_GAME_LUMP) {
            // Precompute compressed blobs (just once per lump))
            struct GLBlob {
                std::vector<uint8_t> payload;
                bool wasCompressed;
            };
            std::vector<GLBlob> blobs;
            blobs.reserve (gameLumps_.size ());

            bool anyCompressed = false;
            for (const GameLump& gl : gameLumps_) {
                GLBlob b;
                b.wasCompressed = (gl.flags & GAMELUMPFLAG_COMPRESSED) != 0;
                if (b.wasCompressed) {
                    anyCompressed    = true;
                    unsigned int sz  = 0;
                    unsigned char* c = LZMA_Compress (gl.data.data (),
                    static_cast<unsigned int> (gl.data.size ()), &sz);
                    if (c) {
                        b.payload.assign (c, c + sz);
                        free (c);
                    } else {
                        b.payload       = gl.data;
                        b.wasCompressed = false;
                    }
                } else {
                    b.payload = gl.data;
                }
                blobs.push_back (std::move (b));
            }

            const int cnt =
            static_cast<int> (gameLumps_.size ()) + (anyCompressed ? 1 : 0);
            const size_t dirBytes = sizeof (dgamelumpheader_t) +
            static_cast<size_t> (cnt) * sizeof (dgamelump_t);
            uint32_t glCurOfs = static_cast<uint32_t> (lumpStart + dirBytes);

            dgamelumpheader_t glHdr{};
            glHdr.lumpCount = cnt;
            w.write (glHdr);

            for (size_t i = 0; i < gameLumps_.size (); ++i) {
                const GameLump& gl = gameLumps_[i];
                const GLBlob& b    = blobs[i];
                dgamelump_t entry{};
                entry.id = gl.id;
                entry.flags =
                b.wasCompressed ? gl.flags : (gl.flags & ~GAMELUMPFLAG_COMPRESSED);
                entry.version = gl.version;
                entry.fileofs = static_cast<int> (glCurOfs);
                entry.filelen = gl.filelen;
                w.write (entry);
                glCurOfs += static_cast<uint32_t> (b.payload.size ());
            }
            if (anyCompressed) {
                dgamelump_t dummy{};
                dummy.fileofs = static_cast<int> (glCurOfs);
                w.write (dummy);
            }
            for (const GLBlob& b : blobs)
                w.writeBytes (b.payload.data (), b.payload.size ());

        } else {
            const lump_t& l = header_.lumps[idx];
            if (l.filelen <= 0 || l.fileofs <= 0)
                continue;
            if (static_cast<size_t> (l.fileofs + l.filelen) > rawFile_.size ())
                continue;
            w.writeBytes (rawFile_.data () + l.fileofs, static_cast<size_t> (l.filelen));
        }

        const size_t lumpEnd      = w.tell ();
        newHdr.lumps[idx].fileofs = static_cast<int> (lumpStart);
        newHdr.lumps[idx].filelen = static_cast<int> (lumpEnd - lumpStart);
    }

    memcpy (w.buf.data (), &newHdr, sizeof (newHdr));

    std::ofstream f (dest, std::ios::binary | std::ios::trunc);
    if (!f)
        return false;
    f.write (reinterpret_cast<const char*> (w.buf.data ()),
    static_cast<std::streamsize> (w.buf.size ()));
    return f.good ();
}