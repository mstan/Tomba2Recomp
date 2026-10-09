/* Native first-run preparation from the player's disc. The cache contains
 * original resources only, never initialized gameplay state. */
#include "mod_plugins.h"
#include "mod_runtime.h"
#include "psx_sha256.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {
using Bytes=std::vector<unsigned char>;
struct CatalogEntry { const char *path; unsigned lba, size; const char *hash; };
#include "tomba2_seamless_catalog.inc"
struct TextureEntry { unsigned offset,size,width,decoded; const char *hash; };
#include "tomba2_seamless_textures.inc"
std::vector<Bytes> resident;
std::vector<Bytes> decoded;
std::string digest(const Bytes &b) {
    unsigned char h[32]; char hex[65];
    psx_sha256_compute(b.data(),b.size(),h);
    for(unsigned i=0;i<32;i++) std::snprintf(hex+2*i,3,"%02x",h[i]);
    return hex;
}
std::filesystem::path cache_path() {
    /* Sector patches are part of the effective disc. Never let a warm stock
     * cache bypass a changed mod plan; preparation will reject changed assets. */
    const auto &plan=PSXRecompV4::mod_runtime_fingerprint();
    const std::string name="scus94454-resources-v2-"+
        digest(Bytes(plan.begin(),plan.end()))+".pack";
    if(const char *p=std::getenv("TOMBA2_SEAMLESS_CACHE"))
        if(*p) return std::filesystem::path(p)/name;
    std::filesystem::path root;
#ifdef _WIN32
    if(const char *p=std::getenv("LOCALAPPDATA")) root=p;
#else
    if(const char *p=std::getenv("XDG_CACHE_HOME")) root=p;
    else if(const char *p=std::getenv("HOME")) root=std::filesystem::path(p)/".cache";
#endif
    if(root.empty()) throw std::runtime_error("no cache directory");
    return root/"Tomba2Recomp"/"seamless"/name;
}
const Bytes &images(const std::vector<Bytes> &source) {
    for(size_t i=0;i<std::size(catalog);i++)
        if(!std::strcmp(catalog[i].path,"CD/TOMBA2.IMG")) return source.at(i);
    throw std::runtime_error("texture container missing");
}
Bytes decode(const Bytes &img,const TextureEntry &t) {
    if(t.offset>img.size() || t.size>img.size()-t.offset) throw std::runtime_error("texture bounds");
    const int deltas[]={0,-1,-2*int(t.width),-2*int(t.width)-1,-2*int(t.width)-2,
                       -2*int(t.width)-3,-2*int(t.width)+1,-2*int(t.width)+2};
    Bytes out; out.reserve(t.decoded);
    const auto *p=img.data()+t.offset;
    for(unsigned i=0;i<t.size;) {
        unsigned token=p[i++],length=token>>3,kind=token&7;
        if(!kind && !length) break;
        if(length>t.decoded-out.size()) throw std::runtime_error("texture output bound");
        if(!kind) {
            if(length>t.size-i) throw std::runtime_error("texture literal bound");
            out.insert(out.end(),p+i,p+i+length); i+=length;
        } else while(length--) {
            int64_t offset=int64_t(out.size())+deltas[kind];
            if(offset<0 || uint64_t(offset)>=out.size()) throw std::runtime_error("texture reference bound");
            out.push_back(out[size_t(offset)]);
        }
    }
    if(out.size()!=t.decoded || digest(out)!=t.hash) throw std::runtime_error("decoded texture mismatch");
    return out;
}
bool load(const std::filesystem::path &path, std::vector<Bytes> &result) {
    std::ifstream in(path,std::ios::binary);
    char magic[8];
    if(!in.read(magic,8) || std::memcmp(magic,"T2RES002",8)) return false;
    result.clear();
    for(const auto &item:catalog) {
        Bytes b((item.size+2047u)&~2047u);
        if(!in.read(reinterpret_cast<char*>(b.data()),b.size()) || digest(b)!=item.hash) return false;
        result.push_back(std::move(b));
    }
    decoded.clear();
    for(const auto &t:textures) {
        Bytes b(t.decoded);
        if(!in.read(reinterpret_cast<char*>(b.data()),b.size()) || digest(b)!=t.hash) return false;
        decoded.push_back(std::move(b));
    }
    return in.peek()==std::char_traits<char>::eof();
}
void prepare(const std::filesystem::path &path) {
    std::fprintf(stdout,"tomba2 seamless: preparing original disc resources (one time)\n");
    std::fflush(stdout);
    std::vector<Bytes> result;
    for(const auto &item:catalog) {
        /* Original-disc probe verifies that every last-sector tail is zero.
         * Validate the entire padded content against the catalog each launch. */
        Bytes b((item.size+2047u)&~2047u,0);
        uint32_t size=0;
        if(!psx_mod_read_disc_file(item.path,b.data(),uint32_t(b.size()),&size) ||
           size!=item.size || digest(b)!=item.hash)
            throw std::runtime_error(std::string("disc resource mismatch: ")+item.path);
        result.push_back(std::move(b));
    }
    decoded.clear();
    for(const auto &t:textures) decoded.push_back(decode(images(result),t));
    std::filesystem::create_directories(path.parent_path());
    auto tmp=path;
#ifdef _WIN32
    tmp+="."+std::to_string(GetCurrentProcessId())+".tmp";
#else
    tmp+="."+std::to_string(getpid())+".tmp";
#endif
    std::ofstream out(tmp,std::ios::binary|std::ios::trunc);
    out.write("T2RES002",8);
    for(const auto &b:result) out.write(reinterpret_cast<const char*>(b.data()),b.size());
    for(const auto &b:decoded) out.write(reinterpret_cast<const char*>(b.data()),b.size());
    out.close();
    if(!out) throw std::runtime_error("cannot write prepared resource cache");
#ifdef _WIN32
    if(!MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("cannot publish prepared resource cache");
#else
    std::filesystem::rename(tmp,path);
#endif
    resident=std::move(result);
}
}
extern "C" int tomba2_seamless_prepare(void) {
    try {
        auto path=cache_path();
        if(!load(path,resident)) prepare(path);
        size_t bytes=0; for(const auto &b:resident) bytes+=b.size();
        for(const auto &b:decoded) bytes+=b.size();
        std::fprintf(stdout,"tomba2 seamless: %zu resources, %zu decoded textures, %zu resident bytes; cache=%s\n",
            resident.size(),decoded.size(),bytes,path.string().c_str());
        return 1;
    } catch(const std::exception &e) {
        resident.clear(); decoded.clear();
        std::fprintf(stderr,"tomba2 seamless: preparation failed (%s); using retail loading\n",e.what());
        return 0;
    }
}
extern "C" const unsigned char *tomba2_seamless_source(unsigned lba,unsigned bytes) {
    if(resident.size()!=std::size(catalog)) return nullptr;
    for(size_t i=0;i<resident.size();i++) {
        const auto &c=catalog[i];
        if(lba<c.lba) continue;
        const uint64_t offset=uint64_t(lba-c.lba)*2048;
        if(offset<=resident[i].size() && bytes<=resident[i].size()-offset)
            return resident[i].data()+offset;
    }
    return nullptr;
}
extern "C" const unsigned char *tomba2_seamless_texture(unsigned width,
    const unsigned char *encoded,unsigned size,unsigned *output_size) {
    if(resident.size()!=std::size(catalog) || decoded.size()!=std::size(textures)) return nullptr;
    const auto &img=images(resident);
    for(size_t i=0;i<std::size(textures);i++) {
        const auto &t=textures[i];
        if(t.width==width && t.size==size && !std::memcmp(encoded,img.data()+t.offset,size)) {
            *output_size=t.decoded; return decoded[i].data();
        }
    }
    return nullptr;
}
