#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <vector>

// Smash 64 US 1.0 format metadata, derived from ssb-decomp-re commit
// 572a747d8171137718fad472c5d7473dc7309ffd. No game bytes live here.
namespace tomba2::mario {
using Bytes = std::vector<uint8_t>;
inline constexpr uint32_t rom_size = 16u * 1024u * 1024u;
inline constexpr const char* rom_sha256 =
    "15592e79d3c5295cef4371d4992f0bd25bec2102fc29644c93e682f7ea99ef3d";

Bytes decompress_vpk(const Bytes& input, uint32_t expected_size);
Bytes extract_reloc(const Bytes& rom, uint32_t id);
std::vector<uint32_t> required_files();
// An immutable, versioned asset cache. Does not contain the ROM or its path.
Bytes build_cache(const Bytes& canonical_rom);
std::map<uint32_t, Bytes> read_cache(const Bytes& cache);
// Mono 32000 Hz signed PCM, keyed by the original Mario voice ID 429..441.
std::map<uint32_t, Bytes> extract_voices(const Bytes& canonical_rom);
inline constexpr uint32_t voice_cache_base = 10000;

struct Vec3 { float x = 0, y = 0, z = 0; };
struct Transform {
    Vec3 translation, rotation;
    Vec3 scale{1, 1, 1};
};
struct Joint { int parent = -1; Transform bind; };
struct Vertex {
    Vec3 position, normal;
    float u = 0, v = 0;
    uint16_t joint = 0; // cached seam vertices can belong to the preceding bone
};
struct Triangle {
    uint16_t joint = 0, texture = 0;
    uint8_t cull = 2; // F3DEX2 front/back geometry-mode bits, normalized to 1/2
    std::array<Vertex, 3> vertices;
};
struct Texture {
    uint16_t width = 1, height = 1;
    uint8_t mode_s = 2, mode_t = 2; // N64 wrap/mirror/clamp flags
    std::vector<uint16_t> pixels; // PS1 RGBA5551; bit 15 marks opaque texels.
};
struct Model {
    std::vector<Joint> joints;
    std::vector<Triangle> triangles;
    std::vector<Texture> textures;
    Texture hat_icon;
};
Model decode_model(const std::map<uint32_t, Bytes>& files);
struct TextureAtlas {
    Texture texture;
    std::vector<std::array<uint16_t,2>> origins;
};
// Immutable owner-derived pixels, with replicated borders for each material.
// One bank avoids a GL batch break at every interleaved material in the OT.
TextureAtlas make_texture_atlas(const Model& model);

struct Animation {
    uint32_t id = 0;
    float duration = 1;
    // Frames sampled from the original Figatree curves at the source 60 Hz.
    std::vector<std::array<Transform, 25>> frames;
};
Animation decode_animation(uint32_t id, const Bytes& raw, const Model& model);
} // namespace tomba2::mario
