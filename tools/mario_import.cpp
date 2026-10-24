#include "smash_assets.h"
#include "psx_sha256.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace tomba2::mario;
static Bytes read(const std::filesystem::path& p) {
    std::ifstream in(p,std::ios::binary|std::ios::ate);
    if(!in || in.tellg()<0 || in.tellg()>32*1024*1024)
        throw std::runtime_error("Cannot read input, or input exceeds 32 MiB");
    Bytes b(size_t(in.tellg()));in.seekg(0);
    if(!in.read(reinterpret_cast<char*>(b.data()),std::streamsize(b.size())))
        throw std::runtime_error("Incomplete input read");
    return b;
}
static std::string hash(const Bytes& b) {
    uint8_t h[32];psx_sha256_compute(b.data(),b.size(),h);
    std::ostringstream s;
    for(uint8_t c:h)s<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(c);
    return s.str();
}
int main(int argc,char** argv) {
    try {
        if(argc!=3) throw std::runtime_error("Usage: mario-import <Smash 64 US ROM> <new cache file>");
        Bytes rom=read(std::filesystem::u8path(argv[1]));
        if(rom.size()!=rom_size)throw std::runtime_error("Expected a 16 MiB Smash 64 US 1.0 ROM");
        if(rom[0]==0x37 && rom[1]==0x80)
            for(size_t i=0;i<rom.size();i+=2)std::swap(rom[i],rom[i+1]);
        else if(rom[0]==0x40 && rom[1]==0x12)
            for(size_t i=0;i<rom.size();i+=4){std::swap(rom[i],rom[i+3]);std::swap(rom[i+1],rom[i+2]);}
        if(hash(rom)!=rom_sha256)throw std::runtime_error("Unsupported or modified Smash 64 ROM");
        Bytes cache=build_cache(rom);
        auto files=read_cache(cache);auto model=decode_model(files);
        auto out=std::filesystem::u8path(argv[2]);
        if(std::filesystem::exists(out))throw std::runtime_error("Output already exists");
        std::ofstream stream(out,std::ios::binary);
        if(!stream.write(reinterpret_cast<const char*>(cache.data()),std::streamsize(cache.size())))
            throw std::runtime_error("Cannot write output cache");
        std::cout<<"cache_sha256="<<hash(cache)<<"\ncache_bytes="<<cache.size()
                 <<"\nfiles="<<files.size()<<"\njoints="<<model.joints.size()
                 <<"\ntriangles="<<model.triangles.size()<<"\ntextures="<<model.textures.size()<<"\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
