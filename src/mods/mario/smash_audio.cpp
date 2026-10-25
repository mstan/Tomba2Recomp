#include "smash_assets.h"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace tomba2::mario {
namespace {
void check(bool ok) { if(!ok)throw std::runtime_error("Invalid Smash voice bank"); }
uint16_t u16(const Bytes& b,size_t p) {
    check(p<=b.size() && b.size()-p>=2);
    return uint16_t(b[p]<<8|b[p+1]);
}
uint32_t u32(const Bytes& b,size_t p) { return uint32_t(u16(b,p))<<16|u16(b,p+2); }
struct Wave {uint32_t base,length,book;};
}

std::map<uint32_t,Bytes> extract_voices(const Bytes& rom) {
    // US B1_sounds2 DMA ranges and FGM articulation IDs from ssb-decomp-re.
    check(rom.size()==rom_size);
    Bytes ctl(rom.begin()+0xc6b650,rom.begin()+0xc7b1f0);
    constexpr size_t table_base=0xc7b1f0,table_end=0xf573d0;
    std::map<uint32_t,Wave> waves;
    std::set<uint32_t> instruments,sounds;
    auto sound=[&](uint32_t p) {
        if(!p || !sounds.insert(p).second)return;
        uint32_t w=u32(ctl,p+8),base=u32(ctl,w),length=u32(ctl,w+4),book=u32(ctl,w+16);
        check(w+20<=ctl.size() && ctl[w+8]==0 && length && length<=table_end-table_base &&
              base<=table_end-table_base-length);
        waves.emplace(base,Wave{base,length,book});
    };
    auto instrument=[&](uint32_t p) {
        if(!p || !instruments.insert(p).second)return;
        unsigned count=u16(ctl,p+14);check(count<1024);
        for(unsigned i=0;i<count;++i)sound(u32(ctl,p+16+i*4));
    };
    unsigned banks=u16(ctl,2);check(banks && banks<=16);
    for(unsigned i=0;i<banks;++i) {
        uint32_t p=u32(ctl,4+i*4);unsigned count=u16(ctl,p);check(count<1024);
        instrument(u32(ctl,p+8));
        for(unsigned j=0;j<count;++j)instrument(u32(ctl,p+12+j*4));
    }
    check(waves.size()==322);
    std::vector<Wave> ordered;
    for(const auto& item:waves)ordered.push_back(item.second);
    std::map<uint32_t,Bytes> result;
    for(unsigned voice=429;voice<=441;++voice) {
        const Wave& w=ordered.at(173+voice-429);
        // Mario uses order-two/four-predictor Nintendo VADPCM books.
        check(u32(ctl,w.book)==2 && u32(ctl,w.book+4)==4);
        int16_t book[64];
        for(unsigned i=0;i<64;++i)book[i]=int16_t(u16(ctl,w.book+8+i*2));
        Bytes pcm;pcm.reserve(w.length/9*32);
        int32_t history[2]={};
        for(unsigned off=0;off+9<=w.length;off+=9) {
            size_t p=table_base+w.base+off;
            unsigned predictor=rom[p]&15,shift=std::min(12u,unsigned(rom[p]>>4));
            check(predictor<4);
            int32_t residual[16];
            for(unsigned i=0;i<16;++i) {
                int32_t nibble=(rom[p+1+i/2]>>(i%2?0:4))&15;
                residual[i]=(nibble>=8?nibble-16:nibble)*int32_t(1u<<shift);
            }
            for(unsigned half=0;half<2;++half) {
                int32_t decoded[8];const int16_t* c=book+predictor*16;
                for(unsigned i=0;i<8;++i) {
                    int64_t sum=int64_t(history[0])*c[i]+int64_t(history[1])*c[8+i];
                    for(unsigned j=0;j<i;++j)sum+=int64_t(residual[half*8+j])*c[8+i-1-j];
                    // Floor division matches the RSP arithmetic shift portably.
                    int64_t prediction=sum>=0?sum/2048:-((-sum+2047)/2048);
                    decoded[i]=int32_t(std::clamp<int64_t>(prediction+residual[half*8+i],-32768,32767));
                    pcm.push_back(uint8_t(decoded[i]));pcm.push_back(uint8_t(uint32_t(decoded[i])>>8));
                }
                history[0]=decoded[6];history[1]=decoded[7];
            }
        }
        result.emplace(voice,std::move(pcm));
    }
    return result;
}
} // namespace tomba2::mario
