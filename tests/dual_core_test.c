/* SPDX-License-Identifier: GPL-3.0-only */
/* Run the real FM6 pairing on a concurrent host worker, compare the complete
 * mixer to serial output. Controls and lifecycle stay on the producer. */
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>
#include <sched.h>
static uint32_t worker_clock(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000000u + ts.tv_nsec / 1000u);
}
#define MELODEE_DUAL_CORE 1
#define AW_WORD _Atomic uint32_t
#define AW_LOAD(p) atomic_load_explicit(p, memory_order_acquire)
#define AW_STORE(p,v) atomic_store_explicit(p,v,memory_order_release)
#define AW_TICKS() worker_clock()
#define AW_TIMEOUT_TICKS 2000000u
#define AW_RAM_LOOP static
#define AW_IDLE() pthread_testcancel()
/* The host cannot stop the thread mid-job: holding waits until it touches nothing more. */
static void hold_worker(void);
static uint32_t injected_faults;
#define AW_HOLD() hold_worker()
#define AW_FAULTS() injected_faults
#include "../firmware/src/audio_worker.h"
static void hold_worker(void)
{
    while (AW_LOAD(&audio_worker.complete) != AW_LOAD(&audio_worker.request)) sched_yield();
}
/* Reference rendering keeps the same online allocator/caps as the parallel
 * run, but refuses submissions so the actual kernel executes on CPU0. */
static int serial_render;
#define audio_worker_submit(fn,context) (serial_render ? 0 : audio_worker_submit(fn,context))
#define main hostsim_main
#include "hostsim.c"
#undef main

#define FRAMES (CTL * 256u)
static int32_t reference[FRAMES * 2];
static _Atomic uint32_t held_job_entered, held_job_release;
static void held_job(void *context)
{
    assert(context == &held_job_release);
    atomic_store(&held_job_entered, 1);
    while (!atomic_load(&held_job_release)) sched_yield();
}

static void *worker_thread(void *unused)
{
    (void)unused;
    audio_worker_loop();
    return 0;
}
static void setup_case(uint32_t alg, uint32_t eng, uint32_t voices)
{
    memset(trk, 0, sizeof trk);
    eng_state_reset();
    memset(fm6_eff, 0, sizeof fm6_eff);
    memset(midi_bend_q8, 0, sizeof midi_bend_q8);
    memset(midi_bend_target, 0, sizeof midi_bend_target);
    memset(&mod, 0, sizeof mod);
    memset(resource, 0, sizeof resource);
    memset(mix_cache, 0, sizeof mix_cache);
    track_solo = 0;
    for (uint32_t i=0;i<NPART;i++) solo_gain[i]=32768;
    lim_env = LIM_T;
    dc_l = dc_r = dce_l = dce_r = 0;
    rng_state = 0x12345678u;
    mod_seed = 0x2545F491u;
    vage = 0; panic_req = 0;
    host_tracks_init();
    fm6_fn_reset();
    for (uint32_t i = 0; i < NTRK; i++) {
        host_preset(&trk[i], ENGI_FM6, i);
        uint8_t patch[FP_SIZE + 1];
        fm6_unpack(FM6_INIT, patch);
        /* Exercise every operator bus and feedback route, not just the init
         * patch's single audible carrier. */
        for (uint32_t op = 0; op < 6; op++) {
            uint8_t *p = patch + op * FP_OP;
            p[FP_OL] = 75u + op * 4u; p[FP_FC] = 1u + op % 3u;
            p[FP_R1] = 90; p[FP_R1 + 1] = 75; p[FP_R1 + 2] = 70; p[FP_R1 + 3] = 80;
            p[FP_L1] = 99; p[FP_L1 + 1] = 80; p[FP_L1 + 2] = 72; p[FP_L1 + 3] = 0;
        }
        patch[FP_ALG] = alg; patch[FP_FB] = 7;
        fm6_put_patch(i, patch, 1);
        FM6F(&trk[i])[FN_ENGINE] = eng;
        trk[i].p[P_LEVEL] = 70;
        trk[i].p[P_RESERVED34] = 24; trk[i].p[P_RESERVED36] = 36;
        trk[i].p[P_M1SRC] = MS_LFO; trk[i].p[P_M1DST] = MD_PITCH; trk[i].p[P_M1AMT] = 12;
        trk[i].p[P_M2SRC] = MS_VEL; trk[i].p[P_M2DST] = MD_PAN; trk[i].p[P_M2AMT] = 7;
        trk[i].p[P_VOICE] = voices == 8 ? V_UNISON : V_POLY;
    }
    for (uint32_t i = 0; i < voices; i++) trk_note_on(&trk[i % NTRK], 48u + i, 70u + i * 3u);
}
static void render_case(int parallel, uint32_t alg, uint32_t eng, uint32_t voices)
{
    setup_case(alg, eng, voices);
    audio_worker_online = parallel;
    for (uint32_t pos = 0; pos < FRAMES; pos += CTL) {
        int32_t out[2 * CTL];
        if (pos == CTL * 32u) midi_bend_target[0] = 256;
        if (pos == CTL * 64u) for (uint32_t i = 0; i < voices; i++) trk_note_off(&trk[i % NTRK], 48u + i);
        if (pos == CTL * 96u) for (uint32_t i = 0; i < voices; i++) trk_note_on(&trk[i % NTRK], 60u + i, 110u);
        if (pos == CTL * 160u) { trk[0].eng_req = 0; trk[0].p[P_E0] = 1; }
        mix_block(out, CTL);
        assert(!fm6_pending);
        assert(AW_LOAD(&audio_worker.request) == AW_LOAD(&audio_worker.complete));
        if (!parallel) memcpy(reference + 2u * pos, out, sizeof out);
        else if (memcmp(reference + 2u * pos, out, sizeof out)) {
            fprintf(stderr, "difference: algorithm %u engine %u voices %u block %u\n", alg, eng, voices, pos/CTL);
            abort();
        }
    }
}
static uint32_t active_voices(const track_t *t)
{
    uint32_t count=0;
    for(uint32_t i=0;i<NVOICE;i++)count+=t->v[i].active && t->v[i].stage!=4u;
    return count;
}
/* A CPU1 fault mid-song, as the exception handler reports it: a join still
 * waiting drops the paired voice's block, else the next block's check (as
 * audio.c) retires the worker. CPU0 then renders all eight sounding voices
 * (FM6's; the Prophet's pairing went with it, 2026-10-22) */
