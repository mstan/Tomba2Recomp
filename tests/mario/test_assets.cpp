#include "smash_assets.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace tomba2::mario;
static void check(bool ok,const char* message) {
    if(!ok)throw std::runtime_error(message);
}
template<class F> static void rejects(F call) {
    bool rejected=false;try{call();}catch(const std::exception&){rejected=true;}
    check(rejected,"Invalid input accepted");
}
int main(int argc,char**argv) {
    try {
        rejects([]{decompress_vpk({},100);});
        rejects([]{read_cache({});});
        rejects([]{extract_reloc(Bytes(rom_size),296);});
        if(argc<2 || !argv[1][0]){std::cout<<"Malformed-input checks passed; owner cache unavailable\n";return 77;}
        std::ifstream stream(std::filesystem::u8path(argv[1]),std::ios::binary);
        Bytes cache{std::istreambuf_iterator<char>(stream),{}};
        auto files=read_cache(cache);auto model=decode_model(files);
        check(model.joints.size()==25,"Unexpected Mario joint count");
        check(model.triangles.size()==320,"Mario geometry lost/duplicated");
        auto atlas=make_texture_atlas(model);
        check(atlas.origins.size()==model.textures.size(),"Atlas lost a material");
        for(size_t i=0;i<model.textures.size();++i) {
            const auto& t=model.textures[i];auto origin=atlas.origins[i];
            for(unsigned y=0;y<t.height;++y)for(unsigned x=0;x<t.width;++x)
                check(atlas.texture.pixels[(origin[1]+y)*atlas.texture.width+origin[0]+x]==
                      t.pixels[y*t.width+x],"Atlas changed an imported texel");
            check(atlas.texture.pixels[(origin[1]-1)*atlas.texture.width+origin[0]-1]==t.pixels.front(),
                  "Atlas border did not replicate edge texels");
        }
        rejects([]{Model m;Texture t;t.width=256;t.height=1;t.pixels.resize(256);
                   m.textures.push_back(t);make_texture_atlas(m);});
        for(const auto&t:model.triangles)for(const auto&v:t.vertices)
            check(v.joint<25,"Seam vertex lost source bone");
        // Real source motions, including non-blocking cubic translation,
        // auxiliary roots and the distinct item/grab/throw streams.
        unsigned frames=0,motions=0;
        for(uint32_t id:{499,500,502,505,508,509,510,511,512,513,515,518,
                        523,538,541,561,562,563,564,571,572,573,575,588,597,
                        606,607,608,611,622,623,624,625,626}) {
            auto anim=decode_animation(id,files.at(id),model);
            ++motions;
            frames+=unsigned(anim.frames.size());
            for(const auto& frame:anim.frames) for(const auto&t:frame)
                check(std::isfinite(t.translation.x) && std::isfinite(t.rotation.y) &&
                      std::isfinite(t.scale.z),"Invalid sampled pose");
        }
        // Golden PCM from an independent bank/VADPCM decoder; no ROM data is
        // embedded. Covers all thirteen Mario voices and both predictor halves.
        const uint32_t voice_hashes[]={0x160b193e,0xd6b37216,0x69059dae,0x32b7a3cc,
            0x2dfc592c,0x79e30d56,0xfcf4a0e0,0xb31251b3,0x238e2a6c,0x237c1603,
            0x43734536,0xdee8cd48,0xd3fc4423};
        for(unsigned i=0;i<13;++i) {
            const auto& pcm=files.at(voice_cache_base+429+i);
            check(!pcm.empty() && pcm.size()%2==0,"Invalid voice PCM");
            uint32_t hash=2166136261u;
            for(uint8_t byte:pcm)hash=(hash^byte)*16777619u;
            check(hash==voice_hashes[i],"Mario voice decode changed");
        }
        Bytes bad=cache;bad.pop_back();rejects([&]{read_cache(bad);});
        bad=cache;bad.push_back(0);rejects([&]{read_cache(bad);});
        bad=cache;bad[12]=0;rejects([&]{read_cache(bad);});
        std::cout<<"PASS: owner model, "<<motions<<" motions ("<<frames<<" sampled frames), 13 voices, malformed caches\n";
        if(argc>2) {
            // Inspectable data for an independent software-rendered asset QA.
            std::ofstream out(std::filesystem::u8path(argv[2]));
            out<<"{\"joints\":[";
            auto pose=decode_animation(499,files.at(499),model).frames.at(0);
            for(size_t i=0;i<model.joints.size();++i) {
                const auto&t=pose[i];if(i)out<<',';
                out<<'['<<model.joints[i].parent<<','<<t.translation.x<<','<<t.translation.y<<','<<t.translation.z
                   <<','<<t.rotation.x<<','<<t.rotation.y<<','<<t.rotation.z<<','<<t.scale.x<<','<<t.scale.y<<','<<t.scale.z<<']';
            }
            out<<"],\"textures\":[";
            for(size_t i=0;i<model.textures.size();++i) {
                if(i)out<<',';const auto&t=model.textures[i];out<<'['<<t.width<<','<<t.height<<",[";
                for(size_t j=0;j<t.pixels.size();++j){if(j)out<<',';out<<t.pixels[j];}
                out<<"],"<<unsigned(t.mode_s)<<','<<unsigned(t.mode_t)<<']';
            }
            out<<"],\"triangles\":[";
            for(size_t i=0;i<model.triangles.size();++i) {
                const auto&t=model.triangles[i];if(i)out<<',';out<<'['<<t.texture;
                for(const auto&v:t.vertices)out<<",["<<v.joint<<','<<v.position.x<<','<<v.position.y<<','<<v.position.z<<','<<v.u<<','<<v.v<<']';
                out<<']';
            }out<<"]}\n";
        }
    }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
