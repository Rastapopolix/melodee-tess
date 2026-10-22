/* SPDX-License-Identifier: GPL-3.0-only */
#define MELODEE_RAM_AUDIO 1
#define MELODEE_ADMISSION 1
#define MELODEE_DUAL_CORE 1
static int audio_worker_online=1;
static int audio_worker_submit(void (*fn)(void *),void *ctx){(void)fn;(void)ctx;return 0;}
static int audio_worker_join(void){return 1;}
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
static uint32_t sounding(const track_t *t)
{uint32_t n=0;for(uint32_t i=0;i<NVOICE;i++)n+=t->v[i].active && t->v[i].stage!=4u;return n;}
static const uint8_t family[4]={ENGI_FM6,3,ENGI_DRUM,ENGI_DRUM};   /* (the Prophet and CZ-1: retired 2026-10-22) */
static void setup(const uint32_t cat[4])
{
    memset(trk,0,sizeof trk);memset(&song,0,sizeof song);eng_state_reset();
    memset(fm6_eff,0,sizeof fm6_eff);host_tracks_init();
    voice_kills=capacity_steals=resource_failures=0;settings_budget=16;
    for(uint32_t p=0;p<4;p++){uint32_t c=cat[p];host_preset(&trk[p],family[c],c==0?4:c==1?8:c==3?1:0);trk[p].p[P_VOICE]=V_POLY;}
}
int main(void)
{
    const uint32_t native[4]={16,4,8,8};uint32_t cases=0;
    for(uint32_t a=0;a<4;a++)for(uint32_t b=0;b<4;b++)for(uint32_t c=0;c<4;c++)for(uint32_t d=0;d<4;d++){
        uint32_t cat[4]={a,b,c,d},q[4],u[4],used=0;
        setup(cat);
        for(uint32_t p=0;p<4;p++){
            assert(trk_nvoice(&trk[p])==native[cat[p]]);
            u[p]=2;
            q[p]=cat[p]>=2?2:1;used+=q[p]*u[p];
        }
        assert(used<=16);
        for(int changed=1;changed;){changed=0;for(uint32_t p=0;p<4;p++)if(q[p]<native[cat[p]] && used+u[p]<=16){q[p]++;used+=u[p];changed=1;}}
        const uint8_t hits[8]={36,38,39,42,45,37,56,49};
        for(uint32_t p=0;p<4;p++)for(uint32_t n=0;n<q[p];n++)trk_note_on(&trk[p],cat[p]>=2?hits[n]:48+7*n,110);
        for(uint32_t p=0;p<4;p++){if(sounding(&trk[p])!=q[p])fprintf(stderr,"assignment %u,%u,%u,%u part %u: %u sounding vs %u, points %u, kills %u\\n",a,b,c,d,p,sounding(&trk[p]),q[p],voices_busy(),voice_kills);assert(sounding(&trk[p])==q[p]);}
        assert(!voice_kills && !resource_failures && voices_busy()<=voice_budget());cases++;
    }
    assert(cases==256);
    /* Idle configured parts cannot reserve the solo part's capacity. */
    for(uint32_t c=0;c<4;c++){
        uint32_t cat[4]={c,c,c,c};setup(cat);
        uint32_t count=c==0?8:c==1?4:3;
        const uint8_t hits[3]={36,38,42};
        for(uint32_t n=0;n<count;n++)trk_note_on(&trk[0],c>=2?hits[n]:48+7*n,110);
        assert(sounding(&trk[0])==count && !voice_kills && voices_busy()<=16);
    }
    uint32_t cat[4]={3,3,3,3};setup(cat);
    const uint8_t drum[8]={36,38,39,41,45,50,37,49};
    for(uint32_t n=0;n<8;n++)trk_note_on(&trk[0],drum[n],110);
    assert(sounding(&trk[0])==8 && !voice_kills);
    cat[0]=0;setup(cat);settings_budget=32;
    for(uint32_t n=0;n<12;n++)trk_note_on(&trk[0],36+n*3,110);
    assert(sounding(&trk[0])==12 && trk_nvoice(&trk[0])==16);
    printf("Shared admission: 256 guaranteed allocations; FM/SID polyphony, three drum hits, eight 909 instruments and adjustable pool: OK\n");
    return 0;
}
