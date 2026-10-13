/* SPDX-License-Identifier: GPL-3.0-only */
/* Complete insert matrix; host instruction counts are not FM-1 CPU percentages. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif
static volatile uint32_t checksum;
static uint64_t instructions(void){
#ifdef __APPLE__
    struct rusage_info_v4 r;if(!proc_pid_rusage(getpid(),RUSAGE_INFO_V4,(rusage_info_t *)&r))return r.ri_instructions;
#endif
    return 0;
}
static void setup(const uint8_t modes[3],int tracks,int setting,int mixer){
    for(uint32_t id=0;id<RES_COUNT;id++)resource_release(id);
    memset(trk,0,sizeof trk);memset(&song,0,sizeof song);memset(fx_state,0,sizeof fx_state);host_tracks_init();spread_on=0;
    for(int t=0;t<tracks;t++){
        if(mixer){host_preset(&trk[t],t==0?ENGI_PROPHET:t==3?ENGI_DRUM:ENGI_FM6,t==0?3:0);trk[t].p[P_SUS]=127;trk_note_on(&trk[t],48+t*7,100);if(t<3)trk_note_on(&trk[t],52+t*7,100);}
        for(int s=0;s<3;s++){int b=P_FXTYPE+s*FX_PARAMS;trk[t].p[b]=modes[s];trk[t].p[b+1]=100;trk[t].p[b+2]=setting?127:60;trk[t].p[b+3]=setting?127:64;}
    }
    if(mixer>=2){for(uint32_t k=0;k<tracks;k++){memset(trk[k].p+P_FXTYPE,0,12*sizeof(int16_t));trk[k].p[P_FXSEND]=127;trk[k].p[P_FXSEND2]=trk[k].p[P_FXSEND3]=127;}
        for(uint32_t k=0;k<3;k++){song.g[G_MFXTYPE+k*4]=modes[k];song.g[G_MFXMIX+k*4]=127;song.g[G_MFXRATE+k*4]=setting?127:60;song.g[G_MFXCHAR+k*4]=setting?127:64;}song.g[G_FXRETURN]=100;song.g[G_FXMODE]=mixer==3;fx_bus_mix=0;fx_bus_select();
    }
}
static void block(int tracks,int mixer,uint32_t frame){
    int32_t out[CTL*2],b[CTL];
    if(mixer){mix_block(out,CTL);checksum^=(uint32_t)out[0];return;}
    for(uint32_t i=0;i<CTL;i++)b[i]=sine_i((frame*CTL+i)*0x01734567u)/2;
    for(int t=0;t<tracks;t++){int active=fx_process(&trk[t],b,CTL);checksum^=(uint32_t)(active?fx_left[0]:b[0]);}
}
static double run(const char *kind,const uint8_t modes[3],int tracks,int setting,int mixer,int frames){
    setup(modes,tracks,setting,mixer);for(uint32_t i=0;i<256;i++)block(tracks,mixer,i);
    uint64_t i0=instructions(),t0=now_ns();
    for(int i=0;i<frames;i++)block(tracks,mixer,i+256);
    double ns=(double)(now_ns()-t0)/(frames*CTL),ipc=(double)(instructions()-i0)/(frames*CTL);
    for(int t=0;t<(mixer>=2?1:tracks);t++)for(int s=0;s<3;s++)if(modes[s]){uint32_t si=mixer>=2?NPART*3+s:t*3+s;assert(fx_state[si].type==modes[s] && !fx_state[si].failed);}
    printf("%s,%u,%u,%u,%d,%d,%.3f,%.3f,%u\n",kind,modes[0],modes[1],modes[2],tracks,setting,ipc,ns,resource_used());
    return ipc;
}
int main(int argc,char **argv){
    int full=argc>1;puts("kind,slot1,slot2,slot3,tracks,extreme,host_instructions_per_frame,host_ns_per_frame,allocated_bytes");
    for(int setting=0;setting<2;setting++)for(int tracks=1;tracks<=4;tracks+=3)for(int type=0;type<FX_COUNT;type++){
        uint8_t modes[3]={type,0,0};double cost=run("single",modes,tracks,setting,0,2048);
        /* Apple instruction counters are deterministic; leave 20% headroom
         * above the measured ~112 instructions per stereo sample frame.
         * Other hosts return zero rather than a target CPU estimate. */
        if(type==FX_CHORUS && tracks==1 && cost>0)assert(cost<=135.0);
        run("mixer",modes,tracks,setting,1,2048);run("send",modes,tracks,setting,2,2048);run("separate-send",modes,tracks,setting,3,2048);
        if(type){modes[1]=modes[2]=type;run("triple",modes,tracks,setting,0,2048);run("mixer-triple",modes,tracks,setting,1,2048);run("send-triple",modes,tracks,setting,2,2048);run("separate-send-triple",modes,tracks,setting,3,2048);}
    }
    if(full)for(int a=1;a<FX_COUNT;a++)for(int b=1;b<FX_COUNT;b++)for(int c=1;c<FX_COUNT;c++)for(int setting=0;setting<2;setting++){
        uint8_t modes[3]={a,b,c};run("matrix",modes,4,setting,0,256);
    }
    fprintf(stderr,"FX performance: all effects, three-slot/four-track chains, mixer and %u-row matrix passed (%u)\n",full?2u*(FX_COUNT-1u)*(FX_COUNT-1u)*(FX_COUNT-1u):0u,checksum);return 0;
}
