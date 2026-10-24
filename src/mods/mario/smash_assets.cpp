#include "smash_assets.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <tuple>

namespace tomba2::mario {
namespace {
[[noreturn]] void invalid(const char* what) { throw std::runtime_error(what); }
void span(const Bytes& b, size_t p, size_t n) {
    if (p > b.size() || n > b.size() - p) invalid("Truncated Smash asset");
}
uint16_t be16(const Bytes& b, size_t p) {
    span(b,p,2); return uint16_t(b[p] << 8 | b[p+1]);
}
uint32_t be32(const Bytes& b, size_t p) {
    return uint32_t(be16(b,p)) << 16 | be16(b,p+2);
}
float bef(const Bytes& b, size_t p) {
    uint32_t bits = be32(b,p); float f; std::memcpy(&f,&bits,4);
    if (!std::isfinite(f)) invalid("Non-finite Smash transform");
    return f;
}
size_t pointer(const Bytes& b, size_t p) {
    uint32_t v = be32(b,p);
    if (!v) return 0;
    size_t result = (v & 0xffffu) * 4u;
    span(b,result,1); return result;
}
void le32(Bytes& b, uint32_t v) {
    for (unsigned s = 0; s < 32; s += 8) b.push_back(uint8_t(v >> s));
}
uint32_t read_le32(const Bytes& b, size_t p) {
    span(b,p,4); return uint32_t(b[p]) | uint32_t(b[p+1])<<8 |
        uint32_t(b[p+2])<<16 | uint32_t(b[p+3])<<24;
}
struct Bits {
    const Bytes& b; size_t bit = 0;
    uint32_t get(unsigned n) {
        if (n > 32 || bit > b.size()*8 || n > b.size()*8-bit)
            invalid("Truncated VPK bitstream");
        uint32_t v = 0;
        while(n--) { v = (v<<1) | ((b[bit/8] >> (7-bit%8))&1); ++bit; }
        return v;
    }
};
struct Tree {
    struct Node { int left=-1,right=-1; unsigned value=0; };
    std::vector<Node> nodes; int root=-1;
    explicit Tree(Bits& bits) {
        std::vector<int> stack;
        for (unsigned count=0; count<511; ++count) {
            bool branch=bits.get(1)!=0;
            if (branch && stack.size()==1) { root=stack.back(); return; }
            Node node;
            if (branch) {
                if (stack.size()<2) invalid("Invalid VPK Huffman tree");
                node.right=stack.back(); stack.pop_back();
                node.left=stack.back(); stack.pop_back();
            } else {
                node.value=bits.get(8);
                if(node.value>32) invalid("Invalid VPK code width");
            }
            stack.push_back(int(nodes.size())); nodes.push_back(node);
        }
        invalid("VPK Huffman tree exceeds limit");
    }
    unsigned get(Bits& bits) const {
        int index=root;
        for (unsigned depth=0; depth<256; ++depth) {
            const Node& n=nodes.at(size_t(index));
            if(n.left<0) return n.value;
            index=bits.get(1)?n.right:n.left;
        }
        invalid("VPK Huffman tree exceeds depth limit");
    }
};
uint16_t rgba5551(uint16_t n64) {
    // N64 is R5 G5 B5 A1; PS1 is STP B5 G5 R5.
    if (!(n64&1)) return 0;
    return uint16_t(0x8000 | (n64>>11) | ((n64>>6)&31)<<5 | ((n64>>1)&31)<<10);
}
uint16_t color(uint32_t rgba) {
    return uint16_t(0x8000 | ((rgba>>27)&31) |
                    ((rgba>>19)&31)<<5 | ((rgba>>11)&31)<<10);
}
}

Bytes decompress_vpk(const Bytes& input, uint32_t expected_size) {
    if(input.size()<9 || std::memcmp(input.data(),"vpk0",4) ||
       be32(input,4)!=expected_size || expected_size>4*1024*1024)
        invalid("Invalid VPK header/size");
    Bits bits{input,64}; unsigned method=bits.get(8);
    if(method>1) invalid("Unsupported VPK method");
    Tree offsets(bits), lengths(bits);
    Bytes result; result.reserve(expected_size);
    while(result.size()<expected_size) {
        if(!bits.get(1)) { result.push_back(uint8_t(bits.get(8))); continue; }
        uint32_t value=bits.get(offsets.get(bits));
        int64_t distance=value;
        if(method) {
            uint32_t sub=0;
            if(value<=2) { sub=value+1; value=bits.get(offsets.get(bits)); }
            distance=int64_t(value)*4+sub-8;
        }
        uint32_t length=bits.get(lengths.get(bits));
        if(distance<=0 || uint64_t(distance)>result.size() || !length ||
           length>expected_size-result.size()) invalid("Invalid VPK back-reference");
        while(length--) result.push_back(result[result.size()-size_t(distance)]);
    }
    return result;
}

Bytes extract_reloc(const Bytes& rom, uint32_t id) {
    constexpr size_t table=0x1ac870, count=2132, start=table+(count+1)*12;
    if(rom.size()!=rom_size || be32(rom,0)!=0x80371240 || id>=count)
        invalid("Expected canonical Smash 64 US ROM");
    size_t e=table+id*12, p=start+(be32(rom,e)&0x7fffffff);
    size_t packed=be16(rom,e+6)*4u, unpacked=be16(rom,e+10)*4u;
    bool compressed=(be32(rom,e)&0x80000000u)!=0;
    span(rom,p,compressed?packed:unpacked);
    Bytes data(rom.begin()+p,rom.begin()+p+(compressed?packed:unpacked));
    return compressed?decompress_vpk(data,uint32_t(unpacked)):data;
}

std::vector<uint32_t> required_files() {
    std::vector<uint32_t> ids{296,299};
    // Common actions, item carry/throw, jab/kick, aerials and entry poses.
    for(uint32_t i=499;i<=598;++i) ids.push_back(i);
    for(uint32_t i=606;i<=634;++i) ids.push_back(i);
    ids.push_back(640);
    for(uint32_t voice=429;voice<=441;++voice)ids.push_back(voice_cache_base+voice);
    return ids;
}
Bytes build_cache(const Bytes& rom) {
    Bytes result{'M','A','R','I','O','R','0','2'};
    auto voices=extract_voices(rom);
    auto ids=required_files(); le32(result,uint32_t(ids.size()));
    for(uint32_t id:ids) {
        Bytes file=id>=voice_cache_base?voices.at(id-voice_cache_base):extract_reloc(rom,id);
        le32(result,id); le32(result,uint32_t(file.size()));
        result.insert(result.end(),file.begin(),file.end());
    }
    return result;
}
std::map<uint32_t,Bytes> read_cache(const Bytes& cache) {
    span(cache,0,12);
    if(std::memcmp(cache.data(),"MARIOR02",8)) invalid("Wrong Mario cache version");
    auto ids=required_files();
    if(read_le32(cache,8)!=ids.size()) invalid("Wrong Mario cache file count");
    std::map<uint32_t,Bytes> files; size_t p=12;
    for(uint32_t expected:ids) {
        uint32_t id=read_le32(cache,p),size=read_le32(cache,p+4);p+=8;
        if(id!=expected || size>4*1024*1024 || !size) invalid("Invalid Mario cache entry");
        span(cache,p,size); files.emplace(id,Bytes(cache.begin()+p,cache.begin()+p+size)); p+=size;
    }
    if(p!=cache.size()) invalid("Trailing Mario cache data");
    return files;
}

namespace {
// Material animation: first costume's primary colour is a packed RGBA track.
uint32_t material_color(const Bytes& b,size_t script,uint32_t initial,unsigned wanted) {
    if(!script) return initial;
    size_t p=script;
    for(unsigned n=0;n<256;++n) {
        uint32_t cmd=be32(b,p);p+=4;
        unsigned op=cmd>>25, flags=(cmd>>15)&0x3ff;
        if(!op) return initial;
        if(op==1) continue;
        if(op==13) {p+=4;continue;}
        unsigned words=0;
        switch(op) {
        case 2:case 3:case 7:case 8:case 9:case 10:case 11: words=1;break;
        case 4:case 5: words=2;break;
        case 6:words=1;break;
        case 18:case 19:case 20:case 21:
            for(unsigned track=0;track<5;++track) if(flags&(1u<<track)) {
                uint32_t value=be32(b,p);p+=4;
                if(track==wanted) return value;
            }
            continue;
        default: throw std::runtime_error("Unsupported Mario material animation opcode " +
            std::to_string(op) + " at " + std::to_string(p-4));
        }
        for(unsigned i=0;i<10;++i) if(flags&(1u<<i)) p+=words*4;
        span(b,p,0);
    }
    invalid("Unbounded Mario material animation");
}
struct Material { size_t offset=0,palette=0; uint32_t primary=0xffffffff; };

class ModelDecoder {
    const Bytes& b;
    Model result;
    std::map<std::tuple<size_t,size_t,size_t,uint32_t>,uint16_t> textures;
    std::array<Vertex,32> vertices{};
    std::array<bool,32> loaded{};
    std::vector<Material> materials;
    size_t image=0,loaded_palette=0;
    unsigned format=0,size=0,width=1,height=1,material=0;
    uint32_t shade=0xffffffff;
    float scale_s=1,scale_t=1,tile_s=0,tile_t=0;
    unsigned tile_width=1,tile_height=1;
    uint8_t mode_s=2,mode_t=2;
    bool texture_enabled=false;

