#pragma once
#include <cstdint>

namespace tomba2::mario {
// US Smash FGM 429..441 selects tables 296..308. Their first pitch command
// (0x20, signed cents) lowers the raw waveform by approximately one octave.
// Output rate is not waveform playback rate. These integer rates apply those
// source pitches to the 32 kHz synthesizer and keep playback deterministic.
inline uint32_t voice_sample_rate(uint32_t voice) {
    constexpr uint32_t rates[]={16009,16280,16009,16009,16009,16009,16374,
                                16009,16009,16009,16009,16009,16009};
    return voice>=429 && voice<=441?rates[voice-429]:0;
}
inline uint32_t native_voice_replacement(uint8_t sound,uint8_t group) {
    // MAIN.EXE's common sound table marks Tomba's vocal bank entries with
    // group 1. In particular 0x2b is an ordinary effect, not a vocal cue.
    if(group!=1 || sound<0x1d || sound>0x37)return 0;
    switch(sound) {
    case 0x1d:return 435;                         // jump
    case 0x1e:return 439;case 0x1f:return 433;     // death / long fall
    case 0x23:case 0x24:case 0x25:return 440;     // hurt / heavy reactions
    case 0x26:case 0x27:return 438;               // lifting effort
    default:return 429+(sound%3);                // short effort variants
    }
}
}
