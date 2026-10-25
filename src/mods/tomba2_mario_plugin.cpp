#include "mod_plugins.h"
#include "cpu_state.h"
#include "mario/smash_assets.h"
#include "mario/pcm_voice.h"
#include "mario/voice_cues.h"
#include "mario/presentation.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>

namespace {
using namespace tomba2::mario;
constexpr const char* plugin="tomba2.mario.hat";
constexpr uint32_t player=0x800e7e80, item_table=0x800a2be8;
constexpr uint32_t ownership=0x800bfab4, item_order=0x800bfbb4;
constexpr uint32_t virtual_hat=256, return_pc=0x8f00fff0;
constexpr uint32_t state_magic=0x4d484154;
enum StateOffset : uint32_t {
    Magic=0,Requested=4,Equipped=8,DoubleJump=12,Clip=16,Clock=20,
    LastAction=24,MenuPrimary=28,IconItem=32,
    Voice=36,VoiceCursor=40,VoiceFraction=44,PreviousAction=48,PreviousAir=52,MenuObject=56,HatToast=60,
    Name=128,Description=160,Toast=256,Table=512,Icons=4608,Weapons=6000
};
uint32_t memory=0,packets=0;
Model mario_model;
TextureAtlas mario_atlas;
std::map<uint32_t,Animation> animations;
std::map<uint32_t,Bytes> voices;
bool builder_nested=false,equipment_nested=false;
bool weapon_nested=false,item_nested=false;
bool captured_draw_nested=false;

uint8_t rb(uint32_t p) {return psx_mod_read_byte(p);}
uint16_t rh(uint32_t p) {return psx_mod_read_half(p);}
uint32_t rw(uint32_t p) {return psx_mod_read_word(p);}
void wb(uint32_t p,uint8_t v) {psx_mod_write_byte(p,v);}
void wh(uint32_t p,uint16_t v) {psx_mod_write_half(p,v);}
void ww(uint32_t p,uint32_t v) {psx_mod_write_word(p,v);}
uint32_t state(StateOffset offset) {return rw(memory+offset);}
void state(StateOffset offset,uint32_t value) {ww(memory+offset,value);}
void play_voice(uint32_t id) {
    state(Voice,id);state(VoiceCursor,0);state(VoiceFraction,0);
    psx_mod_counter_add("tomba2.mario.voice",1);
}
void mix_voice(int16_t* stereo,uint32_t frames) {
    if(!memory || state(Magic)!=state_magic)return;
    auto clip=voices.find(state(Voice));
    if(clip==voices.end())return;
    const auto& bytes=clip->second;
    VoicePosition position{state(VoiceCursor),state(VoiceFraction)};
    auto mixed=mix_pcm_voice(bytes.data(),bytes.size(),stereo,frames,position,
                             voice_sample_rate(state(Voice)));
    state(VoiceCursor,position.cursor);state(VoiceFraction,position.fraction);
    if(mixed.finished)state(Voice,0);
    psx_mod_counter_add("tomba2.mario.voice_frames",mixed.frames);
}
bool ready() {
    if(!memory || !psx_mod_game_started())return false;
    if(state(Magic)==state_magic)return true;
    // The table is part of MAIN.EXE, loaded after the boot executable.
    if(rw(0x800368d0)!=0x90830024u)return false;
    for(uint32_t i=0;i<256*12;i+=4)ww(memory+Table+i,rw(item_table+i));
    for(uint32_t i=0;i<512;i+=4)ww(memory+Icons+i,rw(0x800a24f4+i));
    static constexpr char name[]="Mario's\nHat";
    static constexpr char description[]="Become Mario! Double jump, punch,\nand grab and throw enemies.";
    for(size_t i=0;i<sizeof(name);++i)wb(memory+Name+uint32_t(i),uint8_t(name[i]));
    for(size_t i=0;i<sizeof(description);++i)wb(memory+Description+uint32_t(i),uint8_t(description[i]));
    ww(memory+Table+virtual_hat*12,0x01090200); // equipment / costume / order 9
    ww(memory+Table+virtual_hat*12+4,memory+Name);
    ww(memory+Table+virtual_hat*12+8,memory+Description);
    wh(memory+Icons+virtual_hat*2,rh(0x800a24f4+14*2));
    state(Magic,state_magic);return true;
}
struct TableSite {uint32_t address,word;bool icons;};
#include "mario/table_sites.h"
void redirect_table(CPUState* cpu,uint32_t address) {
    if(!ready())return;
    for(const auto& site:table_sites)if((site.address&0x1fffffff)==(address&0x1fffffff)) {
        unsigned rs=(site.word>>21)&31;
        int32_t displacement=int16_t(site.word);
        uint32_t effective=cpu->gpr[rs]+uint32_t(displacement);
        uint32_t old=site.icons?0x800a24f4:item_table;
        uint32_t extent=site.icons?514:257*12;
        if(effective>=old && effective<old+extent)
            cpu->gpr[rs]+=memory+(site.icons?Icons:Table)-old;
        return;
    }
}
void original(CPUState* cpu,uint32_t function) {
    psx_mod_call_guest(cpu,function,return_pc,cpu->gpr[4],cpu->gpr[5],cpu->gpr[6],cpu->gpr[7]);
}
int draw_item_name(CPUState* cpu,uint32_t address) {
    if(item_nested || !ready())return 0;
    uint32_t item=cpu->gpr[5],menu=cpu->gpr[4],column=cpu->gpr[6],y=cpu->gpr[7];
    uint32_t layer=rw(cpu->gpr[29]+20);
    item_nested=true;original(cpu,address);item_nested=false;
    state(IconItem,item);
    if(item==virtual_hat && state(Requested) && column<=1 && rb(menu+8)==0)
        psx_mod_call_guest(cpu,0x800342ec,return_pc,menu,column?0xb4:0x24,y,layer);
    return 1;
}
int draw_item_icon(CPUState* cpu,uint32_t) {
    if(!ready() || state(IconItem)!=virtual_hat)return 0;
    state(IconItem,0xffffffffu);
    uint32_t flip=rb(0x1f800135);
    if(flip>1)return 0;
    // Native inventory sprites have signed offsets around this anchor. Centre
    // the imported 8x4 cap (drawn at 2x) in the same slot as the stock icons.
    const auto& icon=mario_model.hat_icon;
    int x=int16_t(cpu->gpr[5])-int(icon.width);
    int y=int16_t(cpu->gpr[6])-int(icon.height);
    uint32_t link=rw(0x800ed8c8)+(cpu->gpr[7]&255)*4;
    uint32_t cursor=packets+(flip*1024+1016)*80;
    for(unsigned row=0;row<icon.height;++row)for(unsigned col=0;col<icon.width;++col) {
        uint16_t pixel=icon.pixels[row*icon.width+col];
        if(!pixel)continue;
        unsigned r=(pixel&31)*255/31,g=((pixel>>5)&31)*255/31,b=((pixel>>10)&31)*255/31;
        ww(cursor,0x03000000u|(rw(link)&0xffffff));
        ww(cursor+4,0x60000000u|r|(g<<8)|(b<<16));
        ww(cursor+8,uint16_t(x+int(col)*2)|(uint32_t(uint16_t(y+int(row)*2))<<16));
        ww(cursor+12,0x00020002u);
        ww(link,cursor&0xffffff);cursor+=16;
    }
    return 1;
}
int inventory_build(CPUState* cpu,uint32_t address) {
    if(builder_nested || !ready())return 0;
    uint32_t menu=cpu->gpr[4];state(HatToast,0);state(MenuObject,menu);
    builder_nested=true;original(cpu,address);builder_nested=false;
    uint32_t count=rb(menu+0x24);
    state(MenuPrimary,count);
    if(count>=254) {psx_mod_counter_add("tomba2.mario.inventory_full",1);return 1;}
    wb(menu+0x24,uint8_t(count+1));
    unsigned secondary=unsigned(rb(menu+0x25))+rb(menu+0x26);
    wb(menu+0x22,uint8_t((count+2)/2+(secondary+2)/2));
    psx_mod_counter_add("tomba2.mario.inventory_build",1);return 1;
}
int inventory_lookup(CPUState* cpu,uint32_t) {
    if(!ready())return 0;
    uint32_t menu=cpu->gpr[4],row=cpu->gpr[5],column=cpu->gpr[6];
    unsigned count=rb(menu+0x24),rows=(count+1)/2;
    if(!count || column>1)return 0;
    int result=-1;
    unsigned flat=row*2+column;
    if(row<rows) {
        if(flat<count) {
            if(flat+1==count)result=int(virtual_hat);
            else for(unsigned i=0;i<256;++i)
                if(rb(ownership+i) && rb(item_table+i*12)==0 && rb(item_order+i)==flat) {result=int(i);break;}
        }
    } else {
        unsigned secondary=unsigned(rb(menu+0x25))+rb(menu+0x26);
        unsigned index=(row-rows)*2+column;
        if(index==secondary)result=999;
        else if(index<secondary)for(unsigned i=0;i<256;++i)
            if(rb(ownership+i) && rb(item_table+i*12)!=0 && rb(item_order+i)==index) {result=int(i);break;}
    }
    cpu->gpr[2]=uint32_t(result);return 1;
}
int equip_hat(CPUState* cpu,uint32_t) {
    if(!ready() || cpu->gpr[5]!=virtual_hat)return 0;
    bool restricted=(rb(0x800bf881)&4) || (rh(0x800bf89e)&0x200);
    if(cpu->gpr[6]==1) {cpu->gpr[2]=restricted?1:0;return 1;}
    // Use the existing costume transaction with the ordinary native costume.
    // The virtual item's identity never enters the byte-sized save structure.
    if(restricted) {cpu->gpr[2]=4;return 1;}
    state(Requested,state(Requested)?0:1);
    state(HatToast,1);
    wb(0x800bf88f,0);wb(0x800bf81d,1);
    cpu->gpr[2]=2;psx_mod_counter_add("tomba2.mario.equip_requested",1);return 1;
}
int equip_native(CPUState* cpu,uint32_t address) {
    if(equipment_nested || !ready() || cpu->gpr[6]==1 || cpu->gpr[5]>=256 ||
       rb(item_table+cpu->gpr[5]*12+1)!=2)return 0;
    equipment_nested=true;
    uint32_t result=psx_mod_call_guest(cpu,address,return_pc,cpu->gpr[4],cpu->gpr[5],cpu->gpr[6],cpu->gpr[7]);
    equipment_nested=false;
    if(result==2 || result==3) {state(Requested,0);state(HatToast,0);}
    cpu->gpr[2]=result;return 1;
}
void costume_reload(CPUState* cpu,uint32_t) {
    if(!ready() || cpu->gpr[4]!=player)return;
    state(Equipped,state(Requested) && rb(player+0x6f)==0?1:0);
    if(state(Equipped))play_voice(437);
    state(DoubleJump,0);state(Clock,0);state(Clip,0);
    psx_mod_counter_add("tomba2.mario.costume_reload",1);
}
void costume_attributes(CPUState* cpu,uint32_t) {
    if(ready() && cpu->gpr[4]==player && rb(player+0x6f)!=0)state(Requested,0);
}
void costume_toast(CPUState* cpu,uint32_t) {
    if(!ready() || !state(HatToast) || cpu->gpr[6]!=0x40)return;
    uint32_t caller=cpu->gpr[31]&0x1fffffff,source=cpu->gpr[4],sp=cpu->gpr[29];
    // The queue receives either an ordinary costume-zero equipped message or
    // a two-item remove/equip message when replacing a native suit. Confirm
    // the costume caller as well as the token list: weapon/shoe confirmations
    // can also mention item zero during the same inventory transaction.
    bool ordinary=caller==0x4ed84 && cpu->gpr[5]==1 &&
        (rw(sp+0x18)&0x1fffffff)==0x4d6e4 && (rw(sp+0x30)&0x1fffffff)==0x343e4 &&
        rh(source)==0 && rh(source+2)==0x8000 && rh(source+4)==0xfffe;
    bool replacing=caller==0x4ee1c && cpu->gpr[5]==2 &&
        (rw(sp+0x20)&0x1fffffff)==0x343fc &&
        rh(source+2)==0x8003 && rh(source+4)==0xffff && rh(source+6)==0 &&
        rh(source+8)==0x8000 && rh(source+10)==0xfffe;
    if(!ordinary && !replacing)return;
    uint32_t cursor=memory+Toast;
    wb(cursor++,0xf4); // native equipment-name colour
    for(char c:std::string("Mario's Hat"))wb(cursor++,c==' '?0xfb:uint8_t(c-32));
    wb(cursor++,0xf0);
    const char* suffix=state(Requested)?" equipped!":" removed!";
    for(const char* c=suffix;*c;++c)wb(cursor++,*c==' '?0xfb:uint8_t(*c-32));
    wb(cursor,0xff);
    state(HatToast,0);
    // Let the native queue consume already encoded text. Both the queued
    // pointer and buffer live in guest memory and follow current-build states.
    cpu->gpr[4]=memory+Toast;cpu->gpr[5]=0xffffffffu;
    psx_mod_counter_add("tomba2.mario.costume_label",1);
}
int player_sound(CPUState* cpu,uint32_t) {
    if(!ready() || !state(Equipped))return 0;
    uint8_t sound=uint8_t(cpu->gpr[4]);
    // High-bit sounds belong to the current area's bank, not the common
    // Tomba vocal table. Do not silence an unrelated area/NPC effect.
    if(sound>=0x70)return 0;
    uint32_t replacement=native_voice_replacement(sound,rb(0x800a4d18+sound*8));
    if(!replacement)return 0;
    play_voice(replacement);
    psx_mod_counter_add("tomba2.mario.native_voice_replaced",1);
    cpu->gpr[2]=0;return 1;
}

uint32_t action_clip() {
    unsigned action=rb(player+5),phase=rb(player+6);
    if(rb(player+4)!=1)return 523;
    switch(action) {
    case 0:return 499;case 1:return 505;
    case 2:return rb(player+0x29)?518:state(DoubleJump)?511:rb(player+0x145)==1?509:513;
    case 3:return phase>=5?563:phase>=2?562:561;
    case 4:return 523;case 5:case 0x32:return rb(player+0x146)==3?608:606;
    case 6:return 622;
    case 7:case 8:case 9:return 523;
    case 0xa:case 0xb:case 0xc:case 0xd:return 575;
    case 0x10:return 588;
    case 0x11:return 571;
    default:return rb(player+0x145)?513:499;
    }
}
void player_tick(CPUState* cpu,uint32_t) {
    if(!ready() || cpu->gpr[4]!=player)return;
    if(!state(Equipped))return;
    if(rb(player+0x6f)!=0) {state(Equipped,0);state(Requested,0);return;}
    unsigned action=rb(player+5);
    // Native vocal events choose their replacement in player_sound. Do not
    // restart that voice a second time just because an action byte changed.
    state(PreviousAction,action);
    state(PreviousAir,rb(player+0x145));
    // +0x145 is the native air phase (zero after landing). +0x29 is not a
    // grounded flag: it remains zero during ordinary idle/walking here.
    if(action<=2 && !rb(player+0x145))state(DoubleJump,0);
    bool free=rb(player+4)==1 && rb(0x1f800137)==0 && !rb(0x800bf80f);
    bool airborne=action==2 && !rb(player+0x29) && rb(player+0x145)!=0;
    uint16_t jump=rh(0x1f800172);
    if(free && airborne && !state(DoubleJump) && (rh(0x800e7e68)&jump)) {
        // Same velocity units and gravity/collision path as native jumping.
        wh(player+0x4a,uint16_t(-0x2800));wb(player+7,1);wb(player+0x145,1);
        state(DoubleJump,1);state(Clock,0);
        play_voice(436);
        psx_mod_counter_add("tomba2.mario.double_jump",1);
    }
    uint32_t clip=action_clip();
    if(state(Clip)!=clip || state(LastAction)!=(action|(unsigned(rb(player+6))<<8))) {
        state(Clip,clip);state(Clock,0);
    } else state(Clock,state(Clock)+2);
    state(LastAction,action|(unsigned(rb(player+6))<<8));
}

uint32_t weapon_record(uint32_t actor) {
    for(unsigned i=0;i<8;++i) {
        uint32_t entry=memory+Weapons+i*12;
        if(rw(entry)==actor)return entry;
    }
    return 0;
}
void weapon_spawn(CPUState* cpu,uint32_t) {
    if(!ready() || !state(Equipped) || cpu->gpr[16]!=player || !cpu->gpr[2])return;
    uint32_t actor=cpu->gpr[2],entry=weapon_record(actor);
    if(!entry)for(unsigned i=0;i<8;++i)if(!rw(memory+Weapons+i*12)){entry=memory+Weapons+i*12;break;}
    if(!entry) {psx_mod_counter_add("tomba2.mario.punch_slots_full",1);return;}
    ww(entry,actor);ww(entry+4,0);ww(entry+8,cpu->gpr[17]);
    psx_mod_counter_add("tomba2.mario.punch_spawn",1);
}
void fist_position(uint32_t actor) {
    float heading=float(rh(player+0x56)&4095)*6.28318530718f/4096;
    // Native horizontal half/full widths are +0x80/+0x82 (6ACAC initializes
    // ordinary weapons to 50/100). +0x76 is weapon-private state, not radius.
    // 88 + 64 = 152 forward units versus the old 72 + 50 = 122, with nearly
    // identical close overlap. Keep vertical bounds and native damage rules.
    wh(actor+0x80,64);wh(actor+0x82,128);
    ww(actor+0x2c,rw(player+0x2c)+uint32_t(int32_t(std::cos(heading)*88*65536)));
    ww(actor+0x30,rw(player+0x30)-16*65536);
    ww(actor+0x34,rw(player+0x34)+uint32_t(int32_t(std::sin(heading)*88*65536)));
}
void punch_collision(CPUState* cpu,uint32_t) {
    if(!ready() || !weapon_record(cpu->gpr[4]))return;
    fist_position(cpu->gpr[4]);
    psx_mod_counter_add("tomba2.mario.punch_collision",1);
}
int punch_weapon(CPUState* cpu,uint32_t address) {
    if(weapon_nested || !ready())return 0;
    uint32_t actor=cpu->gpr[4],entry=weapon_record(actor);
    if(!entry)return 0;
    // Hammer has a native windup before +6 reaches its damaging phase. Count
    // the four active ticks, rather than retiring during that windup. Keep a
    // separate bounded lifetime for interrupted/invalid native transactions.
    const bool hammer=address==0x8006c478u;
    uint32_t clock=rw(entry+4);
    unsigned phase=rb(actor+4),age=clock&0xffffu,lifetime=clock>>16;
    if(phase<2 && (age>=4 || lifetime>=120 || !state(Equipped))) {
        // Let the original family handler release its model/pool and balance
        // its own active-projectile counters through its normal retire states.
        wb(actor+4,2);
        if(unsigned(rb(player+0x176))+1==rw(entry+8))wb(player+0x79,0);
        // The native hammer clears its terrain-impact latch on normal exit;
        // its generic phase-2 retirement only advances to model destruction.
        if(hammer)wb(0x800e807e,0);
    }
    if(phase==1 && age<4)fist_position(actor);
    weapon_nested=true;original(cpu,address);weapon_nested=false;
    if(phase==3) {ww(entry,0);psx_mod_counter_add("tomba2.mario.punch_retired",1);}
    else {
        if(age<4 && state(Equipped) && rb(actor+4)<2) {
            fist_position(actor);
            bool active=!hammer || (rb(actor+5)==1 && rb(actor+6)>=3);
            if(active) {wb(actor,1);++age;}
        }
        wb(actor+1,0);ww(entry+4,((lifetime+1)<<16)|age);
    }
    return 1;
}
void player_initialized(CPUState* cpu,uint32_t) {
    if(!ready() || cpu->gpr[4]!=player)return;
    for(unsigned i=0;i<8;++i)ww(memory+Weapons+i*12,0);
    state(DoubleJump,0);state(Clock,0);state(Voice,0);
}

struct DrawVertex {Vec3 camera;float u,v,light;};
void wf(uint32_t address,float value) {uint32_t bits;std::memcpy(&bits,&value,4);ww(address,bits);}
DrawVertex lerp(const DrawVertex& a,const DrawVertex& b,float t) {
    return {{a.camera.x+(b.camera.x-a.camera.x)*t,a.camera.y+(b.camera.y-a.camera.y)*t,
             a.camera.z+(b.camera.z-a.camera.z)*t},a.u+(b.u-a.u)*t,a.v+(b.v-a.v)*t,a.light+(b.light-a.light)*t};
}
PoseBones presentation_bones() {
    auto clip=animations.find(state(Clip));
    if(clip==animations.end())clip=animations.find(499);
    const Animation& animation=clip->second;
    unsigned frame=state(Clock);
    bool loop=animation.id==499 || animation.id==505 || animation.id==500 || animation.id==575 || animation.id==562;
    if(loop)frame%=std::max(1u,unsigned(animation.frames.size()-1));
    else frame=std::min(frame,unsigned(animation.frames.size()-1));
    auto bones=pose_bones(mario_model,animation.frames[frame]);
    if(rb(player+4)==1 && rb(player+5)==3 && rb(player+6)>=1 && rb(player+6)<=4)
        raise_capture_arms(bones);
    return bones;
}
Matrix player_world() {
    return mario_world(presentation_heading(rh(player+0x56),rb(player+4),rb(player+5)),
        {float(int32_t(rw(player+0x2c)))/65536,
        float(int32_t(rw(player+0x30)))/65536,float(int32_t(rw(player+0x34)))/65536});
}
int draw_captured(CPUState* cpu,uint32_t address) {
    uint32_t actor=cpu->gpr[4];
    if(captured_draw_nested || rb(player+4)!=1 || rb(player+5)!=3 ||
       rb(player+6)<1 || rb(player+6)>6 || rw(player+0x10)!=actor ||
       actor<0x80010000u || actor>0x801ffe00u || rb(actor+4)!=2 || rb(actor+5)!=1)return 0;
    unsigned count=std::min(rb(actor+8),rb(actor+9));
    if(!count || count>32)return 0;
    std::array<uint32_t,32> pointers{};
    std::array<std::array<uint32_t,3>,32> saved{};
    // Translate only the native model's render matrices. Never move the actor,
    // change capture linkage, or replace native collision/aim/release logic.
    // The callback safety gate prevents snapshots during this temporary span.
    for(unsigned i=0;i<count;++i) {
        uint32_t bone=rw(actor+0xc0+i*4);
        if(bone<0x80010000u || bone>0x801fffbc || (bone&3))return 0;
        for(unsigned j=0;j<i;++j)if(pointers[j]==bone)return 0;
        pointers[i]=bone;
        for(unsigned k=0;k<3;++k)saved[i][k]=rw(bone+0x2c+k*4);
    }
    Vec3 hands=capture_hands(presentation_bones(),player_world());
    // Mirror the native capture attachment's 80 + target-height offset
    // (8005B20C) above the hands, leaving the cap clear of the held body.
    hands.y-=80+float(int16_t(rh(actor+0x84)));
    int32_t delta[3]={int32_t(std::round(hands.x))-int32_t(rw(actor+0x2c))/65536,
        int32_t(std::round(hands.y))-int32_t(rw(actor+0x30))/65536,
        int32_t(std::round(hands.z))-int32_t(rw(actor+0x34))/65536};
    for(unsigned i=0;i<count;++i)for(unsigned k=0;k<3;++k)
        ww(pointers[i]+0x2c+k*4,saved[i][k]+uint32_t(delta[k]));
    captured_draw_nested=true;original(cpu,address);captured_draw_nested=false;
    for(unsigned i=0;i<count;++i)for(unsigned k=0;k<3;++k)
        ww(pointers[i]+0x2c+k*4,saved[i][k]);
    psx_mod_counter_add("tomba2.mario.overhead_draw",1);
    return 1;
}
int draw_body(CPUState* cpu,uint32_t address) {
    if(!ready() || !state(Equipped))return 0;
    if(!psx_mod_texture_banks_supported()) {
        psx_mod_counter_add("tomba2.mario.renderer_unavailable",1);return 0;
    }
    if(cpu->gpr[4]!=player)return draw_captured(cpu,address);
    auto bones=presentation_bones();
    Matrix world=player_world();
    Matrix view;
    for(unsigned i=0;i<9;++i)view.m[i/3][i%3]=float(int16_t(rh(0x1f8000f8+i*2)))/4096;
    for(unsigned i=0;i<3;++i)view.m[i][3]=float(int32_t(rw(0x1f80010c+i*4)));
    Matrix camera=multiply(view,world);
    for(auto& bone:bones)bone=multiply(camera,bone);
    float h=float(cpu->gte_ctrl[26]&65535);
    float ox=float(int32_t(cpu->gte_ctrl[24]))/65536,oy=float(int32_t(cpu->gte_ctrl[25]))/65536;
    float near=std::max(16.0f,h*0.5f);
    uint32_t ot=rw(0x800ed8c8),flip=rb(0x1f800135);
    if(!h || flip>1 || !ot || (ot&3))return 0;
    uint32_t cursor=packets+flip*1024*80;unsigned emitted=0;
    for(const Triangle& triangle:mario_model.triangles) {
        std::array<DrawVertex,5> input{},clipped{};unsigned count=3,out=0;
        for(unsigned i=0;i<3;++i) {
            const auto& v=triangle.vertices[i];
            Vec3 normal=v.normal;
            float light=std::clamp(0.85f+0.15f*(normal.y-normal.z),0.6f,1.0f);
            input[i]={transform(bones[v.joint],v.position),v.u,v.v,light};
        }
        for(unsigned i=0;i<count;++i) {
            const auto&a=input[i];const auto&b=input[(i+1)%count];
            bool inside=a.camera.z>=near,next=b.camera.z>=near;
            if(inside)clipped[out++]=a;
            if(inside!=next)clipped[out++]=lerp(a,b,(near-a.camera.z)/(b.camera.z-a.camera.z));
        }
        if(out<3)continue;
        const Texture& texture=mario_model.textures[triangle.texture];
        for(unsigned t=1;t+1<out;++t) {
            if(emitted>=1016) {psx_mod_counter_add("tomba2.mario.packet_limit",1);return 1;}
            DrawVertex v[3]={clipped[0],clipped[t],clipped[t+1]};
            float sx[3],sy[3],q[3],depth=0;
            bool valid=true;
            for(unsigned i=0;i<3;++i) {
                q[i]=1/v[i].camera.z;sx[i]=ox+h*v[i].camera.x*q[i];sy[i]=oy+h*v[i].camera.y*q[i];
                depth+=v[i].camera.z;
                valid=valid && std::abs(sx[i])<2048 && std::abs(sy[i])<2048;
            }
            if(!valid || !visible_face(triangle.cull,sx,sy))continue;
            // Match the native AVSZ3 result and Tomba's nonlinear OT bins.
            uint32_t z=uint32_t(std::max(0.0f,depth/12));
            uint32_t shift=z>>10;
            if(shift>=32)continue;
            z=(z>>shift)+(shift<<9);
            if(z<4 || z>=2048)continue;
            uint32_t link=ot+z*4,bank=0xe000u;
            ww(cursor,0x09000000u|(rw(link)&0xffffff));
            for(unsigned i=0;i<3;++i) {
                uint32_t light=uint32_t(128*v[i].light);
                uint32_t rgb=light|(light<<8)|(light<<16);
                if(i==0)rgb|=0x34000000u;
                else if(i==1)rgb|=(bank&255)<<24;
                else rgb|=(bank>>8)<<24;
                ww(cursor+4+i*12,rgb);
                int x=int(std::round(sx[i])),y=int(std::round(sy[i]));
                ww(cursor+8+i*12,uint16_t(x)|(uint32_t(uint16_t(y))<<16));
                unsigned u=unsigned(std::clamp(v[i].u,0.0f,float(texture.width-1)));
                unsigned vv=unsigned(std::clamp(v[i].v,0.0f,float(texture.height-1)));
                u+=mario_atlas.origins[triangle.texture][0];
                vv+=mario_atlas.origins[triangle.texture][1];
                ww(cursor+12+i*12,u|(vv<<8)|(i==1?0x01000000u:0));
            }
            ww(cursor+40,0x48545031u);
            for(unsigned i=0;i<3;++i) {
                wf(cursor+44+i*4,q[i]);wf(cursor+56+i*8,sx[i]);wf(cursor+60+i*8,sy[i]);
            }
            ww(link,cursor&0xffffff);cursor+=80;++emitted;
        }
    }
    psx_mod_counter_add("tomba2.mario.draws",1);
    psx_mod_counter_add("tomba2.mario.triangles",emitted);
    return 1;
}

// The original actor update, collision, attachments and shadow remain guest-owned.
void bind_textures() {
    const auto& t=mario_atlas.texture;
    if(!psx_mod_define_texture_bank(0xe000,t.width,t.height,t.pixels.data()))
        throw std::runtime_error("Cannot allocate Mario texture atlas");
}
void activate() {
    const uint8_t* data=nullptr;uint64_t size=0;
    if(!psx_mod_current_resource_bytes("mario-assets",&data,&size))
        throw std::runtime_error("Mario asset cache was not prepared");
    auto files=read_cache(Bytes(data,data+size));mario_model=decode_model(files);
    mario_atlas=make_texture_atlas(mario_model);
    voices.clear();
    for(uint32_t voice=429;voice<=441;++voice)voices.emplace(voice,files.at(voice_cache_base+voice));
    animations.clear();
    for(uint32_t id:{499,500,502,505,508,509,510,511,512,513,515,518,523,538,
                        541,561,562,563,564,571,572,573,575,588,597,606,607,608,
                        611,622,623,624,625,626})
        animations.emplace(id,decode_animation(id,files.at(id),mario_model));
    memory=psx_mod_alloc_guest_memory(8192,16);
    packets=psx_mod_alloc_texture_packet_memory(2*1024*80,16);
    if(!memory || !packets)throw std::runtime_error("Cannot allocate Mario runtime memory");
    psx_mod_counter_add("tomba2.mario.state_address",memory);
    bind_textures();
}
// Tables and playback cursors are restored with guest memory. Invalidating
// Magic here would suppress PCM until another gameplay/UI hook calls ready().
void restored() {bind_textures();}
}