    uint16_t texture() {
        const Material& m=materials.at(material);
        size_t palette=m.palette?m.palette:loaded_palette;
        auto key=std::make_tuple(m.offset,image,palette,shade);
        auto known=textures.find(key); if(known!=textures.end()) return known->second;
        Texture t;
        if(texture_enabled && m.offset && (be16(b,m.offset+0x30)&5) && image && palette) {
            t.mode_s=mode_s;t.mode_t=mode_t;
            // MObjSub's image dimensions, not the UV tile window.
            t.width=be16(b,m.offset+0x34); t.height=be16(b,m.offset+0x36);
            if(!t.width || !t.height || t.width>256 || t.height>256)
                invalid("Unsupported Mario texture dimensions");
            unsigned pixel_size=b.at(m.offset+0x33);
            unsigned pixel_format=b.at(m.offset+0x32);
            if(pixel_format!=2 || pixel_size!=0) invalid("Expected Mario CI4 texture");
            size_t count=size_t(t.width)*t.height;span(b,image,(count+1)/2);
            for(size_t i=0;i<count;++i) {
                uint8_t index=uint8_t((b[image+i/2]>>(i%2?0:4))&15);
                t.pixels.push_back(rgba5551(be16(b,palette+index*2)));
            }
            // RDP clamps to the tile window, then applies its mask/mirror.
            // Mario's 64-texel windows deliberately mirror 32-texel images.
            // Bake that small window so PS1 texture packets retain the seam.
            unsigned w=std::min(tile_width,256u),h=std::min(tile_height,256u);
            if(w && h) {
                auto address=[](unsigned v,unsigned extent,uint8_t mode) {
                    if(mode&1) {v%=extent*2;return v>=extent?extent*2-1-v:v;}
                    return (mode&2)?std::min(v,extent-1):v%extent;
                };
                std::vector<uint16_t> expanded(w*h);
                for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)
                    expanded[y*w+x]=t.pixels[address(y,t.height,mode_t)*t.width+address(x,t.width,mode_s)];
                t.width=uint16_t(w);t.height=uint16_t(h);t.pixels=std::move(expanded);
                if(mode_s&2)t.mode_s=2;
                if(mode_t&2)t.mode_t=2;
            }
        } else t.pixels.push_back(color(shade));
        if(result.textures.size()>=65535) invalid("Too many Mario textures");
        uint16_t index=uint16_t(result.textures.size());
        result.textures.push_back(std::move(t));textures[key]=index;return index;
    }
    void triangle(unsigned joint,uint32_t word) {
        Triangle t; t.joint=uint16_t(joint); t.texture=texture();
        for(unsigned i=0;i<3;++i) {
            unsigned index=((word>>(16-i*8))&255)/2;
            if(index>=32 || !loaded[index]) invalid("Mario triangle references unloaded vertex");
            t.vertices[i]=vertices[index];
            t.vertices[i].u-=tile_s;t.vertices[i].v-=tile_t;
        }
        result.triangles.push_back(t);
        if(result.triangles.size()>4096) invalid("Mario model triangle limit exceeded");
    }
    void display_list(unsigned joint,size_t p,unsigned depth=0) {
        if(depth>8) invalid("Mario display-list recursion limit");
        for(unsigned commands=0;commands<4096;++commands,p+=8) {
            uint32_t a=be32(b,p),c=be32(b,p+4);unsigned op=a>>24;
            switch(op) {
            case 0xdf:return;
            case 0xde:
                if((c>>24)==0x0e) {
                    material=(c&0xffffff)/8;
                    if(material>=materials.size()) invalid("Invalid Mario material reference");
                    shade=materials[material].primary;
                    if(materials[material].offset) {
                        size_t sprites=pointer(b,materials[material].offset+4);
                        if(sprites)image=pointer(b,sprites);
                    }
                } else display_list(joint,pointer(b,p+4),depth+1);
                if(a&0x10000) return;
                break;
            case 0x01: {
                unsigned n=(a>>12)&255,end=(a>>1)&127;
                if(!n || end<n || end>32) invalid("Invalid Mario vertex run");
                size_t v=pointer(b,p+4);span(b,v,n*16);
                for(unsigned i=0;i<n;++i) {
                    Vertex vertex;size_t q=v+i*16;
                    vertex.joint=uint16_t(joint);
                    vertex.position={float(int16_t(be16(b,q))),float(int16_t(be16(b,q+2))),float(int16_t(be16(b,q+4)))};
                    vertex.u=float(int16_t(be16(b,q+8)))/32*scale_s;
                    vertex.v=float(int16_t(be16(b,q+10)))/32*scale_t;
                    vertex.normal={float(int8_t(b[q+12]))/127,float(int8_t(b[q+13]))/127,float(int8_t(b[q+14]))/127};
                    vertices[end-n+i]=vertex;loaded[end-n+i]=true;
                } break;
            }
            case 0x02: {
                unsigned index=(a&0xffff)/2,field=(a>>16)&255;
                if(index>=32 || !loaded[index]) invalid("Mario modify-vertex references unloaded vertex");
                if(field==0x14) {
                    vertices[index].u=float(int16_t(c>>16))/32;
                    vertices[index].v=float(int16_t(c))/32;
                } else invalid("Unsupported Mario modify-vertex field");
                break;
            }
            case 0x05:triangle(joint,a);break;
            case 0x06:triangle(joint,a);triangle(joint,c);break;
            case 0xfd:image=pointer(b,p+4);format=(a>>21)&7;size=(a>>19)&3;break;
            case 0xfa:shade=c;break;
            case 0xf0:loaded_palette=image;break;
            case 0xd7:
                texture_enabled=(a&2)!=0;
                scale_s=float(c>>16)/65536;scale_t=float(c&65535)/65536;break;
            case 0xf2:
                if(((c>>24)&7)==0) {
                    tile_s=float((a>>12)&4095)/4;tile_t=float(a&4095)/4;
                    tile_width=unsigned(float((c>>12)&4095)/4-tile_s+1);
                    tile_height=unsigned(float(c&4095)/4-tile_t+1);
                }
                break;
            case 0xf5:
                if(((c>>24)&7)==0) {mode_s=uint8_t((c>>8)&3);mode_t=uint8_t((c>>18)&3);}
                break;
            // RSP/RDP state changes affect stock lighting/tile filtering;
            // geometry, material identity and UVs are preserved above.
            case 0xd9:case 0xdb:case 0xe2:case 0xe3:case 0xe6:
            case 0xe7:case 0xe8:case 0xf3:
            case 0xf9:case 0xfc:break;
            default:throw std::runtime_error("Unsupported Mario display-list opcode " +
                std::to_string(op) + " at " + std::to_string(p));
            }
        }
        invalid("Unterminated Mario display list");
    }
public:
    explicit ModelDecoder(const Bytes& model):b(model) {}
    Model run() {
        std::array<int,32> parents;parents.fill(-1);
        for(unsigned i=0;i<25;++i) {
            size_t p=0x2200+i*44;unsigned depth=be32(b,p)&0xfff;
            if(depth>=32 || (depth && parents[depth-1]<0)) invalid("Invalid Mario joint tree");
            Joint j;j.parent=depth?parents[depth-1]:-1;
            j.bind.translation={bef(b,p+8),bef(b,p+12),bef(b,p+16)};
            j.bind.rotation={bef(b,p+20),bef(b,p+24),bef(b,p+28)};
            j.bind.scale={bef(b,p+32),bef(b,p+36),bef(b,p+40)};
            parents[depth]=int(i); result.joints.push_back(j);
        }
        for(unsigned i=0;i<25;++i) {
            size_t dl=pointer(b,0x2200+i*44+4); if(!dl) continue;
            size_t list=pointer(b,i*4),anims=pointer(b,0x2680+i*4);
            materials.clear();
            // Glove display lists use the RSP's white lights directly and have
            // no MObjSub; the fighter's model-parts table selects their mesh.
            if(!list) materials.push_back({0,0,0xffffffff});
            for(unsigned n=0;list && n<8;++n) {
                size_t m=pointer(b,list+n*4);if(!m)break;
                size_t pal_table=pointer(b,m+0x2c);
                size_t pal=pal_table?pointer(b,pal_table):0;
                uint32_t rgba=be32(b,m+0x50);
                size_t script=anims?pointer(b,anims+n*4):0;
                rgba=material_color(b,script,rgba,0);
                if(be16(b,m+0x30)&0x1000)
                    rgba=material_color(b,script,be32(b,m+0x60),3)|255u;
                materials.push_back({m,pal,rgba});
            }
            if(materials.empty()) invalid("Empty Mario material list");
            image=0;material=0;shade=materials[0].primary;display_list(i,dl);
        }
        if(result.triangles.empty()) invalid("Empty Mario mesh");
        // Cap portion of Mario's original 8x10 stock icon. The source bitmap
        // has a 16-texel pitch and RDP's odd-row word swap (SP_TEXSHUF).
        result.hat_icon.width=8;result.hat_icon.height=4;
        for(unsigned y=0;y<4;++y)for(unsigned x=0;x<8;++x) {
            size_t p=0x71a8+y*8+((x/2)^((y&1)?4:0));span(b,p,1);
            unsigned index=(b[p]>>(x&1?0:4))&15;
            result.hat_icon.pixels.push_back(rgba5551(be16(b,0x7200+index*2)));
        }
        return std::move(result);
    }
};
}
Model decode_model(const std::map<uint32_t,Bytes>& files) {
    return ModelDecoder(files.at(296)).run();
}

