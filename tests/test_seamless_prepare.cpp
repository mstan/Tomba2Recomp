/* Integration test using an owner-supplied disc, through the framework's
 * resident pack (mod_resident.cpp) with its disc and plan services replaced
 * by readers of the real image. No disc data is distributed. The decoder
 * catalog was verified separately with the original MIPS routine. */
#include "mod_runtime.h"
#include "psx_sha256.h"
#include <algorithm>
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
static bool patch_img=false;
static std::string fingerprint="stock-test-plan";
static const CatalogEntry *entry(const char *path) {
    for(const auto &c:catalog) if(!std::strcmp(c.path,path)) return &c;
    return nullptr;
}
namespace PSXRecompV4 {
const std::string &mod_runtime_fingerprint() { return fingerprint; }
bool mod_runtime_read_disc_file_sectors(const std::string &path,uint32_t,std::vector<uint8_t> &padded,
                                        uint32_t &lba,uint32_t &size,std::string *error) {
    ++reads;
    padded.clear();
    const CatalogEntry *c=entry(path.c_str());
    if(!available || !c) { if(error) *error=path+": unavailable"; return false; }
    padded.resize((c->size+2047u)&~2047u);
    for(unsigned s=0;s<padded.size()/2048;s++) {
        unsigned char raw[2352];
        disc.clear();
        disc.seekg(uint64_t(c->lba+s)*2352);
        if(!disc.read(reinterpret_cast<char*>(raw),sizeof raw) || raw[15]!=2 || (raw[18]&0x20)) return false;
        std::memcpy(padded.data()+s*2048u,raw+24,2048);
    }
    if(patch_img && path=="CD/TOMBA2.IMG") padded[textures[0].offset+3]^=0x5A;
    lba=c->lba; size=c->size;
    return true;
}
}
extern "C" int psx_mod_disc_file_extent(const char *path,uint32_t *lba,uint32_t *size) {
    const CatalogEntry *c=entry(path);
    if(!c) return 0;
    *lba=c->lba; *size=c->size;
    return 1;
}
extern "C" void psx_mod_counter_add(const char *,uint32_t) {}
extern "C" uint8_t psx_mod_read_byte(uint32_t) { return 0; }
static void require(bool condition,const char *message) {
    if(!condition) {std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
static std::string hash(const unsigned char *p,size_t n) {
    unsigned char bytes[32];char hex[65];psx_sha256_compute(p,n,bytes);
    for(unsigned i=0;i<32;i++) std::snprintf(hex+2*i,3,"%02x",bytes[i]);
    return hex;
}
int main(int argc,char **argv) {
    if(argc!=3) {std::fprintf(stderr,"usage: test_seamless_prepare disc.bin scratch-directory\n");return 2;}
    const auto cache=std::filesystem::absolute(argv[2])/
        ("run-"+std::to_string(std::filesystem::file_time_type::clock::now().time_since_epoch().count()));
    require(!std::filesystem::exists(cache),"test requires a fresh cache directory");
#ifdef _WIN32
    _putenv_s("PSX_RESIDENT_CACHE",cache.string().c_str());
#else
    setenv("PSX_RESIDENT_CACHE",cache.string().c_str(),1);
#endif
    disc.open(argv[1],std::ios::binary);require(bool(disc),"disc open");
    require(tomba2_seamless_prepare()==1 && reads==std::size(catalog),"cold native preparation");
    for(const auto &c:catalog) {
        const unsigned n=(c.size+2047)&~2047u;
        const auto *p=tomba2_seamless_source(c.lba,n);
        require(p && hash(p,n)==c.hash,"resident resource hash");
    }
    const auto *img=tomba2_seamless_source(6613,6289408);
    require(img!=nullptr,"texture container resident");
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
    for(const auto &e:std::filesystem::recursive_directory_iterator(cache))
        if(e.path().extension()==".pack") pack=e.path();
    require(!pack.empty(),"pack published");
    fingerprint="changed-asset-plan";
    require(tomba2_seamless_prepare()==0 && !tomba2_seamless_source(374,4),
            "changed mod plan cannot reuse warm stock cache");
    /* A modded TOMBA2.IMG: served as is, textures left to the game's decoder. */
    available=true; patch_img=true;
    require(tomba2_seamless_prepare()==1,"modded plan prepared from the effective disc");
    img=tomba2_seamless_source(6613,6289408);
    require(img && img[textures[0].offset+3]!=0,"modified container served");
    require(!tomba2_seamless_texture(textures[1].width,img+textures[1].offset,textures[1].size,&n),
            "no prepared textures for a modified container");
    fingerprint="stock-test-plan"; patch_img=false; available=false;
    require(tomba2_seamless_prepare()==1,"return to original mod plan");
    {std::fstream out(pack,std::ios::in|std::ios::out|std::ios::binary);out.seekp(4096+31);out.put(char(0xAA));}
    available=true;reads=0;
    require(tomba2_seamless_prepare()==1 && reads==std::size(catalog),"corrupt resource rebuild");
    {std::ofstream out(pack,std::ios::binary|std::ios::trunc);out.write("PSXRES01",8);}
    available=false;
    require(tomba2_seamless_prepare()==0 && !tomba2_seamless_source(374,4),"failed repair clears partial resident state");
    std::error_code ec;
    std::filesystem::remove_all(cache,ec);
    std::printf("PASS: cold/warm preparation, %zu resource hashes, %zu original-MIPS texture hashes, "
                "modded container, corruption recovery and fail-closed fallback\n",
                std::size(catalog),std::size(textures));
}
