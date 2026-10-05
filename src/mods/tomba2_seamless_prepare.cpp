/* Tomba! 2 resources held resident through the framework's resident disc
 * packs (mod_resident.h): read once from the effective (mod-patched) disc,
 * SHA-256 verified and cached per mod plan. The pack contains disc resources
 * and decoded textures only, never initialized gameplay state. */
#include "mod_plugins.h"
#include "mod_resident.h"
#include "psx_sha256.h"
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>
#include <vector>

namespace {
struct CatalogEntry { const char *path; unsigned lba, size; const char *hash; };
#include "tomba2_seamless_catalog.inc"
struct TextureEntry { unsigned offset, size, width, decoded; const char *hash; };
#include "tomba2_seamless_textures.inc"

constexpr uint32_t kTextureTag = 0x54455854u; /* 'TEXT' */
const PSXResidentPack *pack;
uint32_t img_file = ~0u;
/* Derived blob index per catalogued texture, or ~0u when not prepared. */
std::vector<uint32_t> texture_blob;

std::string digest(const uint8_t *p, size_t n) {
    uint8_t h[32];
    char hex[65];
    psx_sha256_compute(p, n, h);
    for (unsigned i = 0; i < 32; i++) std::snprintf(hex + 2 * i, 3, "%02x", h[i]);
    return hex;
}

/* The original MAIN.EXE texture decoder at 0x80044D8C (catalogued offsets,
 * widths and outputs verified with the original MIPS routine). */
bool decode(const uint8_t *img, uint32_t img_size, const TextureEntry &t, std::vector<uint8_t> &out) {
    if (t.offset > img_size || t.size > img_size - t.offset) return false;
    const int deltas[] = {0, -1, -2 * int(t.width), -2 * int(t.width) - 1, -2 * int(t.width) - 2,
                          -2 * int(t.width) - 3, -2 * int(t.width) + 1, -2 * int(t.width) + 2};
    out.clear();
    out.reserve(t.decoded);
    const uint8_t *p = img + t.offset;
    for (unsigned i = 0; i < t.size;) {
        unsigned token = p[i++], length = token >> 3, kind = token & 7;
        if (!kind && !length) break;
        if (length > t.decoded - out.size()) return false;
        if (!kind) {
            if (length > t.size - i) return false;
            out.insert(out.end(), p + i, p + i + length);
            i += length;
        } else {
            while (length--) {
                const int64_t offset = int64_t(out.size()) + deltas[kind];
                if (offset < 0 || uint64_t(offset) >= out.size()) return false;
                out.push_back(out[size_t(offset)]);
            }
        }
    }
    return out.size() == t.decoded && digest(out.data(), out.size()) == t.hash;
}

/* Textures are prepared only from the original TOMBA2.IMG: the catalogued
 * offsets describe that file. A modified IMG leaves texture decoding to the
 * game's own routine. */
int derive(PSXResidentSink *sink, uint32_t file, const uint8_t *data, uint32_t size, int stock, void *) {
    if (std::strcmp(catalog[file].path, "CD/TOMBA2.IMG") || !stock) return 1;
    std::vector<uint8_t> out;
    for (size_t i = 0; i < std::size(textures); i++) {
        const auto &t = textures[i];
        if (!decode(data, size, t, out)) return 0;
        const uint32_t meta[4] = {t.offset, t.size, t.width, uint32_t(i)};
        if (!psx_resident_emit(sink, file, kTextureTag, meta, out.data(), uint32_t(out.size()))) return 0;
    }
    return 1;
}
}  // namespace

extern "C" int tomba2_seamless_prepare(void) {
    static PSXResidentFile files[std::size(catalog)];
    for (size_t i = 0; i < std::size(catalog); i++) {
        files[i] = {catalog[i].path, catalog[i].size, catalog[i].hash};
        if (!std::strcmp(catalog[i].path, "CD/TOMBA2.IMG")) img_file = uint32_t(i);
    }
    PSXResidentSpec spec{};
    spec.struct_size = sizeof spec;
    spec.title = "Tomba2Recomp";
    spec.format = "scus94454-resources-v3";
    spec.files = files;
    spec.file_count = uint32_t(std::size(files));
    spec.policy = PSX_RESIDENT_ALLOW_MODIFIED;
    spec.derive = derive;
    texture_blob.assign(std::size(textures), ~0u);
    pack = psx_resident_prepare(&spec);
    if (!pack) return 0;
    for (uint32_t d = 0; d < psx_resident_derived_count(pack); d++) {
        uint32_t tag = 0, meta[4] = {};
        if (psx_resident_derived(pack, d, nullptr, &tag, meta, nullptr) && tag == kTextureTag &&
            meta[3] < texture_blob.size())
            texture_blob[meta[3]] = d;
    }
    psx_mod_counter_add("tomba2.seamless.modified_files", psx_resident_modified_files(pack));
    return 1;
}

/* Bytes at effective LBA `lba`, entirely within one resident file. */
extern "C" const unsigned char *tomba2_seamless_source(unsigned lba, unsigned bytes) {
    return pack ? psx_resident_find_lba(pack, lba, bytes, nullptr) : nullptr;
}

/* Decoded output for an encoded texture whose bytes match a catalogued
 * original texture exactly; NULL hands decoding back to the game. */
extern "C" const unsigned char *tomba2_seamless_texture(unsigned width, const unsigned char *encoded,
                                                        unsigned size, unsigned *output_size) {
    uint32_t img_size = 0;
    const uint8_t *img = pack ? psx_resident_file(pack, img_file, &img_size, nullptr) : nullptr;
    if (!img) return nullptr;
    for (size_t i = 0; i < std::size(textures); i++) {
        const auto &t = textures[i];
        if (texture_blob[i] == ~0u || t.width != width || t.size != size || t.offset > img_size ||
            t.size > img_size - t.offset || std::memcmp(encoded, img + t.offset, size))
            continue;
        uint32_t n = 0;
        const uint8_t *p = psx_resident_derived(pack, texture_blob[i], nullptr, nullptr, nullptr, &n);
        if (!p) return nullptr;
        *output_size = n;
        return p;
    }
    return nullptr;
}
