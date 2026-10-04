/* Integration test using an owner-supplied disc. No disc data is distributed.
 * The decoder catalog was verified separately with the original MIPS routine. */
#include "mod_plugins.h"
#include "psx_sha256.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
struct CatalogEntry { const char *path; unsigned lba,size; const char *hash; };
#include "../src/mods/tomba2_seamless_catalog.inc"
struct TextureEntry { unsigned offset,size,width,decoded; const char *hash; };
#include "../src/mods/tomba2_seamless_textures.inc"
extern "C" int tomba2_seamless_prepare(void);
extern "C" const unsigned char *tomba2_seamless_source(unsigned,unsigned);
extern "C" const unsigned char *tomba2_seamless_texture(unsigned,const unsigned char*,unsigned,unsigned*);
static std::ifstream disc;
static unsigned reads;
static bool available=true;
static std::string fingerprint="stock-test-plan";
namespace PSXRecompV4 {
const std::string &mod_runtime_fingerprint() { return fingerprint; }
}
extern "C" int psx_mod_read_disc_file(const char *path,void *buffer,uint32_t capacity,uint32_t *size) {
    ++reads;
    if(!available) return 0;
    for(const auto &c:catalog) if(!std::strcmp(c.path,path)) {
        if(capacity<c.size) return 0;
        for(unsigned offset=0;offset<c.size;offset+=2048) {
            unsigned char raw[2352];
            disc.seekg(uint64_t(c.lba+offset/2048)*2352);
            if(!disc.read(reinterpret_cast<char*>(raw),sizeof raw) || raw[15]!=2 || (raw[18]&0x20)) return 0;
            std::memcpy(static_cast<unsigned char*>(buffer)+offset,raw+24,std::min(2048u,c.size-offset));
        }
        *size=c.size; return 1;
    }
    return 0;
}
static void require(bool condition,const char *message) {
    if(!condition) {std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
static std::string hash(const unsigned char *p,size_t n) {
    unsigned char bytes[32];char hex[65];psx_sha256_compute(p,n,bytes);
    for(unsigned i=0;i<32;i++) std::snprintf(hex+2*i,3,"%02x",bytes[i]);
    return hex;
}
int main(int argc,char **argv) {
    if(argc!=3) {std::fprintf(stderr,"usage: test_seamless_prepare disc.bin empty-cache-directory\n");return 2;}
    const auto cache=std::filesystem::absolute(argv[2]);
    require(!std::filesystem::exists(cache),"test requires a fresh cache directory");
#ifdef _WIN32
    _putenv_s("TOMBA2_SEAMLESS_CACHE",cache.string().c_str());
#else
    setenv("TOMBA2_SEAMLESS_CACHE",cache.string().c_str(),1);
#endif
    disc.open(argv[1],std::ios::binary);require(bool(disc),"disc open");
    require(tomba2_seamless_prepare()==1 && reads==std::size(catalog),"cold native preparation");
    for(const auto &c:catalog) {
        const unsigned n=(c.size+2047)&~2047u;
        const auto *p=tomba2_seamless_source(c.lba,n);
        require(p && hash(p,n)==c.hash,"resident resource hash");
    }
    const auto *img=tomba2_seamless_source(6613,6289408);
    for(const auto &t:textures) {
        unsigned n=0;
        const auto *p=tomba2_seamless_texture(t.width,img+t.offset,t.size,&n);
        require(p && n==t.decoded && hash(p,n)==t.hash,"original MIPS texture oracle");
    }
    require(!tomba2_seamless_source(0xFFFFFFFFu,4),"invalid LBA rejected");
    require(!tomba2_seamless_source(374,0xFFFFFFFFu),"oversized request rejected");
    require(!tomba2_seamless_source(12771,2048),"XA remains outside resource cache");
    unsigned n=0;unsigned char bad[4]={1,2,3,4};
    require(!tomba2_seamless_texture(16,bad,4,&n),"unrecognized texture rejected");
    reads=0; available=false;
    require(tomba2_seamless_prepare()==1 && reads==0,"warm launch uses prepared bytes");
    std::filesystem::path pack;
    for(const auto &entry:std::filesystem::directory_iterator(cache)) pack=entry.path();
    fingerprint="changed-asset-plan";
    require(tomba2_seamless_prepare()==0 && !tomba2_seamless_source(374,4),
            "changed mod plan cannot reuse warm stock cache");
    fingerprint="stock-test-plan";
    require(tomba2_seamless_prepare()==1,"return to original mod plan");
    {std::fstream out(pack,std::ios::in|std::ios::out|std::ios::binary);out.seekp(31);out.put(0xAA);}
    available=true;reads=0;
    require(tomba2_seamless_prepare()==1 && reads==std::size(catalog),"corrupt resource rebuild");
    {std::ofstream out(pack,std::ios::binary|std::ios::trunc);out.write("T2RES002",8);}
    available=false;
    require(tomba2_seamless_prepare()==0 && !tomba2_seamless_source(374,4),"failed repair clears partial resident state");
    std::puts("PASS: cold/warm preparation, 34 resource hashes, 280 original-MIPS texture hashes, corruption recovery and fail-closed fallback");
}
