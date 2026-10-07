#include "render_pass_ot.hpp"

/* USA MAIN.EXE's two scene draw aggregates consume the model/effect queues.
 * Replay stops at their return; the scheduler, simulation, audio and input
 * remain outside. Finish at the main-loop PutDispEnv AFTER its VBlank wait.
 * The generated hooks also apply to the original-disc AOT overlay bodies. */
namespace {
constexpr uint32_t SceneA=0x8003F9A8,EndA=0x8003FA14;
constexpr uint32_t SceneB=0x8003FA44,EndB=0x8003FA9C;
constexpr uint32_t PutDispEnv=0x8008179C,PutDrawEnv=0x800815D0;
constexpr uint32_t DrawOTag=0x80081560,DrawSync=0x80080F6C;
constexpr uint32_t Base=0x800E80A8,Stride=0x2070,CurrentOT=0x800ED8C8;
PSXDrawReplay replay;
PSXOTReplay ot;
uint32_t ticks=0,captured_tick=0,buffer=0,scene=0;
bool capturing=false,ready=false;
uint32_t rd(uint32_t p){return psx_mod_read_word(p);}
uint16_t rh(uint32_t p){return psx_mod_read_half(p);}
void reset(){capturing=ready=false;scene=0;replay.invalidate();ot.reset();}
void tick(){if(!g_psx_render_pass_active){++ticks;if(ticks-captured_tick>8 && (capturing||ready))reset();}}
void begin(CPUState* cpu,uint32_t address) {
    if(g_psx_render_pass_active)return;
    const uint32_t next=rd(CurrentOT);
    if(!psx_mod_game_started() || psx_mod_read_byte(0x1F80019C)!=0 ||
       (next!=Base && next!=Base+Stride) || rd(SceneA)!=0x27BDFFE8 ||
       rd(0x80050D48)!=0x0C0205E7){reset();return;}
    if(capturing || (scene && scene!=address))reset();
    scene=address;buffer=next;captured_tick=ticks;ready=false;capturing=true;
    replay.capture(cpu,scene,scene==SceneA?EndA:EndB);
}
void end(CPUState*,uint32_t) {
    if(g_psx_render_pass_active || !capturing)return;
    capturing=false;
    ready=replay.prepare(captured_tick) && ot.capture(buffer,2048);
    psx_mod_counter_add("tomba2.fr.frames",1);
    psx_mod_counter_add("tomba2.fr.projections",replay.stats.changed);
}
int pass(CPUState* cpu,void*,uint32_t alpha) {
    if(!replay.restore(cpu,alpha))return 0;
    const uint32_t other=buffer==Base?Base+Stride:Base;
    const int from=rh(buffer+0x2016),to=rh(other+0x2016),height=rh(other+0x201A);
    // SDK DRAWENV contains generated GP0 storage as well as its rectangle.
    // Install the complete pending display environment before drawing.
    for(uint32_t i=0;i<0x5C;i+=4)psx_mod_write_word(buffer+0x2014+i,rd(other+0x2014+i));
    PSXDrawReplay::call(cpu,PutDrawEnv,buffer+0x2014);
    if(!replay.draw(cpu)){psx_mod_counter_add("tomba2.fr.draw_failed",1);return 0;}
    ot.apply_late();
    if(!PSXOTReplay::retarget_y(buffer+0x1FFC,from,to,height)) {
        psx_mod_counter_add("tomba2.fr.environment_rejected",1);return 0;
    }
    PSXDrawReplay::call(cpu,DrawOTag,buffer+0x1FFC);
    PSXDrawReplay::call(cpu,DrawSync,0);
    return 1;
}
void finish(CPUState* cpu,uint32_t) {
    if(g_psx_render_pass_active || cpu->gpr[31]!=0x80050D50 || !ready)return;
    ready=false;
    const uint32_t other=buffer==Base?Base+Stride:Base;
    if(rd(CurrentOT)!=buffer || cpu->gpr[4]!=buffer+0x2000 ||
       psx_mod_read_byte(0x1F80019C)!=0 ||
       rd(buffer+0x2000)!=rd(other+0x2014) || rd(buffer+0x2004)!=rd(other+0x2018))return;
    if(!ot.preserve_late()){psx_mod_counter_add("tomba2.fr.overlay_rejected",1);return;}
    PSXModRenderPassFrame frame{};frame.struct_size=sizeof frame;
    frame.period_vblanks=replay.ticks;frame.shown_after_vblanks=0;
    frame.x=rh(buffer+0x2000);frame.y=rh(buffer+0x2002);
    frame.w=rh(buffer+0x2004);frame.h=rh(buffer+0x2006);
    psx_mod_counter_add("tomba2.fr.passes",psx_mod_render_pass_frame(cpu,&frame,pass,nullptr));
}
void rate(unsigned fps){reset();ticks=0;PSXDrawReplay::rate(fps,PSX_MOD_RENDER_PASS_FLIP_PENDING);}
void display(){rate(0);} void rate60(){rate(60);} void rate90(){rate(90);}
void rate120(){rate(120);} void rate144(){rate(144);} void rate165(){rate(165);} void rate240(){rate(240);}
}
PSX_MOD_CONSTRUCTOR(tomba2_register_frame_rate_plugins) {
    const char* ids[]={"tomba2.framerate.display","tomba2.framerate.60","tomba2.framerate.90",
        "tomba2.framerate.120","tomba2.framerate.144","tomba2.framerate.165","tomba2.framerate.240"};
    void(*rates[])()={display,rate60,rate90,rate120,rate144,rate165,rate240};
    for(unsigned i=0;i<7;++i) {
        psx_mod_register_activation_plugin(ids[i],rates[i]);
        psx_mod_register_vblank_plugin(ids[i],tick);
        psx_mod_register_savestate_plugin(ids[i],reset);
        psx_mod_register_function_entry_plugin(ids[i],SceneA,begin);
        psx_mod_register_function_entry_plugin(ids[i],SceneB,begin);
        psx_mod_register_instruction_plugin(ids[i],EndA,0x03E00008,end);
        psx_mod_register_instruction_plugin(ids[i],EndB,0x03E00008,end);
        psx_mod_register_function_entry_plugin(ids[i],PutDispEnv,finish);
    }
}
