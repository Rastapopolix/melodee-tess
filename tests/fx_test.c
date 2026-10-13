/* SPDX-License-Identifier: GPL-3.0-only */
/* Real UI/mixer/storage integration plus deterministic DSP stress. */
#include <assert.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static void reset_fx(void)
{
    fx_clear_all();memset(fx_state,0,sizeof fx_state);
    for(uint32_t k=0;k<FX_STATE_COUNT;k++)resource_release(RES_FX0+k);
    memset(trk,0,sizeof trk);spread_on=0;trk[0].p[P_FXRATE]=60;trk[0].p[P_FXCHAR]=64;
}
static uint64_t fx_signal(uint32_t type,int rate,int character,int mix,const char *demo)
{
    reset_fx();track_t *t=&trk[0];t->p[P_FXTYPE]=(int16_t)type;t->p[P_FXRATE]=(int16_t)rate;
    t->p[P_FXCHAR]=(int16_t)character;t->p[P_FXMIX]=(int16_t)mix;
    uint32_t phase=0;uint64_t h=1469598103934665603ull,energy=0;int32_t b[CTL];
    FILE *w=demo?fopen(demo,"wb"):0;if(demo)assert(w);if(w)wav_hdr(w,2u*FS/CTL*CTL);
    for(uint32_t frame=0;frame<2u*FS/CTL;frame++){
        for(uint32_t i=0;i<CTL;i++){
            phase+=0x02347120u;
            b[i]=((sine_i(phase)+sine_i(phase*3u)/2)/4)*2;
            if(frame%173u==0u && !i)b[i]+=18000;
        }
        int on=fx_process(t,b,CTL);
        assert(resource[RES_FX0].size==(type==FX_ECHO && mix?8192u:type==FX_REVERB && mix?FX_REVERB_BYTES:type==FX_CHORUS && mix?FX_CHORUS_BYTES:type==FX_ROOM && mix?2048u:0u));
        for(uint32_t i=0;i<CTL;i++){
            int32_t l=on?fx_left[i]:b[i],r=on?fx_right[i]:b[i];
            assert(l>=-1000000 && l<=1000000 && r>=-1000000 && r<=1000000);
            if(type==FX_WIDTH)assert(r==b[i]);
            h=(h^(uint32_t)l)*1099511628211ull;h=(h^(uint32_t)r)*1099511628211ull;
            energy+=(uint64_t)(l<0?-l:l)+(uint64_t)(r<0?-r:r);
            if(w)wav_put(w,clamp(l,-32767,32767),clamp(r,-32767,32767));
        }
    }
    if(w)fclose(w);assert(energy>1000000u);return h;
}
static void legacy(void)
{
    static uint8_t b[BANK_STORE_SIZE];
    const char *names[]={"fun16.bin","fbkk.bin","tplf.bin","fun17.bin","fbkk17.bin","tplg.bin"};
    for(uint32_t k=0;k<6u;k++){
        char path[96];snprintf(path,sizeof path,"tests/fixtures/fx/%s",names[k]);
        FILE *f=fopen(path,"rb");assert(f);uint32_t n=(uint32_t)fread(b,1,sizeof b,f);fclose(f);
        ui_power_on();
        if(k%3u==2u){assert(tmpl_take(b,n));template_load();}
        else{
            assert(k%3u?bank_valid(b,n):proj_import(&proj_scratch,b,(int)n));
            assert(proj_scratch.t[0].p[P_E0]==17 && proj_scratch.motion.event[0].param==P_E0);
            assert(proj_scratch.recording[0].on==55 && proj_scratch.recording[0].duration==217 && proj_scratch.recording[0].owner==11);
            assert(!project_restore_runtime(&proj_scratch));if(k%3u)bank_restore(b);
            assert(TSEL->step[0].n==3 && TSEL->step[0].note[2]==67);
            if(k%3u)assert(pattern_at(1,3)->step[7].note[0]==72);
        }
        assert(TSEL->p[P_E0]==17);
        assert(TSEL->p[P_FXTYPE]==(k<3u?FX_OFF:FX_ECHO) && TSEL->p[P_FXMIX]==(k<3u?0:91));
        assert(TSEL->p[P_FXRATE]==(k<3u?60:83) && TSEL->p[P_FXCHAR]==(k<3u?64:77));
        for(uint32_t slot=1;slot<FX_SLOTS;slot++)assert(!TSEL->p[P_FXTYPE+slot*FX_PARAMS]);
    }
    puts("Frozen FUN16/FUN17, FBKK, TPLF/TPLG: notes, banks, timing, motion, engine IDs and original insert preserved: OK");
}
static void frozen_three_slots(void)
{
    static uint8_t b[BANK_STORE_SIZE];const char *names[]={"fun18.bin","fbkk18.bin","tplh.bin"};
    const int16_t chain[12]={1,80,30,71,6,95,55,65,8,105,88,99};
    for(uint32_t k=0;k<3;k++){
        char path[96];snprintf(path,sizeof path,"tests/fixtures/fx/%s",names[k]);FILE *f=fopen(path,"rb");assert(f);uint32_t n=fread(b,1,sizeof b,f);fclose(f);ui_power_on();
        if(k==2){assert(tmpl_take(b,n));template_load();}else {assert(k?bank_valid(b,n):proj_import(&proj_scratch,b,n));assert(!project_restore_runtime(&proj_scratch));}
        if(k==0){ /* Old engine ID 112 shifts, a tagged native 112 must not. */
            motion_store_t m={.count=2,.on=1,.event={{0,112,10},{0,112,(MO_TAG_FM6<<8)|80}}};
            uint32_t off=68u+NTRK*(116u+2u+NSTEP*8u)+sizeof(chain_config_t);memcpy(b+off,&m,sizeof m);uint32_t h=proj_hash(b,n-4u);memcpy(b+n-4u,&h,4);
            assert(proj_import(&proj_scratch,b,n));assert(proj_scratch.motion.event[0].param==P_E4 && proj_scratch.motion.event[1].param==112 && proj_scratch.motion.event[1].value==((MO_TAG_FM6<<8)|80));
        }
        assert(!memcmp(trk[0].p+P_FXTYPE,chain,sizeof chain));assert(!trk[0].p[P_FXSEND]);
        for(uint32_t j=0;j<3;j++)assert(!song.g[G_MFXTYPE+j*4] && song.g[G_MFXMIX+j*4]==127);
        assert(song.g[G_FXRETURN]==100);if(k!=2)assert(trk[0].step[0].note[0]==60);
    }
    puts("Frozen FUN18/FBKK18/TPLH: all three inserts preserved; send bus defaults off: OK");
}
static void send_return(void)
{
    ui_power_on();int32_t l[CTL],r[CTL];memset(mix_l,0,sizeof mix_l);memset(mix_r,0,sizeof mix_r);memset(fx_send_l,0,sizeof fx_send_l);memset(fx_send_r,0,sizeof fx_send_r);
    mix_add(4000,-3000,0,65536);mix_add(2000,6000,0,32768);assert(mix_l[0]==6000 && mix_r[0]==3000 && fx_send_l[0]==5000 && fx_send_r[0]==0);
    fx_bus_start(CTL);fx_bus_join();assert(!fx_bus_result); /* no doubled dry when OFF */
    song.g[G_MFXTYPE]=FX_REVERB;for(uint32_t frame=0;frame<400;frame++){
        for(uint32_t i=0;i<CTL;i++){fx_send_l[i]=frame<20?6000:0;fx_send_r[i]=frame<20?-5000:0;}
        fx_bus_start(CTL);fx_bus_join();assert(fx_bus_result);memcpy(l,fx_left,sizeof l);memcpy(r,fx_right,sizeof r);
    }
    assert(resource[RES_FX0+NPART*FX_SLOTS].size==4096 && !fx_state[NPART*FX_SLOTS].failed);
    uint64_t energy=0;for(uint32_t i=0;i<CTL;i++)energy+=abs(l[i])+abs(r[i]);assert(energy>10);
    song.g[G_MFXTYPE]=FX_ECHO;song.g[G_MFX2TYPE]=FX_REVERB;song.g[G_MFX3TYPE]=FX_ROOM;
    for(uint32_t frame=0;frame<64;frame++){fx_bus_start(CTL);fx_bus_join();for(uint32_t j=0;j<3;j++)if(fx_bus_plan[j].active && fx_bus_plan[j].f->type>=FX_ECHO)assert(fx_bus_plan[j].mem==resource[RES_FX0+NPART*FX_SLOTS+j].ptr);}
    assert(fx_state[NPART*FX_SLOTS].mix==32768); /* fully wet is exact on a send */
    song.g[G_MFXTYPE]=FX_REVERB;song.g[G_MFX2TYPE]=song.g[G_MFX3TYPE]=FX_OFF;
    trk[0].p[P_FXSEND]=81;apply_preset_to(&trk[0],0);assert(trk[0].p[P_FXSEND]==81);sound_init_of(TSEL,eng_idx(TSEL->eng_req));assert(trk[0].p[P_FXSEND]==81);
    song.g[G_FXRETURN]=73;project_t q;project_capture(&q);project_store_t wire;assert(proj_pack(&wire,&q) && proj_import(&proj_scratch,&wire,sizeof wire));assert(proj_scratch.g[G_FXRETURN]==73 && proj_scratch.g[G_MFXTYPE]==FX_REVERB && proj_scratch.t[0].p[P_FXSEND]==81);
    template_save();memset(song.g+G_MFXTYPE,0,(G_COUNT-G_MFXTYPE)*sizeof *song.g);trk[0].p[P_FXSEND]=0;template_load();assert(song.g[G_FXRETURN]==73 && song.g[G_MFXTYPE]==FX_REVERB && trk[0].p[P_FXSEND]==81);
    go_title("FX SEND");turn(EN_K1,1);assert(trk[0].p[P_FXSEND]>81);turn(EN_K2,1);assert(song.g[G_FXRETURN]>73);
    wire.raw[PROJ_FX_WIRE_OFF+3u]=128;uint32_t hash=proj_hash(wire.raw,sizeof wire-4u);memcpy(wire.raw+sizeof wire-4u,&hash,4);assert(!proj_import(&proj_scratch,&wire,sizeof wire));
    /* OFF and a type change fade the parallel return to silence, while an
     * insert bypass stays dry. RETURN edits also ramp rather than step. */
    song.g[G_MFXTYPE]=FX_REVERB;song.g[G_MFXMIX]=127;song.g[G_FXRETURN]=127;
    memset(mix_l,0,sizeof mix_l);memset(mix_r,0,sizeof mix_r);int32_t out[CTL*2];
    for(uint32_t k=0;k<24;k++){fx_bus_start(CTL);fx_bus_join();master_samples(out,CTL);}assert(fx_bus_mix==32768);
    song.g[G_MFXTYPE]=0;for(uint32_t k=0;k<24;k++){fx_bus_start(CTL);fx_bus_join();master_samples(out,CTL);}
    assert(!fx_bus_target && !fx_bus_mix && !fx_bus_result);
    song.g[G_MFXTYPE]=FX_REVERB;for(uint32_t k=0;k<16;k++){fx_bus_start(CTL);fx_bus_join();master_samples(out,CTL);}assert(fx_bus_mix==32768);
    song.g[G_MFXTYPE]=FX_ECHO;fx_bus_start(CTL);fx_bus_join();assert(!fx_bus_target);master_samples(out,CTL);assert(fx_bus_mix==32768-CTL*128);
    song.g[G_FXRETURN]=0;for(uint32_t k=0;k<24;k++){fx_bus_start(CTL);fx_bus_join();master_samples(out,CTL);}assert(!fx_bus_mix);
    puts("Shared send/return: post-fader sum, no dry doubling when OFF, one 4KiB tank, preset independence, UI and project/template persistence: OK");
}
static void separate_sends(void)
{
    /* Frozen records are written by the preceding firmware, not this decoder. */
    uint8_t b[PROJ_STORE_SIZE];
    const char *paths[]={"tests/fixtures/fun19-send-chain.bin","tests/fixtures/tpli-send-chain.bin"};
    for(uint32_t k=0;k<2;k++){
        FILE *f=fopen(paths[k],"rb");assert(f);uint32_t n=fread(b,1,sizeof b,f);fclose(f);ui_power_on();
        if(k){assert(tmpl_take(b,n));template_load();}else{assert(proj_import(&proj_scratch,b,n));assert(!project_restore_runtime(&proj_scratch));}
        assert(!song.g[G_FXMODE] && TSEL->p[P_FXSEND]==81 && !TSEL->p[P_FXSEND2] && !TSEL->p[P_FXSEND3]);
        assert(TSEL->p[P_E3]==clamp(42,track_desc(TSEL,P_E3)->min,track_desc(TSEL,P_E3)->max) && song.g[G_FXRETURN]==73 && song.g[G_MFXTYPE]==FX_REVERB);
    }
    ui_power_on();fx_bus_mix=0;song.g[G_FXMODE]=1;fx_bus_select();fx_send_on=1;
    memset(mix_l,0,sizeof mix_l);memset(mix_r,0,sizeof mix_r);
    memset(fx_send_l,0,sizeof fx_send_l);memset(fx_send_r,0,sizeof fx_send_r);
    memset(fx_send_extra_l,0,sizeof fx_send_extra_l);memset(fx_send_extra_r,0,sizeof fx_send_extra_r);
    memset(fx_bus_sum_l,0,sizeof fx_bus_sum_l);memset(fx_bus_sum_r,0,sizeof fx_bus_sum_r);
    fx_extra_gain[0]=32768;fx_extra_gain[1]=65536;mix_add(4000,-3000,0,0);mix_extra_sends(CTL);
    assert(mix_l[0]==4000 && mix_r[0]==-3000 && !fx_send_l[0] && !fx_send_r[0]);
    assert(fx_send_extra_l[0][0]==2000 && fx_send_extra_r[0][0]==-1500 && fx_send_extra_l[1][0]==4000 && fx_send_extra_r[1][0]==-3000);
    /* Each slot must see its own input and return exactly once, even with an
     * inactive middle slot. Compare with separately rendered reference outputs. */
    int32_t expected_l[CTL]={0},expected_r[CTL]={0};fx_state_t before[FX_SLOTS];
    for(uint32_t slot=0;slot<FX_SLOTS;slot++){
        song.g[G_MFXTYPE+slot*FX_PARAMS]=slot==1?FX_OFF:FX_PHASER;
        int32_t *l=slot?fx_send_extra_l[slot-1]:fx_send_l,*r=slot?fx_send_extra_r[slot-1]:fx_send_r;
        for(uint32_t i=0;i<CTL;i++){l[i]=(int32_t)(i*97+slot*303);r[i]=-(int32_t)(i*53+slot*197);}
        fx_slot_prepare(&fx_bus_plan[slot],NPART*FX_SLOTS+slot,song.g+G_MFXTYPE+slot*FX_PARAMS);
        before[slot]=fx_state[NPART*FX_SLOTS+slot];
        if(!fx_bus_plan[slot].active)continue;
        memcpy(fx_left,l,sizeof fx_left);memcpy(fx_right,r,sizeof fx_right);assert(fx_slot_run(&fx_bus_plan[slot],CTL));
        for(uint32_t i=0;i<CTL;i++){expected_l[i]+=fx_scale(fx_left[i],(i+1u)*256u);expected_r[i]+=fx_scale(fx_right[i],(i+1u)*256u);}
    }
    for(uint32_t slot=0;slot<FX_SLOTS;slot++)fx_state[NPART*FX_SLOTS+slot]=before[slot];
    fx_bus_start(CTL);fx_bus_join();assert(fx_bus_result);
    assert(!memcmp(expected_l,fx_left,sizeof expected_l) && !memcmp(expected_r,fx_right,sizeof expected_r));
    /* The real mixer routes only this track to slot 2 after volume and pan. */
    ui_power_on();fx_bus_mix=0;song.g[G_FXMODE]=1;song.g[G_MFX2TYPE]=FX_PHASER;
    host_preset(TSEL,ENGI_FM6,8);TSEL->p[P_SUS]=127;TSEL->p[P_LEVEL]=127;TSEL->p[P_PAN]=-64;
    TSEL->p[P_FXSEND2]=127;trk_note_on(TSEL,60,100);int32_t probe[CTL*2];uint64_t send_energy=0;
    for(uint32_t k=0;k<32;k++){
        mix_block(probe,CTL);
        for(uint32_t i=0;i<CTL;i++){
            assert(!fx_send_l[i] && !fx_send_r[i] && !fx_send_extra_r[0][i] && !fx_send_extra_l[1][i] && !fx_send_extra_r[1][i]);
            send_energy+=abs(fx_send_extra_l[0][i]);assert(probe[2*i]>=-32767 && probe[2*i]<=32767);
        }
    }
    assert(send_energy>1000);TSEL->p[P_LEVEL]=0;
    mix_block(probe,CTL);
    for(uint32_t i=0;i<CTL;i++)assert(!fx_send_extra_l[0][i]);
    TSEL->p[P_LEVEL]=127;
    TSEL->p[P_FXSEND]=20;TSEL->p[P_FXSEND2]=71;TSEL->p[P_FXSEND3]=99;
    apply_preset_to(TSEL,0);sound_init_of(TSEL,eng_idx(TSEL->eng_req));
    assert(TSEL->p[P_FXSEND]==20 && TSEL->p[P_FXSEND2]==71 && TSEL->p[P_FXSEND3]==99);
    project_t q;project_store_t wire;project_capture(&q);assert(proj_pack(&wire,&q) && proj_import(&proj_scratch,&wire,sizeof wire));
    assert(proj_scratch.g[G_FXMODE]==1 && proj_scratch.t[0].p[P_FXSEND2]==71 && proj_scratch.t[0].p[P_FXSEND3]==99);
    template_save();song.g[G_FXMODE]=0;TSEL->p[P_FXSEND2]=TSEL->p[P_FXSEND3]=0;template_load();
    assert(song.g[G_FXMODE]==1 && TSEL->p[P_FXSEND2]==71 && TSEL->p[P_FXSEND3]==99);
    go_title("FX SENDS");turn(EN_K2,1);turn(EN_K3,-1);assert(TSEL->p[P_FXSEND2]>71 && TSEL->p[P_FXSEND3]<99);
    go_title("FX SEND");turn(EN_K3,-1);assert(!song.g[G_FXMODE]);
    uint32_t inserts_epoch=fx_state[0].epoch;fx_bus_mix=32768;fx_bus_select();assert(fx_bus_separate);
    int32_t out[CTL*2];for(uint32_t k=0;k<32;k++){fx_bus_start(CTL);fx_bus_join();master_samples(out,CTL);}
    assert(!fx_bus_separate && fx_state[0].epoch==inserts_epoch && TSEL->p[P_FXSEND2]>71);
    wire.raw[PROJ_FX_MODE_OFF]=2;uint32_t h=proj_hash(wire.raw,sizeof wire-4u);memcpy(wire.raw+sizeof wire-4u,&h,4);assert(!proj_import(&proj_scratch,&wire,sizeof wire));
    ui_power_on();fx_bus_mix=0;fx_bus_select();
    puts("Separate sends: independent inputs/returns, inactive-slot silence, routing fade, old chain imports, UI and project/template persistence: OK");
}
static void fx_chain_test(void)
{
    ui_power_on();track_t *t=&trk[0];
    int16_t settings[FX_SLOTS*FX_PARAMS]={FX_PHASER,80,30,71,FX_ECHO,95,55,65,FX_REVERB,105,88,99};
    memcpy(t->p+P_FXTYPE,settings,sizeof settings);
    int32_t b[CTL];uint64_t energy=0;
    for(uint32_t frame=0;frame<1600;frame++){
        for(uint32_t i=0;i<CTL;i++)b[i]=frame<80?8000*sine_i((frame*CTL+i)*12345678u)/32768:0;
        assert(fx_process(t,b,CTL));
        for(uint32_t i=0;i<CTL;i++)if(frame>80)energy+=abs(fx_left[i])+abs(fx_right[i]);
    }
    assert(energy>10000 && fx_state[0].type==FX_PHASER && fx_state[1].type==FX_ECHO && fx_state[2].type==FX_REVERB);
    assert(!resource[RES_FX0].size && resource[RES_FX0+1].size==8192 && resource[RES_FX0+2].size==FX_REVERB_BYTES);
    uint32_t pos=fx_state[2].rev_pos[0];void *tank=resource[RES_FX0+2].ptr;
    apply_preset_to(t,2);assert(!memcmp(t->p+P_FXTYPE,settings,sizeof settings));
    undo_swap();assert(!memcmp(t->p+P_FXTYPE,settings,sizeof settings));
    set_engine_of(t,ENGI_FM6);apply_preset_to(t,8);assert(!memcmp(t->p+P_FXTYPE,settings,sizeof settings));
    memset(b,0,sizeof b);fx_process(t,b,CTL);
    assert(resource[RES_FX0+2].ptr==tank && fx_state[2].rev_pos[0]!=pos);
    int rc=up_store(0,"CHAIN");assert(rc==0 || rc==3);
    t->p[P_FX2CHAR]=19;assert(!up_load(0));assert(t->p[P_FX2CHAR]==19);t->p[P_FX2CHAR]=65;
    project_t q;project_store_t p;project_capture(&q);assert(proj_pack(&p,&q));
    assert(proj_import(&proj_scratch,&p,sizeof p));assert(!memcmp(proj_scratch.t[0].p+P_FXTYPE,settings,sizeof settings));
    template_save();memset(t->p+P_FXTYPE,0,sizeof settings);template_load();assert(!memcmp(t->p+P_FXTYPE,settings,sizeof settings));
    sound_init_of(t,t->eng_req);assert(!memcmp(t->p+P_FXTYPE,settings,sizeof settings));
    assert(!native_store(ENGI_FM6,1,0,"FX FREE") || native_used(ENGI_FM6,1));
    assert(!native_load(ENGI_FM6,1,0));assert(!memcmp(t->p+P_FXTYPE,settings,sizeof settings));
    press(B_FX);assert(cur_page()->id[0]==P_FXTYPE);turn(EN_SELECT,1);assert(cur_page()->id[0]==P_FX2TYPE);
    turn(EN_SELECT,1);assert(cur_page()->id[0]==P_FX3TYPE);
    t->p[P_FX2MIX]=0;for(uint32_t k=0;k<24;k++)fx_process(t,b,CTL);
    assert(!resource[RES_FX0+1].size && resource[RES_FX0+2].size==FX_REVERB_BYTES);
    puts("Three serial slots: independent bypass, live tails, preset/engine/undo/user independence, project/template and page navigation: OK");
}
static void reverb_tail(void)
{
    uint64_t early[2]={0},late[2]={0};
    for(uint32_t setting=0;setting<2;setting++){
        reset_fx();track_t *t=&trk[0];t->p[P_FXTYPE]=FX_REVERB;t->p[P_FXMIX]=127;t->p[P_FXRATE]=85;t->p[P_FXCHAR]=setting?127:35;
        int32_t b[CTL]={0};fx_process(t,b,CTL);fx_state[0].mix=127*256;
        uint64_t diff=0;uint32_t dense=0,count=0;
        for(uint32_t frame=0;frame<12u*FS/CTL;frame++){
            memset(b,0,sizeof b);if(!frame)for(uint32_t i=0;i<CTL;i++)b[i]=18000;
            fx_process(t,b,CTL);
            for(uint32_t i=0;i<CTL;i++){
                uint32_t sample=frame*CTL+i;int32_t l=fx_left[i],r=fx_right[i];
                assert(abs(l)<32768 && abs(r)<32768);
                if(sample>FS/10 && sample<FS/4){dense+=abs(l)+abs(r)>2;count++;diff+=abs(l-r);}
                if(sample>FS/10 && sample<FS/2)early[setting]+=(int64_t)l*l+(int64_t)r*r;
                if(sample>FS/2 && sample<FS)late[setting]+=(int64_t)l*l+(int64_t)r*r;
                if(sample>11u*FS)assert(abs(l)+abs(r)<=2);
            }
        }
        assert(dense>count*9u/10u && diff>10000 && early[setting]>late[setting]);
    }
    assert(late[1]>late[0]*8u);
    reset_fx();int32_t b[CTL]={0};
    for(uint32_t slot=0;slot<FX_SLOTS;slot++){
        uint32_t base=P_FXTYPE+slot*FX_PARAMS;
        trk[0].p[base]=FX_REVERB;trk[0].p[base+1]=100;trk[0].p[base+2]=80;trk[0].p[base+3]=90;
    }
    b[0]=20000;assert(fx_process(&trk[0],b,CTL));
    for(uint32_t slot=0;slot<FX_SLOTS;slot++)assert(resource[RES_FX0+slot].size==FX_REVERB_BYTES && !fx_state[slot].failed);
    memset(b,0,sizeof b);for(uint32_t k=0;k<2000;k++)assert(fx_process(&trk[0],b,CTL));
    puts("Reverb: dense stereo reflections, controllable decay, bounded response and silence after the tail: OK");
}
static void reverb_presence(void)
{
    uint64_t deep_tail=0;
    const uint32_t hz[]={110,220,440,880,1760};
    for(uint32_t h=0;h<NELEM(hz);h++){
        reset_fx();track_t *t=&trk[0];t->p[P_FXTYPE]=FX_REVERB;t->p[P_FXMIX]=127;t->p[P_FXRATE]=100;t->p[P_FXCHAR]=100;
        int32_t b[CTL]={0};fx_process(t,b,CTL);fx_state[0].mix=127*256;
        uint32_t phase=0,inc=(uint32_t)((uint64_t)hz[h]*0x100000000ull/FS);uint64_t dry=0,wet=0;
        for(uint32_t frame=0;frame<4u*FS/CTL;frame++){
            for(uint32_t i=0;i<CTL;i++){
                phase+=inc;b[i]=frame<2u*FS/CTL?8000*sine_i(phase)/32768:0;
                if(frame>=FS/CTL && frame<2u*FS/CTL)dry+=(int64_t)b[i]*b[i];
            }
            fx_process(t,b,CTL);
            for(uint32_t i=0;i<CTL;i++){
                int32_t l=fx_left[i],r=fx_right[i];assert(abs(l)<32768 && abs(r)<32768);
                uint64_t e=((int64_t)l*l+(int64_t)r*r)/2;
                if(frame>=FS/CTL && frame<2u*FS/CTL)wet+=e;
                if(h==1u && frame>=3u*FS/CTL)deep_tail+=e;
            }
        }
        /* Previously the 220..1760 Hz return was 8..22 dB below the dry tone.
         * Keep musical mids present, without permitting hot bass to clip it. */
        assert(h==4u?wet>dry/8u:wet>dry/4u);
    }
    assert(deep_tail>100000u);
    for(uint32_t slot=0;slot<FX_SLOTS;slot++){
        uint32_t base=P_FXTYPE+slot*FX_PARAMS;trk[0].p[base]=FX_REVERB;
        assert(str_eq(track_desc(&trk[0],base+3u)->label,"DEPTH"));
    }
    puts("Reverb: present bass/mids, bounded hot wet peaks, sustained deep tail and DEPTH in every slot: OK");
}
static void reverb_rate_conversion(void)
{
    const int32_t weight[15]={1,2,3,4,5,6,7,8,7,6,5,4,3,2,1};
    for(uint32_t mode=0;mode<3;mode++){
        fx_state_t f={0};int32_t history[15]={0},reference=0,rng=1;
        for(uint32_t sample=0;sample<250000u;sample++){
            int32_t x=mode==0?1000000:mode==1?-1000000:(int32_t)(noise32(&rng)%2000001u)-1000000;
            for(uint32_t k=14;k;k--)history[k]=history[k-1];history[0]=x;
            if(!f.decim){
                int32_t sum=0;for(uint32_t k=0;k<15;k++)sum+=weight[k]*history[k];
                reference=(reference+7*(sum/64))/8;
            }
            /* Compare to a finite FIR reference through repeated 32-bit wraps,
             * including hot DC and signed random inputs from previous slots. */
            assert(fx_reverb_input(&f,x)==reference);
            f.decim=(f.decim+1u)&7u;
        }
    }
    const uint32_t hz[]={1000,8000,11025};uint64_t energy[3]={0};
    for(uint32_t k=0;k<3;k++){
        fx_state_t f={0};uint32_t phase=0,inc=(uint32_t)((uint64_t)hz[k]*0x100000000ull/FS);
        for(uint32_t sample=0;sample<FS;sample++){
            phase+=inc;int32_t y=fx_reverb_input(&f,16000*sine_i(phase)/32768);
            if(!f.decim && sample>FS/4u)energy[k]+=(int64_t)y*y;
            f.decim=(f.decim+1u)&7u;
        }
    }
    assert(energy[0]>100000000u && energy[1]<energy[0]/60u && energy[2]<100000u);
    puts("Reverb rate conversion: exact FIR reference through integrator wraps, preserved mids and alias-band rejection: OK");
}
static void rate_and_pan(void)
{
    /* Cached block conversion must match the separately verified FIR model,
     * including arbitrary block boundaries, stereo differences and wraps. */
    fx_state_t actual={0},reference[2]={{0}};actual.type=FX_TAPE;int32_t rng=13;
    for(uint32_t block=0;block<10000u;block++){
        uint32_t n=1u+block%CTL,phase=actual.decim,count=0;int32_t expected[2][CTL/4u+1u];
        for(uint32_t i=0;i<n;i++){
            fx_left[i]=(int32_t)(noise32(&rng)%2000001u)-1000000;fx_right[i]=-fx_left[i];
            int32_t l=fx_filter_input(&reference[0],0,fx_left[i],phase),r=fx_filter_input(&reference[1],0,fx_right[i],phase);
            if(!phase){expected[0][count]=l;expected[1][count++]=r;}phase=(phase+1u)&3u;
            fx_dry_l[i]=fx_dry_r[i]=0;
        }
        assert(fx_down(&actual,n)==count);
        for(uint32_t i=0;i<count;i++)assert(fx_low_l[i]==expected[0][i] && fx_low_r[i]==expected[1][i]);
        fx_up(&actual,n,0);assert(actual.decim==phase);
        for(uint32_t ch=0;ch<2u;ch++)for(uint32_t k=0;k<4u;k++)assert(actual.rev_damp[ch*4u+k]==reference[ch].rev_damp[k]);
    }
    for(uint32_t shift=1;shift<16u;shift++)for(uint32_t i=0;i<5000u;i++){
        int32_t x=(int32_t)noise32(&rng);assert(fx_div_pow2(x,shift)==x/(int32_t)(1u<<shift));
    }
    for(uint32_t type=FX_PHASER;type<=FX_SPIT;type++){
        reset_fx();track_t *t=&trk[0];t->p[P_FXTYPE]=type;t->p[P_FXMIX]=127;t->p[P_PAN]=1;
        int32_t b[CTL];for(uint32_t i=0;i<CTL;i++)b[i]=4000;
        for(uint32_t k=0;k<50u;k++)fx_process(t,b,CTL);
        assert(type==FX_WIDTH || fx_state[0].mono); /* PAN alone adds no stereo filter work */
        t->p[P_PAN]=63;fx_process(t,b,CTL);for(uint32_t i=0;i<CTL;i++)assert(type==FX_WIDTH?fx_right[i]==4000:fx_left[i]==fx_scale(fx_right[i],1024u));
        t->p[P_PAN]=-64;fx_process(t,b,CTL);for(uint32_t i=0;i<CTL;i++)assert(!fx_right[i]);
        t->p[P_PAN]=0;spread_on=1;t->p[P_SPRD]=127;
        for(uint32_t i=0;i<CTL;i++)spread_buf[i]=i&1u?-3000:3000;
        fx_process(t,b,CTL);assert(!fx_state[0].mono);
    }
    puts("FX performance safety: cached stereo conversion, arbitrary blocks/wraps, exact shifts, mono pan and genuine spread: OK");
}
static void tape_smooth(void)
{
    for(uint32_t hz=0;hz<=220;hz+=220)for(int rate=60;rate<=127;rate+=67){
        reset_fx();track_t *t=&trk[0];t->p[P_FXTYPE]=FX_TAPE;t->p[P_FXMIX]=127;t->p[P_FXRATE]=rate;t->p[P_FXCHAR]=127;
        int32_t b[CTL]={0};fx_process(t,b,CTL);fx_state[0].mix=127*256;
        uint32_t phase=0,inc=(uint32_t)((uint64_t)hz*0x100000000ull/FS),jump=0;int32_t prev=0;
        for(uint32_t frame=0;frame<6u*FS/CTL;frame++){
            for(uint32_t i=0;i<CTL;i++){phase+=inc;b[i]=hz?8000*sine_i(phase)/32768:8000;}
            if(frame>FS/CTL && frame%401u==0u)t->p[P_FXCHAR]=t->p[P_FXCHAR]?0:127;
            fx_process(t,b,CTL);
            for(uint32_t i=0;i<CTL;i++){
                int32_t y=fx_left[i];uint32_t delta=(uint32_t)abs(y-prev);prev=y;
                if(frame>FS/CTL && delta>jump)jump=delta;
                assert(abs(y)<24000 && fx_left[i]==fx_right[i]);
            }
        }
        /* A steady input used to jump by ~1000 samples when random targets
         * changed; a 220 Hz tone jumped by ~1500..2000 instead of its 251. */
        assert(jump<=(hz?600u:2u));assert(!resource[RES_FX0].size);
        t->p[P_FXCHAR]=0;fx_process(t,b,CTL);t->p[P_FXCHAR]=127;fx_process(t,b,CTL);
        for(uint32_t k=0;k<4000;k++){memset(b,0,sizeof b);fx_process(t,b,CTL);}
        for(uint32_t ch=0;ch<2;ch++)for(uint32_t k=0;k<3;k++)assert(!fx_state[0].q[ch][k].z1 && !fx_state[0].q[ch][k].z2);
    }
    puts("Tape: maximum jitter/rate have smooth phase/filter transitions, bounded stereo output, no allocation and no denormal tail: OK");
}
static void chorus_test(void)
{
    _Static_assert(FX_REVERB==8 && FX_CHORUS==9,"stored FX IDs stay fixed");
    /* A fully wet, depth-zero chorus preserves upper audio frequencies and
     * independent stereo channels; unlike the other time FX it is full-rate. */
    fx_state_t f={.type=FX_CHORUS,.rev_l=256*65536,.rev_r=256*65536,.mix=32768};
    int16_t mem[FX_CHORUS_BYTES/2u]={0};int32_t step[2]={0},rng=31;
    uint64_t dry=0,wet=0;
    for(uint32_t block=0;block<3000u;block++){
        uint32_t n=1u+block%CTL;
        for(uint32_t i=0;i<n;i++){
            int32_t x=8000*sine_i((block*CTL+i)*0x2e709de5u)/32768;
            fx_left[i]=fx_dry_l[i]=x;fx_right[i]=fx_dry_r[i]=-x;
            dry+=(int64_t)x*x;
        }
        fx_chorus(&f,mem,n,step,32768);
        for(uint32_t i=0;i<n;i++){assert(abs(fx_left[i]+fx_right[i])<=4);wet+=(int64_t)fx_left[i]*fx_left[i];}
    }
    assert(wet>dry*98u/100u && wet<dry*102u/100u);
    /* Hot output from preceding slots stays bounded, and no feedback means
     * silence becomes exact within one ring, even after repeated wraps. */
    for(uint32_t block=0;block<1000;block++){
        for(uint32_t i=0;i<CTL;i++){fx_left[i]=fx_dry_l[i]=(int32_t)(noise32(&rng)%2000001u)-1000000;fx_right[i]=fx_dry_r[i]=-fx_left[i];}
        fx_chorus(&f,mem,CTL,step,32768);
        for(uint32_t i=0;i<CTL;i++)assert(abs(fx_left[i])<=131068 && abs(fx_right[i])<=131068);
    }
    for(uint32_t block=0;block<512u/CTL+2u;block++){
        memset(fx_left,0,sizeof fx_left);memset(fx_right,0,sizeof fx_right);fx_chorus(&f,mem,CTL,step,32768);
    }
    for(uint32_t i=0;i<CTL;i++)assert(!fx_left[i] && !fx_right[i]);
    ui_power_on();
    for(uint32_t slot=0;slot<FX_SLOTS;slot++){
        uint32_t b=P_FXTYPE+slot*FX_PARAMS,g=G_MFXTYPE+slot*FX_PARAMS;
        trk[0].p[b]=song.g[g]=FX_CHORUS;trk[0].p[b+1]=100;
        assert(str_eq(track_desc(&trk[0],b+2)->label,"RATE") && track_desc(&trk[0],b+2)->fmt==F_LFOHZ);
        assert(str_eq(track_desc(&trk[0],b+3)->label,"DEPTH"));
        assert(str_eq(global_desc(g+2)->label,"RATE") && global_desc(g+2)->fmt==F_LFOHZ);
        assert(str_eq(global_desc(g+3)->label,"DEPTH"));
    }
    project_t q;project_store_t p;project_capture(&q);assert(proj_pack(&p,&q) && proj_import(&proj_scratch,&p,sizeof p));
    template_save();memset(trk[0].p+P_FXTYPE,0,FX_SLOTS*FX_PARAMS*sizeof(int16_t));memset(song.g+G_MFXTYPE,0,FX_SLOTS*FX_PARAMS*sizeof(int16_t));template_load();
    int32_t b[CTL]={0};assert(fx_process(&trk[0],b,CTL));fx_bus_start(CTL);fx_bus_join();assert(fx_bus_result);
    for(uint32_t slot=0;slot<FX_SLOTS;slot++){
        assert(proj_scratch.t[0].p[P_FXTYPE+slot*FX_PARAMS]==FX_CHORUS && proj_scratch.g[G_MFXTYPE+slot*FX_PARAMS]==FX_CHORUS);
        assert(resource[RES_FX0+slot].size==FX_CHORUS_BYTES && resource[RES_FX0+NPART*FX_SLOTS+slot].size==FX_CHORUS_BYTES);
        trk[0].p[P_FXTYPE+slot*FX_PARAMS]=song.g[G_MFXTYPE+slot*FX_PARAMS]=FX_OFF;
    }
    for(uint32_t k=0;k<24;k++){fx_process(&trk[0],b,CTL);fx_bus_start(CTL);fx_bus_join();}
    for(uint32_t slot=0;slot<FX_SLOTS;slot++)assert(!resource[RES_FX0+slot].size && !resource[RES_FX0+NPART*FX_SLOTS+slot].size);
    puts("Chorus: full-band independent stereo, bounded hot inputs, exact silent tail, 2 KiB/slot, RATE/DEPTH, insert/send persistence and bypass release: OK");
}
int main(int argc,char **argv)
{
    legacy();frozen_three_slots();send_return();separate_sends();fx_chain_test();reverb_tail();reverb_presence();reverb_rate_conversion();rate_and_pan();tape_smooth();chorus_test();
    uint64_t dry=fx_signal(FX_OFF,60,64,0,0);
    for(uint32_t mode=FX_PHASER;mode<FX_COUNT;mode++){
        assert(fx_signal(mode,60,64,0,0)==dry);
        uint64_t a=fx_signal(mode,60,64,127,0);assert(a!=dry && a==fx_signal(mode,60,64,127,0));
        assert(a!=fx_signal(mode,127,64,127,0));assert(a!=fx_signal(mode,60,127,127,0));
        for(int rate=0;rate<=127;rate+=127)for(int cha=0;cha<=127;cha+=127)fx_signal(mode,rate,cha,127,0);
        if(argc>1){char path[256];snprintf(path,sizeof path,"%s/%s.wav",argv[1],N_FX[mode]);fx_signal(mode,60,64,100,path);}
        printf("%s: deterministic, audible controls, bounded extremes, zero-mix bypass: OK\n",N_FX[mode]);
    }
    /* Time tails survive voice completion, bypass releases their allocation. */
    for(uint32_t mode=FX_ECHO;mode<=FX_ROOM;mode++){
        reset_fx();track_t *t=&trk[0];t->p[P_FXTYPE]=(int16_t)mode;t->p[P_FXMIX]=127;t->p[P_FXRATE]=0;
        t->p[P_FXCHAR]=80;fx_state[0].mix=32768;int32_t b[CTL]={20000};
        assert(fx_process(t,b,CTL));uint64_t tail=0;
        for(uint32_t k=0;k<1500;k++){memset(b,0,sizeof b);assert(fx_process(t,b,CTL));for(uint32_t i=0;i<CTL;i++)tail+=(uint32_t)abs(fx_left[i]);}
        assert(tail>1000);t->p[P_FXMIX]=0;
        for(uint32_t k=0;k<16;k++)fx_process(t,b,CTL);assert(!resource[RES_FX0].size);
    }
    /* Exhaust all usable banks: fail dry, with no per-block allocation retry. */
    reset_fx();for(uint32_t id=0;id<RES_COUNT;id++)resource_release(id);
    for(uint32_t id=0;id<RES_FX0;id++)assert(id==4u || resource_get(id,id==0?163840u:id==1?20480u:id==2?4096u:2048u));
    int32_t b[CTL]={0};
    for(uint32_t mode=0;mode<2;mode++){
        reset_fx();trk[0].p[P_FXTYPE]=mode?FX_CHORUS:FX_ECHO;trk[0].p[P_FXMIX]=127;
        assert(!fx_process(&trk[0],b,CTL));uint32_t fail=resource_failures;assert(fx_state[0].failed);
        for(uint32_t k=0;k<100;k++)assert(!fx_process(&trk[0],b,CTL));assert(resource_failures==fail);
    }
    for(uint32_t id=0;id<RES_COUNT;id++)resource_release(id);
    ui_power_on();press(B_FX);assert(!ui.home && cur_page()->fam==FAM_FX && !ui.layer);
    turn(EN_K1,2);assert(TSEL->p[P_FXTYPE]==FX_WIDTH);turn(EN_K2,50);assert(TSEL->p[P_FXMIX]>0);
    project_t q;project_capture(&q);project_store_t p;assert(proj_pack(&p,&q));
    assert(proj_import(&proj_scratch,&p,sizeof p));assert(proj_scratch.t[0].p[P_FXTYPE]==FX_WIDTH);
    frames(400);press(B_OCTUP);assert(pop.on==POP_SHEET);press(B_OCTDN);assert(!pop.on);
    press(B_OCTDN);assert(ui.home);
    /* Four live tracks through the actual mixer, then mute a sounding insert. */
    ui_power_on();int32_t out[CTL*2];
    for(uint32_t k=0;k<NPART;k++){
        set_engine_of(&trk[k],ENGI_FM6);apply_preset_to(&trk[k],8);
        trk[k].p[P_FXTYPE]=(int16_t)(FX_PHASER+k);trk[k].p[P_FXMIX]=100;trk_note_on(&trk[k],48+k*7u,100);
    }
    for(uint32_t k=0;k<256u;k++){
        mix_block(out,CTL);for(uint32_t i=0;i<CTL*2u;i++)assert(out[i]>=-32767 && out[i]<=32767);
    }
    for(uint32_t k=0;k<NPART;k++)trk[k].p[P_MUTE]=1;
    for(uint32_t k=0;k<2048u;k++)mix_block(out,CTL);
    for(uint32_t i=0;i<CTL*2u;i++)assert(abs(out[i])<=1);
    puts("FX: tails, allocator exhaustion/recovery, FX button/knobs/Esc and project round trip: OK");return 0;
}
