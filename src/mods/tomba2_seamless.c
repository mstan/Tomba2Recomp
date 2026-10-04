/* SCUS-94454 resident loading enhancement. Guest clocks, CD timing and audio
 * pacing are untouched. Unsupported requests execute the original reader. */
#include "mod_plugins.h"
#include "bios_hle.h"
#include "cpu_state.h"
#include "dirty_ram_interp.h"
#include "spu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern int tomba2_seamless_prepare(void);
extern const unsigned char *tomba2_seamless_source(unsigned lba,unsigned bytes);
extern const unsigned char *tomba2_seamless_texture(unsigned width,const unsigned char *encoded,
    unsigned size,unsigned *output_size);
extern uint8_t *memory_get_ram_ptr(void);
static int ready,trace,retail,synchronous_worker;
static uint32_t synchronous_sp,synchronous_return;
static unsigned frame,reads,last_read,trace_lines;
static int (*previous_hook)(CPUState *,uint32_t);
static uint32_t r32(uint32_t a) { return psx_mod_read_word(a); }
static uint16_t r16(uint32_t a) { return psx_mod_read_half(a); }
static void w32(uint32_t a,uint32_t v) { psx_mod_write_word(a,v); }
static void copy_ram(uint32_t a,const unsigned char *p,unsigned n) {
    /* Asset writes invalidate executable pages and their overlay generations. */
    if(a>=0x800BF000u) {
        memcpy(memory_get_ram_ptr()+(a&0x1FFFFFFFu),p,n);
        dirty_ram_mark_executable_range(a&0x1FFFFFFFu,n);
        for(uint32_t q=a;q<a+n;q=(q&~255u)+256) psx_mod_write_byte(q,p[q-a]);
    } else {
        for(unsigned i=0;i<n;i++) psx_mod_write_byte(a+i,p[i]);
    }
}
static int resident_read(uint32_t dest,uint32_t lba,unsigned n) {
    const unsigned char *p=n && n<=0x200000u ? tomba2_seamless_source(lba,n) : NULL;
    if(!p || dest<0x80010000u || dest>0x80200000u || n>0x80200000u-dest) return 0;
    copy_ram(dest,p,n);
    w32(0x1F8001F0u,lba); w32(0x1F8001F4u,0); w32(0x1F8001F8u,dest+n);
    w32(0x1F800284u,n/4); w32(0x1F800288u,dest);
    w32(0x800BE0E0u,lba+(n+2047u)/2048u-1);
    psx_mod_write_half(0x800BE0E6u,0);
    psx_mod_write_half(0x800BE0E8u,0);
    psx_mod_write_half(0x800BE0EAu,0);
    ++reads; last_read=frame;
    return 1;
}
static uint32_t guest(CPUState *cpu,uint32_t target,uint32_t ra) {
    uint32_t regs[32],saved_pc=cpu->pc,hi=cpu->hi,lo=cpu->lo;
    memcpy(regs,cpu->gpr,sizeof regs);
    cpu->gpr[31]=ra;
    psx_dispatch_call(cpu,target,ra);
    uint32_t result=cpu->gpr[2];
    memcpy(cpu->gpr,regs,sizeof regs);cpu->pc=saved_pc;cpu->hi=hi;cpu->lo=lo;
    return result;
}
static int loader_layout(void) {
    /* Bind the synchronous worker adapter to the verified original extent
     * table. A remapped asset package must use the game's ordinary workers. */
    static const uint32_t extents[]={6565,98304,6613,6289408,1908,9537536,
                                     9684,3698688,1906,4096};
    for(unsigned i=0;i<sizeof extents/sizeof extents[0];i++)
        if(r32(0x800BE0F0u+i*4)!=extents[i]) return 0;
    return r32(0x8001DB8Cu)==0x27BDFFD8u &&
           r32(0x80044F58u)==0x3C021F80u && r32(0x801FE078u)==0x801FF200u;
}
static int dispatch(CPUState *cpu,uint32_t pc) {
    if(synchronous_worker && pc==0x51FB4u) {
        psx_mod_write_half(0x801FE070u,0);
        psx_mod_write_byte(0x801FE0DCu,0); psx_mod_write_byte(0x801FE0DFu,0);
        cpu->gpr[29]=synchronous_sp; cpu->gpr[31]=synchronous_return;
        return 1;
    }
    if(trace && pc==0x51F14u && cpu->gpr[4]==1) {
        fprintf(stdout,"tomba2 seamless: spawn frame=%u fn=%08X ra=%08X\n",frame,cpu->gpr[5],cpu->gpr[31]);fflush(stdout);
    }
    if(!retail && !synchronous_worker && pc==0x51F14u && cpu->gpr[4]==1 &&
       (cpu->gpr[5]==0x80044F58u || cpu->gpr[5]==0x8004514Cu || cpu->gpr[5]==0x800452C0u) &&
       cpu->gpr[31]==0x80044C50u && r32(0x1F800138u)==0x801FE000u && loader_layout() &&
       psx_mod_read_byte(0x801FE0DEu)<22 &&
       r16(0x801FE070u)==0 && r16(0x801FE0E0u)==0 &&
       !r32(0x800AC63Cu) && !r32(0x800AC620u) && r32(0x800AC62Cu)==3) {
        uint32_t current=r32(0x1F800138u),sp=cpu->gpr[29],target=cpu->gpr[5];
        synchronous_worker=1; synchronous_sp=r32(0x801FE078u); synchronous_return=cpu->gpr[31];
        cpu->gpr[29]=synchronous_sp;
        w32(0x1F800138u,0x801FE070u); w32(0x801FE07Cu,target);
        w32(0x801FE080u,cpu->gpr[28]); psx_mod_write_byte(0x801FE0DFu,0);
        psx_mod_write_half(0x801FE070u,4);
        unsigned before=frame;
        guest(cpu,target,synchronous_return);
        synchronous_worker=0; cpu->gpr[29]=sp; w32(0x1F800138u,current);
        if(trace) { fprintf(stdout,"tomba2 seamless: worker fn=%08X frames=%u..%u\n",target,before,frame);fflush(stdout); }
        return 1;
    }
    if(!retail && pc==0x51F14u && cpu->gpr[4]==1 && cpu->gpr[5]==0x8001DB38u &&
       r32(0x8001DB8Cu)==0x27BDFFD8u && r16(0x801FE070u)==0 &&
       r32(0x1F8001F4u)<=0x80000u &&
       resident_read(r32(0x1F8001F8u),r32(0x1F8001F0u),r32(0x1F8001F4u)*4)) {
        if(psx_mod_read_byte(0x801FE0DCu)) psx_mod_write_byte(0x1F80019Bu,1);
        psx_mod_write_byte(0x801FE0DCu,0); psx_mod_write_byte(0x801FE0DFu,0);
        return 1;
    }
    if(!retail && pc==0x44D8Cu && r32(0x80044D8Cu)==0x27BDFFE0u) {
        uint32_t src=cpu->gpr[6]&0x1FFFFFFFu,dest=cpu->gpr[5]&0x1FFFFFFFu,size=cpu->gpr[7],n=0;
        if(src<0x200000u && size<=0x200000u-src && dest<0x200000u) {
            const unsigned char *p=tomba2_seamless_texture(r16(cpu->gpr[4]+4),
                memory_get_ram_ptr()+src,size,&n);
            if(p && n<=0x200000u-dest) {
                copy_ram(dest|0x80000000u,p,n); cpu->gpr[2]=n;
                return 1;
            }
        }
    }
    /* SsVabTransBody's ordinary DMA write only. Header allocation, voice
     * metadata and all music commands remain the game's original code. */
    if(!retail && pc==0x99150u && cpu->gpr[31]==0x80096A00u &&
       r32(0x80099150u)==0x27BDFFE8u && !r32(0x800AC63Cu) &&
       !r32(0x800AC620u) && r32(0x800AC62Cu)==3) {
        uint32_t src=cpu->gpr[4]&0x1FFFFFFFu,size=cpu->gpr[5];
        uint32_t start=(uint32_t)r16(0x800AC61Cu)<<3,n=(size+63u)&~63u;
        if(size && size<=0x7EFF0u && src<0x200000u && n<=0x200000u-src &&
           start>=0x1010u && start<0x80000u && n<=0x80000u-start) {
            spu_write(0x1F801DA6u,start>>3);
            spu_write(0x1F801DAAu,(spu_read(0x1F801DAAu)&~0x30u)|0x20u);
            for(unsigned i=0;i<n;i+=4) spu_dma_write(r32(src+i));
            w32(0x800AC654u,0); w32(0x800AC658u,cpu->gpr[4]); w32(0x800AC65Cu,n/64);
            w32(0x800AC638u,1);
            cpu->gpr[2]=size;
            if(trace) { fprintf(stdout,"tomba2 seamless: SPU bank frame=%u addr=%X bytes=%u\n",frame,start,n); fflush(stdout); }
            return 1;
        }
    }
    /* MAIN overlays the boot EXE at these PCs. Never intercept the boot image. */
    if((pc==0x1DB8Cu || pc==0x1DC40u) &&
       r32(0x8001DB8Cu)==0x27BDFFD8u && r32(0x8001DB90u)==0xAFB00010u) {
        uint32_t dest=cpu->gpr[4],lba=cpu->gpr[5],size=cpu->gpr[6];
        unsigned n=(size+3u)&~3u;
        const unsigned char *p=size && size<=0x200000u ? tomba2_seamless_source(lba,n) : NULL;
        int safe=p && dest>=0x80010000u && dest<=0x80200000u && n<=0x80200000u-dest &&
            (pc!=0x1DB8Cu || r16(0x801FE070u)==0);
        if(trace) {
            fprintf(stdout,"tomba2 seamless: read frame=%u pc=%05X ra=%08X dest=%08X lba=%u size=%u worker=%u resident=%d\n",
                frame,pc,cpu->gpr[31],dest,lba,size,r16(0x801FE070u),safe&&!retail);
            fflush(stdout);
        }
        last_read=frame;
        if(safe && !retail) {
            resident_read(dest,lba,n); cpu->gpr[2]=size;
            return 1;
        }
    }
    if(trace && pc==0x51F80u && frame-last_read<12 && trace_lines++<4096) {
        uint32_t t=r32(0x1F800138u);
        fprintf(stdout,"tomba2 seamless: yield frame=%u ra=%08X frames=%u thread=%08X phase=%u/%u\n",
            frame,cpu->gpr[31],cpu->gpr[4],t,r16(t+0x48),r16(t+0x4A));
    }
    return previous_hook ? previous_hook(cpu,pc) : 0;
}
static void tick(void) {
    ++frame;
    if(ready && g_psx_bios_hle_hook!=dispatch) {
        previous_hook=g_psx_bios_hle_hook; g_psx_bios_hle_hook=dispatch;
    }
}
static void activate(void) {
    const char *s=getenv("TOMBA2_SEAMLESS_TRACE"); trace=s && !strcmp(s,"1");
    s=getenv("TOMBA2_SEAMLESS_RETAIL"); retail=s && !strcmp(s,"1");
    ready=tomba2_seamless_prepare();
}
PSX_MOD_CONSTRUCTOR(tomba2_register_seamless) {
    (void)psx_mod_register_activation_plugin("tomba2.seamless",activate);
    (void)psx_mod_register_vblank_plugin("tomba2.seamless",tick);
}
