/* SPDX-License-Identifier: GPL-3.0-only */
/* Numerical contracts for native fp32 synthesis. Reference calculations use
 * host doubles/libm, independently of the target's tables and fast math. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>

static void fm6_numerics(void)
{
    fm6_voice_t voice={0};float input[64],output[64];
    voice.ph[0]=0x13579u;voice.fq[0]=139817;voice.g[0]=0.2f;voice.dg[0]=1.1f/64.0f;
    uint32_t phase=voice.ph[0];double gain=voice.g[0],step=voice.dg[0],error=0;
    for(uint32_t i=0;i<64;i++)input[i]=(float)(sin(i*0.19)*3.7);
    fm6_op(&voice,0,output,input,0,FM6_MODERN,64);
    for(uint32_t i=0;i<64;i++){
        gain+=step;
        double want=sin(6.283185307179586*((phase&0xFFFFFFu)/16777216.0+input[i]))*gain;
        double e=fabs(output[i]-want);if(e>error)error=e;
        phase+=(uint32_t)voice.fq[0];
    }
    assert(error<2e-5 && voice.ph[0]==phase);
    /* The Q24 log pitch lookup must remain exact, including negative logs.
     * Two short partial products replace the original wide interpolation. */
    for(int32_t octave=-4;octave<=20;octave++)for(uint32_t i=0;i<1024;i++)
    for(uint32_t f=0;f<16384;f+=127){
        int32_t lf=octave*16777216+(int32_t)(i*16384+f);
        int32_t y=FM6_FREQ[i]+(int32_t)(((int64_t)(FM6_FREQ[i+1]-FM6_FREQ[i])*f)>>14);
        int sh=20-octave;int32_t want=sh<=0?y:sh>31?0:y>>sh;
        assert(fm6_freq(lf)==want);
    }
    int32_t levels[6];for(uint32_t k=0;k<6;k++)levels[k]=15*16777216;
    fm6_plan(&voice,levels,31,FM6_MARK1,1);
    fm6_plan(&voice,levels,31,FM6_MODERN,1);
    for(uint32_t k=0;k<6;k++)assert(voice.g[k]==0.0f&&voice.gout[k]>1.9f&&voice.gout[k]<2.1f);
    printf("fp32 FM6: modulated operator error %.3g; exact Q24 pitch steps\n",error);
}

int main(void)
{
    fm6_numerics();
    puts("native fp32 numerical contracts passed");return 0;
}