PSX_MOD_CONSTRUCTOR(tomba2_register_mario_plugin) {
    psx_mod_register_activation_plugin(plugin,activate);
    psx_mod_register_pcm_plugin(plugin,mix_voice);
    psx_mod_register_savestate_plugin(plugin,restored);
    psx_mod_register_function_filter_plugin(plugin,0x80036240,inventory_build);
    psx_mod_register_function_filter_plugin(plugin,0x800368d0,inventory_lookup);
    psx_mod_register_function_filter_plugin(plugin,0x80035fac,equip_hat);
    psx_mod_register_function_filter_plugin(plugin,0x800349a4,equip_native);
    psx_mod_register_function_filter_plugin(plugin,0x800376ec,draw_item_name);
    psx_mod_register_function_filter_plugin(plugin,0x8007e938,draw_item_icon);
    psx_mod_register_function_filter_plugin(plugin,0x80074590,player_sound);
    psx_mod_register_function_entry_plugin(plugin,0x8004fa38,costume_toast);
    psx_mod_register_function_entry_plugin(plugin,0x80057fd4,costume_reload);
    psx_mod_register_function_entry_plugin(plugin,0x80067ef4,costume_attributes);
    psx_mod_register_function_entry_plugin(plugin,0x8005950c,player_tick);
    psx_mod_register_function_filter_plugin(plugin,0x8003cdd8,draw_body);
    psx_mod_register_instruction_plugin(plugin,0x800534dc,0x00402021,weapon_spawn);
    psx_mod_register_function_entry_plugin(plugin,0x80069858,punch_collision);
    psx_mod_register_function_entry_plugin(plugin,0x80058648,player_initialized);
    for(uint32_t address:{0x8006ae28u,0x8006bb6cu,0x8006c478u})
        psx_mod_register_function_filter_plugin(plugin,address,punch_weapon);
    for(const auto& site:table_sites)
        psx_mod_register_instruction_plugin(plugin,site.address,site.word,redirect_table);
}
