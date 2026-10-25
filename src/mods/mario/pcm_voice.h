#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace tomba2::mario {
struct VoicePosition {uint32_t cursor=0,fraction=0;};
struct VoiceMix {uint32_t frames=0;bool finished=false;};

// Source-rate signed little-endian mono -> 44.1 kHz stereo. All mutable state is
// explicit so guest-memory save/load, rewind and rollback resume identically.
inline VoiceMix mix_pcm_voice(const uint8_t* pcm,size_t bytes,int16_t* stereo,
                              uint32_t frames,VoicePosition& position,uint32_t source_rate=32000) {
    if(!pcm || bytes%2 || bytes/2>std::numeric_limits<uint32_t>::max() ||
       !source_rate || source_rate>192000 || position.fraction>=44100 ||
       position.cursor>=bytes/2)return {0,true};
    if(!stereo || !frames)return {};
    uint32_t count=uint32_t(bytes/2),mixed=0;
    auto sample=[&](uint32_t i) {
        size_t offset=size_t(i)*2;
        return int32_t(int16_t(uint16_t(pcm[offset])|uint16_t(pcm[offset+1])<<8));
    };
    for(uint32_t f=0;f<frames && position.cursor<count;++f) {
        int32_t a=sample(position.cursor),b=sample(std::min(position.cursor+1,count-1));
        int32_t value=a+int32_t(int64_t(b-a)*position.fraction/44100);
        uint32_t fade=std::min({128u,position.cursor,count-1-position.cursor});
        value=int32_t(int64_t(value)*fade*3/(128*4));
        for(unsigned ch=0;ch<2;++ch)
            stereo[f*2+ch]=int16_t(std::clamp(int32_t(stereo[f*2+ch])+value,-32768,32767));
        position.fraction+=source_rate;
        position.cursor+=position.fraction/44100;position.fraction%=44100;
        ++mixed;
    }
    return {mixed,position.cursor>=count};
}
}
