#include "smash_assets.h"
#include "presentation.h"
#include "action_animation.h"
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
        const float front_x[3]={0,1,0},front_y[3]={0,0,1};
        const float back_x[3]={0,0,1},back_y[3]={0,1,0};
        check(visible_face(2,front_x,front_y) && !visible_face(2,back_x,back_y),
              "Backface mode does not preserve source front faces");
        check(!visible_face(1,front_x,front_y) && visible_face(1,back_x,back_y),
              "Front-cull mode does not preserve source back faces");
        check(visible_face(0,front_x,front_y) && visible_face(0,back_x,back_y) &&
              !visible_face(3,front_x,front_y) && !visible_face(3,back_x,back_y),
              "Explicit two-sided/both-cull modes were lost");
        // Native RotMatrix's +X column at four quarter turns, independently
        // of the imported model's +Z bind-facing convention.
        const Vec3 native_forward[]={{1,0,0},{0,0,-1},{-1,0,0},{0,0,1}};
        for(unsigned h=0;h<4096;h+=1024) {
            Matrix world=mario_world(uint16_t(h),{});
            Vec3 a=transform(world,{}),b=transform(world,{0,0,1});
            check(std::abs(b.x-a.x-0.72f*native_forward[h/1024].x)<0.0001f &&
                  std::abs(b.z-a.z-0.72f*native_forward[h/1024].z)<0.0001f,
                  "Imported forward axis does not match native heading");
        }
        // The native costume presentation ends at yaw 0x400, facing the
        // camera. Imported +Z must point toward the viewer there, not away.
        Matrix costume=mario_world(1024,{});
        Vec3 origin=transform(costume,{}),face=transform(costume,{0,0,1});
        check(std::abs(face.x-origin.x)<0.0001f && face.z<origin.z,
              "Costume transformation shows the back of Mario's head");
        // Bucket pickup dialogue: native mode4/action30, heading1027. The
        // captured Q12 world matrix's +X column is {-19,0,-4096}. Mario must
        // face the same way without an action-specific half-turn exception.
        Matrix dialogue=mario_world(1027,{});
        Vec3 dialogue_origin=transform(dialogue,{}),dialogue_face=transform(dialogue,{0,0,1});
        check(std::abs(dialogue_face.x-dialogue_origin.x-0.72f*(-19.0f/4096))<0.001f &&
              std::abs(dialogue_face.z-dialogue_origin.z+0.72f)<0.001f,
              "Bucket pickup dialogue disagrees with the native render matrix");
        check(capture_animation(8)==520 && capture_animation(9)==520 &&
              capture_animation(2)==562 && capture_animation(6)==563 &&
              capture_animation(11,1)==509 && capture_animation(11,2)==513,
              "Chest perching uses a carry/throw pose or breaks ordinary capture");
        check(traversal_animation(12,0x41)==573 && traversal_animation(13,0x41)==576 &&
              traversal_animation(11,0x40)==572 && traversal_animation(2,2)==499 &&
              traversal_animation(7,0x63)==509,
              "Ladder catch/climb/exit and sign-turn phases are conflated");
        if(argc<2 || !argv[1][0]){std::cout<<"Malformed-input checks passed; owner cache unavailable\n";return 77;}
        std::ifstream stream(std::filesystem::u8path(argv[1]),std::ios::binary);
        Bytes cache{std::istreambuf_iterator<char>(stream),{}};
        auto files=read_cache(cache);auto model=decode_model(files);
        check(model.joints.size()==25,"Unexpected Mario joint count");
        check(model.triangles.size()==320,"Mario geometry lost/duplicated");
        unsigned back_culled=0;
        for(const auto& t:model.triangles) {check(t.cull<=3,"Invalid source cull mode");back_culled+=t.cull==2;}
        check(back_culled>0,"Source fighter backface mode was not retained");
        auto capture=decode_animation(562,files.at(562),model);
        auto hang=decode_animation(573,files.at(573),model);
        auto base=pose_bones(model,hang.frames[0]),reach_a=base,reach_b=base;
        climb_ladder(reach_a,1.570796327f);climb_ladder(reach_b,4.71238898f);
        for(auto segment:{std::array<unsigned,2>{3,5},{5,6},{9,11},{11,12},
                          {14,16},{16,17},{19,21},{21,22}}) {
            auto length=[&](const PoseBones& bones) {
                Vec3 a=transform(bones[segment[0]],{}),b=transform(bones[segment[1]],{});
                return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));
            };
            check(std::abs(length(base)-length(reach_a))<0.001f &&
                  std::abs(length(base)-length(reach_b))<0.001f,"Ladder motion stretches a limb");
        }
        for(unsigned tip:{6,12,17,22}) {
            Vec3 a=transform(reach_a[tip],{}),b=transform(reach_b[tip],{});
            check(std::abs(a.y-b.y)+std::abs(a.z-b.z)>10,
                  "Ladder hand or foot does not alternate between steps");
        }
        for(const auto& pose:capture.frames) {
            auto before=pose_bones(model,pose),raised=before;
            raise_capture_arms(raised);
            for(auto chain:{std::array<unsigned,2>{3,6},{9,12}}) {
                Vec3 root=transform(before[chain[0]],{}),hand=transform(before[chain[1]],{});
                Vec3 held=transform(raised[chain[1]],{});
                float length=std::sqrt((hand.x-root.x)*(hand.x-root.x)+
                    (hand.y-root.y)*(hand.y-root.y)+(hand.z-root.z)*(hand.z-root.z));
                check(std::abs(held.x-root.x)<0.001f && std::abs(held.z-root.z)<0.001f &&
                      std::abs(held.y-root.y-length)<0.001f,
                      "Overhead hold does not raise the imported hands without stretching");
            }
            // Legs/head/torso retain their imported pose, independent of arms.
            for(unsigned i:{0,1,2,7,8,14,15,16,17,18,19,20,21,22,23,24})
                for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                    check(before[i].m[r][c]==raised[i].m[r][c],"Capture pose changed a non-arm bone");
        }
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
        for(uint32_t id:{499,500,502,505,508,509,510,511,512,513,515,518,520,
                        523,538,541,561,562,563,564,571,572,573,575,576,588,597,
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
            uint32_t inspect_id=argc>3?uint32_t(std::stoul(argv[3])):499;
            auto inspect=decode_animation(inspect_id,files.at(inspect_id),model);
            unsigned inspect_frame=argc>4?unsigned(std::stoul(argv[4])):0;
            auto pose=inspect.frames.at(inspect_frame);
            std::cout<<"Exported motion "<<inspect_id<<" frame "<<inspect_frame<<" / "<<inspect.frames.size()<<'\n';
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