namespace {
struct MotionRecipe { uint32_t id,slots; bool auxiliary,wide; };
#include "motion_recipe.h"
struct Segment { float at,duration,base,target,rate0,rate1; unsigned kind; };
using Tracks=std::array<std::vector<Segment>,10>;
float units(unsigned track,int16_t value,bool rate=false) {
    float divisor=track<3?512.0f:track==3?16384.0f:
                  track<7?(rate?32.0f:4.0f):(rate?8192.0f:4096.0f);
    return value/divisor;
}
Tracks figatree(const Bytes& b,size_t p,float& length) {
    Tracks tracks;
    std::array<float,10> target{},rate{};
    std::array<bool,10> initialized{};
    float clock=0;
    for(unsigned commands=0;commands<4096;++commands) {
        unsigned c=be16(b,p);p+=2;
        unsigned op=c>>11,flags=(c>>1)&1023;
        if(op==0 || op==13) {
            for(unsigned t=0;t<10;++t) if(initialized[t] && tracks[t].empty())
                tracks[t].push_back({0,0,target[t],target[t],0,0,0});
            length=std::max(length,clock);return tracks;
        }
        float duration=(c&1)?float(be16(b,p)):0;
        if(c&1)p+=2;
        if(op==1 || op==14) {clock+=duration;continue;}
        if(op==11) {
            for(unsigned t=0;t<10;++t) if((flags&(1u<<t)) && !tracks[t].empty())
                tracks[t].back().at-=duration;
            continue;
        }
        if(op<2 || op>10) throw std::runtime_error("Unsupported Mario Figatree command " +
            std::to_string(op) + " at " + std::to_string(p-2));
        for(unsigned t=0;t<10;++t) if(flags&(1u<<t)) {
            int16_t raw=int16_t(be16(b,p));p+=2;
            if(op==6) {rate[t]=units(t,raw,true);continue;}
            float value=units(t,raw),next_rate=0;
            unsigned kind=op==9 || op==10?3:op==4 || op==5 || op==7 || op==8?2:1;
            if(op==4 || op==5) {next_rate=units(t,int16_t(be16(b,p)),true);p+=2;}
            initialized[t]=true;
            // Zero-time initializers establish the target for subsequent curves.
            // A later initializer must also supersede an older timed segment.
            tracks[t].push_back({clock,duration,target[t],value,rate[t],next_rate,kind});
            target[t]=value;rate[t]=next_rate;
        }
        if(op==2 || op==4 || op==7 || op==9)clock+=duration;
        if(clock>4096) invalid("Mario animation exceeds duration limit");
    }
    invalid("Unterminated Mario Figatree script");
}
float sample(const std::vector<Segment>& track,float frame) {
    const Segment* selected=&track.front();
    for(const auto& s:track) {if(s.at>frame)break;selected=&s;}
    const Segment& s=*selected;
    if(s.duration<=0) return s.target;
    float u=std::clamp((frame-s.at)/s.duration,0.0f,1.0f);
    if(s.kind==3)return u<1?s.base:s.target;
    if(s.kind!=2)return s.base+(s.target-s.base)*u;
    float u2=u*u,u3=u2*u;
    return (2*u3-3*u2+1)*s.base+(-2*u3+3*u2)*s.target+
        (u3-2*u2+u)*s.duration*s.rate0+(u3-u2)*s.duration*s.rate1;
}
void assign(Transform& transform,unsigned track,float v) {
    switch(track) {
    case 0:transform.rotation.x=v;break;case 1:transform.rotation.y=v;break;
    case 2:transform.rotation.z=v;break;
    case 3:invalid("Mario path interpolation needs a source path descriptor");
    case 4:transform.translation.x=v;break;case 5:transform.translation.y=v;break;
    case 6:transform.translation.z=v;break;
    case 7:transform.scale.x=v;break;case 8:transform.scale.y=v;break;
    case 9:transform.scale.z=v;break;
    }
}
}
Animation decode_animation(uint32_t id,const Bytes& raw,const Model& model) {
    auto found=std::find_if(std::begin(motion_recipes),std::end(motion_recipes),
                           [id](const MotionRecipe& r){return r.id==id;});
    if(found==std::end(motion_recipes) || !found->slots || found->slots>26 || found->wide)
        invalid("Unsupported Mario animation format");
    if(model.joints.size()!=25)invalid("Mario animation requires the Mario rig");
    std::array<Tracks,25> joint_tracks;Animation result;result.id=id;
    for(unsigned slot=0;slot<found->slots;++slot) {
        size_t p=pointer(raw,slot*4);if(!p)continue;
        if(found->auxiliary && slot==0)continue; // Tomba owns all root motion.
        unsigned joint=slot-(found->auxiliary?1:0);
        if(joint>=25)invalid("Mario animation joint outside rig");
        try {joint_tracks[joint]=figatree(raw,p,result.duration);}
        catch(const std::exception& e) {throw std::runtime_error("Motion " +
            std::to_string(id) + " joint " + std::to_string(joint) + ": " + e.what());}
    }
    unsigned count=unsigned(std::ceil(result.duration))+1;
    if(count>4097)invalid("Mario animation frame limit");
    result.frames.resize(count);
    for(unsigned f=0;f<count;++f) for(unsigned j=0;j<25;++j) {
        Transform& t=result.frames[f][j];t=model.joints[j].bind;
        for(unsigned k=0;k<10;++k) if(!joint_tracks[j][k].empty())
            assign(t,k,sample(joint_tracks[j][k],float(f)));
    }
    return result;
}
TextureAtlas make_texture_atlas(const Model& model) {
    TextureAtlas atlas;
    atlas.texture.width=256;atlas.texture.height=64;
    atlas.texture.pixels.resize(256*64);
    unsigned x=0,y=0,row_height=0;
    for(const auto& texture:model.textures) {
        unsigned w=texture.width,h=texture.height;
        if(!w || !h || texture.pixels.size()!=size_t(w)*h || w+2>256 || h+2>64)
            invalid("Mario texture cannot fit native-UV atlas");
        if(x+w+2>256) {x=0;y+=row_height;row_height=0;}
        if(y+h+2>64)invalid("Mario textures exceed native-UV atlas");
        atlas.origins.push_back({uint16_t(x+1),uint16_t(y+1)});
        for(unsigned row=0;row<h+2;++row)for(unsigned col=0;col<w+2;++col) {
            unsigned sx=col?std::min(col-1,w-1):0,sy=row?std::min(row-1,h-1):0;
            atlas.texture.pixels[(y+row)*256+x+col]=texture.pixels[sy*w+sx];
        }
        x+=w+2;row_height=std::max(row_height,h+2);
    }
    return atlas;
}
} // namespace tomba2::mario
