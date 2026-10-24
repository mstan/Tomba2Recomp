#include "pcm_voice.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace tomba2::mario;
static void check(bool ok,const char* message) {
    if(!ok)throw std::runtime_error(message);
}
int main() {
    try {
        std::vector<uint8_t> pcm(1024);
        for(unsigned i=0;i<512;++i) {
            uint16_t value=uint16_t(int16_t(int(i*127%65536)-32768));
            pcm[i*2]=uint8_t(value);pcm[i*2+1]=uint8_t(value>>8);
        }
        std::vector<int16_t> whole(2048,5000),chunks=whole;
        VoicePosition a,b;
        auto result=mix_pcm_voice(pcm.data(),pcm.size(),whole.data(),1024,a);
        unsigned offset=0,total=0;
        for(unsigned n:{17u,113u,1u,227u,311u,355u}) {
            total+=mix_pcm_voice(pcm.data(),pcm.size(),chunks.data()+offset*2,n,b).frames;
            offset+=n;
        }
        check(whole==chunks && a.cursor==b.cursor && a.fraction==b.fraction,
              "PCM depends on callback block size");
        check(result.finished && total==result.frames && total>512 && total<1024,
              "Voice failed to finish at its resampled length");
        VoicePosition current;std::vector<int16_t> prefix(114);
        mix_pcm_voice(pcm.data(),pcm.size(),prefix.data(),57,current);
        auto saved=current;
        std::vector<int16_t> first(400),replayed(400);
        mix_pcm_voice(pcm.data(),pcm.size(),first.data(),200,current);
        mix_pcm_voice(pcm.data(),pcm.size(),replayed.data(),200,saved);
        check(first==replayed && current.cursor==saved.cursor && current.fraction==saved.fraction,
              "Restoring a mid-voice cursor changed the PCM suffix");
        std::vector<int16_t> untouched(64,1234),bad=untouched;
        VoicePosition invalid{0xffffffffu,0};
        check(mix_pcm_voice(pcm.data(),pcm.size(),bad.data(),32,invalid).finished && bad==untouched,
              "Invalid restored cursor touched output");
        invalid={0,44100};
        check(mix_pcm_voice(pcm.data(),pcm.size(),bad.data(),32,invalid).finished && bad==untouched,
              "Invalid restored fraction touched output");
        invalid={};
        check(mix_pcm_voice(pcm.data(),1,bad.data(),32,invalid).finished && bad==untouched,
              "Odd PCM input accepted");
        std::vector<uint8_t> loud(1024,0xff);
        for(unsigned i=0;i<512;++i)loud[i*2+1]=0x7f;
        std::vector<int16_t> clipped(2,32000);VoicePosition middle{200,0};
        mix_pcm_voice(loud.data(),loud.size(),clipped.data(),1,middle);
        check(clipped[0]==32767 && clipped[1]==32767,"Voice overflow wrapped instead of saturating");
        std::cout<<"PASS: PCM block-size independence, mid-voice restore, completion, invalid state, saturation\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
