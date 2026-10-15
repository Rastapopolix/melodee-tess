/* SPDX-License-Identifier: GPL-3.0-only */
/* Resident profile admission, with online caps and serial host rendering. */
#define MELODEE_RAM_AUDIO 1
#define MELODEE_DUAL_CORE 1
static int audio_worker_online=1;
static int audio_worker_submit(void (*fn)(void *),void *ctx){(void)fn;(void)ctx;return 0;}
static int audio_worker_join(void){return 1;}
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>

static void fresh(void)
{
    memset(trk,0,sizeof trk);memset(&song,0,sizeof song);
    memset(resource,0,sizeof resource);
    host_tracks_init();
    memset(voice_kills_part,0,sizeof voice_kills_part);voice_kills=0;
    audio_worker_online=1;
    for(unsigned p=0;p<NPART;p++){
        host_preset(&trk[p],HOST_SYNTH,0);
        trk[p].p[P_VOICE]=V_POLY;
    }
}

int main(void)
{
    fresh();assert(voice_budget()==18u);
    for(unsigned p=0;p<NPART;p++)for(unsigned n=0;n<2u;n++)trk_note_on(&trk[p],48u+4u*n,100);
    trk_note_on(&trk[3],60,100);
    assert(voices_busy()==18u && !voice_kills);
    trk_note_on(&trk[3],67,100);
    assert(voice_kills && voices_busy()<=18u);
    for(unsigned p=0;p<NPART;p++)assert(trk[p].v[lowest_held(&trk[p])].note==48u);

    fresh();
    host_preset(&trk[0],ENGI_PROPHET,0);trk[0].p[P_VOICE]=V_POLY;
    for(unsigned n=0;n<3u;n++)trk_note_on(&trk[0],48u+4u*n,100);
    assert(voice_units(&trk[0])==2u && voices_busy()==6u);
    for(unsigned p=1;p<NPART;p++)for(unsigned n=0;n<2u;n++)trk_note_on(&trk[p],48u+4u*n,100);
    assert(voices_busy()==18u && !voice_kills_part[0]);
    audio_worker_online=0;
    assert(voice_budget()==16u && voice_units(&trk[0])==3u);
    trk_note_on(&trk[3],67,100);
    assert(voices_busy()<=16u && voice_kills);
    /* main.c clock_boot: the CPU1 budget follows the clock running (a 320 a soft reset kept); one core keeps 16 */
    audio_worker_online=1;
    vbudget_cpu1=VBUDGET*320u/360u; assert(voice_budget()==16u);
    audio_worker_online=0; assert(voice_budget()==16u);
    vbudget_cpu1=VBUDGET;
    printf("Resident voice budget: 18 units (16 at 320 MHz), bounded admission, protected bass, CPU1 fallback to 16: OK\n");
    return 0;
}