static void fm6_retire(void)
{
    setup_case(0,0,0);
    audio_worker_online=1;serial_render=0;
    host_preset(&trk[0],ENGI_FM6,4);
    for(uint32_t i=0;i<8;i++)trk_note_on(&trk[0],48+i*3,100);
    assert(active_voices(&trk[0])==8);
    uint32_t jobs=0;int32_t peak=0;
    for(uint32_t pos=0;pos<CTL*64u;pos+=CTL){
        int32_t out[2*CTL];
        audio_worker_check();
        if(pos==CTL*16u)injected_faults=1;
        mix_block(out,CTL);
        assert(!fm6_pending && AW_LOAD(&audio_worker.request)==AW_LOAD(&audio_worker.complete));
        if(pos==CTL*17u){assert(!audio_worker_online);jobs=audio_worker.jobs;}
        if(pos>CTL*16u)for(uint32_t i=0;i<2u*CTL;i++)if(abs(out[i])>peak)peak=abs(out[i]);
    }
    assert(audio_worker.jobs==jobs && peak>0 && active_voices(&trk[0])==8);
    panic_req=1;events_block(CTL);
    injected_faults=0;audio_worker_online=1;
}
static void *release_stalled_job(void *unused)
{(void)unused;struct timespec t={0,5000000};nanosleep(&t,0);atomic_store(&held_job_release,1);return 0;}
static void shared_bus_retire(void)
{
    setup_case(7,0,4);fx_bus_mix=fx_bus_target=0;memset(fx_state,0,sizeof fx_state);for(uint32_t i=G_MFXTYPE;i<G_COUNT;i++)song.g[i]=GP[i].def;
    song.g[G_MFXTYPE]=FX_REVERB;audio_worker_online=1;serial_render=0;memset(fx_send_l,0,sizeof fx_send_l);memset(fx_send_r,0,sizeof fx_send_r);
    fx_bus_start(CTL);fx_bus_join();uint32_t base=NPART*FX_SLOTS;assert(resource[RES_FX0+base].size==4096);void *ptr=resource[RES_FX0+base].ptr;
    atomic_store(&held_job_entered,0);atomic_store(&held_job_release,0);assert(audio_worker_submit(held_job,&held_job_release));while(!atomic_load(&held_job_entered))sched_yield();
    fx_bus_pending=fx_bus_result=1;injected_faults=1;pthread_t release;assert(!pthread_create(&release,0,release_stalled_job,0));fx_bus_join();assert(!pthread_join(release,0));
    assert(!fx_bus_result && !fx_bus_pending && !audio_worker_online && resource[RES_FX0+base].ptr==ptr && fx_state[base].epoch!=fx_epoch);
    injected_faults=0;fx_bus_start(CTL);fx_bus_join();assert(fx_bus_result && fx_state[base].epoch==fx_epoch && !fx_state[base].failed);
    for(uint32_t i=G_MFXTYPE;i<G_COUNT;i++)song.g[i]=GP[i].def;memset(fx_state,0,sizeof fx_state);
    puts("Shared FX bus: worker fault discards result, keeps allocation owned, resets history and continues serially: OK");
}
static void shared_bus_case(int parallel,uint32_t type)
{
    setup_case(7,0,4);fx_bus_mix=fx_bus_target=0;memset(fx_state,0,sizeof fx_state);for(uint32_t i=G_MFXTYPE;i<G_COUNT;i++)song.g[i]=GP[i].def;
    song.g[G_MFXTYPE]=type;song.g[G_MFX2TYPE]=FX_TAPE;song.g[G_MFX3TYPE]=FX_REVERB;
    for(uint32_t k=0;k<NPART;k++)trk[k].p[P_FXSEND]=40+k*20;
    audio_worker_online=1;serial_render=!parallel;
    for(uint32_t frame=0;frame<FRAMES;frame+=CTL){int32_t out[2*CTL];mix_block(out,CTL);assert(!fx_bus_pending && AW_LOAD(&audio_worker.complete)==AW_LOAD(&audio_worker.request));
        if(parallel)assert(!memcmp(out,reference+frame*2,sizeof out));else memcpy(reference+frame*2,out,sizeof out);
    }
    for(uint32_t i=G_MFXTYPE;i<G_COUNT;i++)song.g[i]=GP[i].def;memset(fx_state,0,sizeof fx_state);serial_render=0;
}
static _Atomic uint32_t batch_advances;
static void batch_count_kernel(const void *context,int32_t *pcm)
{
    (void)context;atomic_fetch_add(&batch_advances,1u);
    for(uint32_t i=0;i<CTL;i++)pcm[i]=6;
}
static void batch_corruption(void)
{
    int32_t dropped[CTL]={0},producer[CTL]={0};audio_worker_online=1;serial_render=0;
    audio_batch_count=2;AB_STORE(&audio_batch_invalid,0u);AB_STORE(&audio_batch_closed,1u);
    AB_STORE(&audio_batch_published,2u);AB_STORE(&audio_batch_worker_mask,1u);
    for(uint32_t k=0;k<2;k++){
        audio_batch_job_t *j=&audio_batch_jobs[k];j->run=batch_count_kernel;j->n=k?CTL:CTL+1u;
        j->profile_id=AP_FM6;j->guard=(uint32_t)(uintptr_t)j->run^j->n^j->profile_id^0x913ab267u;
        j->out=k?producer:dropped;
    }
    atomic_store(&batch_advances,0u);audio_batch_started=(uint8_t)audio_worker_submit(audio_batch_parallel,0);
    assert(audio_batch_started);audio_batch_flush();
    assert(!audio_worker_online && atomic_load(&batch_advances)==1u);
    for(uint32_t i=0;i<CTL;i++)assert(!dropped[i] && producer[i]==6);
    AB_STORE(&audio_batch_invalid,0u);AB_STORE(&audio_batch_published,AUDIO_BATCH_JOBS+1u);
    audio_batch_parallel(0);assert(AB_LOAD(&audio_batch_invalid));
    AB_STORE(&audio_batch_invalid,0u);AB_STORE(&audio_batch_published,0u);
    puts("Audio batch: corrupt bounds retire worker, drop partial PCM and never replay advanced state: OK");
}
static int dual_core_suite(void)
{
    pthread_t thread;
    assert(!pthread_create(&thread, 0, worker_thread, 0));
    while (!AW_LOAD(&audio_worker.ready)) sched_yield();
    audio_worker_online = 1;
    assert(audio_worker_submit(held_job, &held_job_release));
    while (!atomic_load(&held_job_entered)) sched_yield();
    assert(!audio_worker_submit(fm6_worker_kernel, 0));
    assert(audio_worker.context == &held_job_release);
    atomic_store(&held_job_release, 1);
    audio_worker_join();
    for (uint32_t eng = 0; eng < 3; eng++)
        for (uint32_t alg = 0; alg < 32; alg++)
            for (uint32_t voices = 1; voices <= 16; voices++) {
                render_case(0, alg, eng, voices);
                render_case(1, alg, eng, voices);
            }
    for(uint32_t type=1;type<FX_COUNT;type++){shared_bus_case(0,type);shared_bus_case(1,type);}
    puts("Shared FX bus: 8 three-effect chains, four live synth tracks, serial/concurrent bit equality and joins: OK");
    shared_bus_retire();
    fm6_retire();
    batch_corruption();
    audio_worker_online = 0;
    assert(!audio_worker_submit(fm6_worker_kernel, 0));
    assert(audio_worker.jobs > 0);
    assert(!pthread_cancel(thread));
    assert(!pthread_join(thread, 0));
    /* Recreate an idle worker at the sequence boundary: 0 is a valid wrapped
     * request, not an empty-job sentinel. */
    AW_STORE(&audio_worker.ready, 0);
    AW_STORE(&audio_worker.request, UINT32_MAX);
    AW_STORE(&audio_worker.complete, UINT32_MAX);
    atomic_store(&held_job_entered, 0);
    assert(!pthread_create(&thread, 0, worker_thread, 0));
    while (!AW_LOAD(&audio_worker.ready)) sched_yield();
    audio_worker_online = 1;
    assert(audio_worker_submit(held_job, &held_job_release));
    audio_worker_join();
    assert(atomic_load(&held_job_entered));
    assert(AW_LOAD(&audio_worker.request) == 0);
    assert(!pthread_cancel(thread));
    assert(!pthread_join(thread, 0));
    printf("dual-core: 1536 FM6 serial/concurrent cases identical; %u FM6 pairs; retire mid-song, busy/offline/wrap ok\n", fm6_pairs);
    return 0;
}

#ifndef AUDIO_MATRIX_INCLUDE
int main(void){return dual_core_suite();}
#endif
