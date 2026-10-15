/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* Central parameter IDs (pid.h). A track's and the global settings keep their descriptors where Melodee has them
 * (params.c TP, GP and the engines'); this file names them, says what each is for, and adds Tessera's (TX_DESC).
 * Left out: the reserved and retired ids (param_reserved, global_reserved, DIGITAL's OP ENV / OP LEVEL) and the
 * CPU meter. tests/pid_test.c keeps the list: an id changing meaning fails it (tests/pid_ids.txt). */
#include "pid.h"

static const char *const TX_N_MODE[] = {"STEP", "TURING", "EUCLID", "MARKOV", "MORPH"};
static const char *const TX_N_EVERY[] = {"OFF", "1", "2", "4", "8", "16"};
static const char *const TX_N_FORM[] = {"OFF", "AB", "ABAC", "ABCB", "AABA", "ABCA", "AABC"};
static const char *const TX_N_FOLLOW[] = {"OFF", "T1", "T2", "T3", "T4"};

/* the ranges tessera_param_set keeps (PULSES and ROT: at most STEPS, below STEPS, as it says) */
static const param_desc_t TX_DESC[TX_COUNT] = {
    [TX_MODE] = PE("MODE", TX_N_MODE, 0),
    [TX_TUR_LEN] = {"LENGTH", F_INT, 2, 16, 16, 0, "steps"},
    [TX_TUR_PROB] = PD("PROB", F_PCT, 0, 100, 0),
    [TX_TUR_SPAN] = {"SPAN", F_INT, 1, 3, 2, 0, "oct"},
    [TX_EUC_STEPS] = {"STEPS", F_INT, 1, 32, 16, 0, "steps"},
    [TX_EUC_PULSES] = PD("PULSE", F_INT, 0, 32, 4),
    [TX_EUC_ROT] = PD("ROT", F_INT, 0, 31, 0),
    [TX_MKV_LEAP] = PD("LEAP", F_PCT, 0, 100, 25),
    [TX_MKV_DENS] = PD("DENS", F_PCT, 5, 100, 70),
    [TX_MKV_SPAN] = {"SPAN", F_INT, 1, 3, 2, 0, "oct"},
    [TX_MRP_POS] = PD("POS", F_PCT, 0, 127, 0),
    [TX_MRP_SNPA] = PD("SNP A", F_INT, 0, 7, 0),
    [TX_MRP_SNPB] = PD("SNP B", F_INT, 0, 7, 2),
    [TX_EV_LOCK] = PD("LOCK", F_PCT, 0, 100, 0),
    [TX_EV_EVERY] = {"EVERY", F_ENUM, 0, 5, 0, TX_N_EVERY, "bar"},
    [TX_EV_AMOUNT] = PD("AMOUNT", F_PCT, 0, 100, 30),
    [TX_EV_SEED] = PD("ROLL", F_INT, 1, 128, 1),    /* (TESS_SEED_MAX; a new track picks one at random) */
    [TX_FM_FORM] = PE("FORM", TX_N_FORM, 0),
    [TX_FM_CONTRAST] = PD("CONTR", F_PCT, 0, 100, 60),
    [TX_FM_FLOURISH] = PD("FLRSH", F_SEMI, -12, 12, 0),
    [TX_TW_SHIFT] = PD("SHIFT", F_SEMI, -24, 24, 0),
    [TX_TW_DROP] = PD("DROP", F_PCT, 0, 100, 0),
    [TX_TW_HUMAN] = PD("HUMAN", F_PCT, 0, 100, 0),
    [TX_TW_FOLLOW] = PE("FOLLOW", TX_N_FOLLOW, 0),
};

#define T_(p, n) {PID_TRACK(p), PID_F_TRACK, n, ""}
#define G_(g, f, n) {PID_GLOBAL(g), f, n, ""}
#define X_(x, n, h) {PID_TESS(x), PID_F_TRACK, n, h}
#define MOD_(k) T_(P_M##k##SRC, "MOD " #k " SOURCE"), T_(P_M##k##DST, "MOD " #k " DESTINATION"), \
                T_(P_M##k##AMT, "MOD " #k " AMOUNT")
#define FX_(k, b) T_(b, "FX " #k " TYPE"), T_(b + 1, "FX " #k " MIX"), T_(b + 2, "FX " #k " RATE"), \
                  T_(b + 3, "FX " #k " CHARACTER")
#define MFX_(k, b) G_(b, 0, "SEND FX " #k " TYPE"), G_(b + 1, 0, "SEND FX " #k " MIX"), G_(b + 2, 0, "SEND FX " #k " RATE"), \
                   G_(b + 3, 0, "SEND FX " #k " CHARACTER")

static const pid_entry_t PIDS[] = {             /* in id order (pid_test checks it) */
    T_(P_LEVEL, "LEVEL"),
    T_(P_ATK, "ENV ATTACK"), T_(P_DEC, "ENV DECAY"), T_(P_SUS, "ENV SUSTAIN"), T_(P_REL, "ENV RELEASE"),
    T_(P_ED_FLT, "ENV TO FILTER"), T_(P_ED_PIT, "ENV TO PITCH"), T_(P_ED_SHP, "ENV TO SHAPE"),
    T_(P_RECQ, "RECORD QUANTIZE"),
    T_(P_LRATE, "LFO RATE"), T_(P_LWAVE, "LFO WAVE"), T_(P_LPHASE, "LFO PHASE"), T_(P_LFADE, "LFO FADE"),
    T_(P_LD_PIT, "LFO TO PITCH"), T_(P_LD_FLT, "LFO TO FILTER"), T_(P_LD_SHP, "LFO TO SHAPE"), T_(P_LD_AMP, "LFO TO LEVEL"),
    T_(P_AMODE, "ARP MODE"), T_(P_ARATE, "ARP RATE"), T_(P_AOCT, "ARP OCTAVES"), T_(P_AGATE, "ARP GATE"),
    T_(P_ASWING, "ARP SWING"), T_(P_APROB, "ARP PROBABILITY"), T_(P_AHOLD, "ARP HOLD"), T_(P_AORDER, "ARP ORDER"),
    T_(P_ROOT, "ROOT"), T_(P_SCALE, "SCALE"), T_(P_QUANT, "QUANTIZE"), T_(P_TRANS, "TRANSPOSE"),
    T_(P_SLEN, "PATTERN LENGTH"), T_(P_SDIV, "PATTERN DIVISION"), T_(P_SSWING, "PATTERN SWING"), T_(P_SGATE, "PATTERN GATE"),
    T_(P_VOICE, "VOICE MODE"), T_(P_GLIDE, "GLIDE"), T_(P_PAN, "PAN"), T_(P_MUTE, "MUTE"),
    T_(P_GLMODE, "GLIDE MODE"), T_(P_PRIO, "NOTE PRIORITY"), T_(P_ALLOC, "VOICE ALLOCATION"), T_(P_DETUNE, "UNISON DETUNE"),
    MOD_(1), MOD_(2), MOD_(3), MOD_(4),
    T_(P_CHRD, "CHORD"), T_(P_VOIC, "CHORD VOICING"), T_(P_MPCDEG, "MPC DEGREE"),
    T_(P_LN0, "DRUM KICK"), T_(P_LN1, "DRUM SNARE"), T_(P_LN2, "DRUM CLAP"), T_(P_LN3, "DRUM CLOSED HAT"),
    T_(P_LN4, "DRUM OPEN HAT"), T_(P_LN5, "DRUM TOM"), T_(P_LN6, "DRUM RIM"), T_(P_LN7, "DRUM BELL"),
    T_(P_LSYNC, "LFO SYNC"), T_(P_LTRIG, "LFO TRIGGER"), T_(P_LPOL, "LFO POLARITY"), T_(P_SPRD, "STEREO SPREAD"),
    FX_(1, P_FXTYPE), FX_(2, P_FX2TYPE), FX_(3, P_FX3TYPE),
    T_(P_FXSEND, "FX SEND"), T_(P_FXSEND2, "FX SEND 2"), T_(P_FXSEND3, "FX SEND 3"),
    T_(P_E0, "ENGINE 1"), T_(P_E1, "ENGINE 2"), T_(P_E2, "ENGINE 3"), T_(P_E3, "ENGINE 4"),   /* (named by the */
    T_(P_E4, "ENGINE 5"), T_(P_E5, "ENGINE 6"), T_(P_E6, "ENGINE 7"), T_(P_E7, "ENGINE 8"),   /* track's engine) */

    G_(G_BPM, 0, "TEMPO"), G_(G_SWING, 0, "SWING"), G_(G_CLOCK, 0, "CLOCK SOURCE"), G_(G_TUNE, 0, "TUNE"),
    G_(G_MIDI, 0, "MIDI INPUT"), G_(G_DRUMCH, PID_F_DEVICE, "DRUM MIDI CHANNEL"), G_(G_ROUTE, 0, "MIDI ROUTING"),
    G_(G_SLOT, PID_F_ACTION, "PROJECT SLOT"), G_(G_BOOT, PID_F_DEVICE, "BOOT PROJECT"),
    G_(G_LOAD, PID_F_ACTION, "PROJECT LOAD"), G_(G_SAVE, PID_F_ACTION, "PROJECT SAVE"),
    G_(G_ENGSEL, PID_F_ACTION, "ENGINE"), G_(G_A4, PID_F_DEVICE, "A4 TUNING"),
    G_(G_CLRSEQ, PID_F_ACTION, "CLEAR SEQUENCE"), G_(G_INITSND, PID_F_ACTION, "INIT SOUND"),
    MFX_(1, G_MFXTYPE), MFX_(2, G_MFX2TYPE), MFX_(3, G_MFX3TYPE),
    G_(G_FXRETURN, 0, "FX RETURN"), G_(G_FXMODE, 0, "FX MODE"),

    X_(TX_MODE, "GENERATOR MODE", "STEP plays the track's own steps; TURING, EUCLID, MARKOV and MORPH make its notes."),
    X_(TX_TUR_LEN, "TURING LENGTH", "How many steps the register loops over before it comes round again."),
    X_(TX_TUR_PROB, "TURING PROB", "The chance a step changes as it comes round: 0 locks the loop, 100 is new every pass."),
    X_(TX_TUR_SPAN, "TURING SPAN", "How many octaves the notes spread over."),
    X_(TX_EUC_STEPS, "EUCLID STEPS", "The rhythm's length in steps."),
    X_(TX_EUC_PULSES, "EUCLID PULSES", "How many hits, spread as evenly as they go over STEPS (at most STEPS)."),
    X_(TX_EUC_ROT, "EUCLID ROTATION", "Turns the rhythm round by this many steps (fewer than STEPS)."),
    X_(TX_MKV_LEAP, "MARKOV LEAP", "How far each note jumps from the last: 0 moves to its neighbours, 100 leaps widely."),
    X_(TX_MKV_DENS, "MARKOV DENSITY", "The chance a step plays a note; the others rest."),
    X_(TX_MKV_SPAN, "MARKOV SPAN", "How many octaves the notes spread over."),
    X_(TX_MRP_POS, "MORPH POSITION", "Blends rhythm A (0) into rhythm B (full)."),
    X_(TX_MRP_SNPA, "MORPH RHYTHM A", "Rhythm A: one of eight preset patterns."),
    X_(TX_MRP_SNPB, "MORPH RHYTHM B", "Rhythm B: one of eight preset patterns."),
    X_(TX_EV_LOCK, "EVOLVE LOCK", "How much of the phrase stays when EVOLVE mutates it: 100 loops it, 0 is new every "
                                  "time. Turing's is its PROB turned round; Euclid has none."),
    X_(TX_EV_EVERY, "EVOLVE EVERY", "Bars between mutations; OFF keeps the phrase as it is."),
    X_(TX_EV_AMOUNT, "EVOLVE AMOUNT", "How much changes at each mutation."),
    X_(TX_EV_SEED, "EVOLVE ROLL", "The track's seed, 1 to 128: each one a line of its own, the same every time. "
                                  "Playing, a new one starts on the next bar line."),
    X_(TX_FM_FORM, "FORM", "The order the sections play in (A, B, C), each EVERY bars long; OFF plays one phrase."),
    X_(TX_FM_CONTRAST, "FORM CONTRAST", "How different a new section is from the one before it."),
    X_(TX_FM_FLOURISH, "FORM FLOURISH", "Semitones added on the last beat of every phrase; 0 is off."),
    X_(TX_TW_SHIFT, "TWEAK SHIFT", "Transposes the notes by semitones; while following, by scale steps from the "
                                   "followed note."),
    X_(TX_TW_DROP, "TWEAK DROP", "The chance a hit is left out, whatever the generator."),
    X_(TX_TW_HUMAN, "TWEAK HUMAN", "Varies velocity and gate a little, as a player would."),
    X_(TX_TW_FOLLOW, "TWEAK FOLLOW", "Plays another track's pitch, SHIFT away, in this track's scale; OFF is its own."),
};
#undef T_
#undef G_
#undef X_
#undef MOD_
#undef FX_
#undef MFX_

uint32_t pid_count(void) { return NELEM(PIDS); }
const pid_entry_t *pid_at(uint32_t i) { return i < NELEM(PIDS) ? &PIDS[i] : 0; }

const pid_entry_t *pid_find(uint32_t pid)
{
    uint32_t lo = 0, hi = NELEM(PIDS);
    while (lo < hi) {                               /* (PIDS is in id order) */
        uint32_t mid = (lo + hi) / 2u;
        if (PIDS[mid].pid == pid) return &PIDS[mid];
        if (PIDS[mid].pid < pid) lo = mid + 1u;
        else hi = mid;
    }
    return 0;
}

const param_desc_t *pid_desc(uint32_t pid, uint32_t tr)
{
    if (!pid_find(pid)) return 0;
    if (pid >= PID_TESS(0)) return &TX_DESC[pid - PID_TESS(0)];
    if (pid >= PID_GLOBAL(0)) return global_desc(pid - PID_GLOBAL(0));
    return track_desc(&trk[tr % NTRK], pid);
}

int32_t pid_get(uint32_t pid, uint32_t tr)
{
    if (!pid_find(pid)) return 0;
    if (pid >= PID_TESS(0)) return tessera_param_get(tr, pid - PID_TESS(0));
    if (pid == PID_GLOBAL(G_A4)) return tuning_a4;          /* (the device's own: settings_persist.c) */
    if (pid == PID_GLOBAL(G_BOOT)) return settings_boot;
    if (pid == PID_GLOBAL(G_DRUMCH)) return settings_drumch;
    if (pid >= PID_GLOBAL(0)) return song.g[pid - PID_GLOBAL(0)];
    return trk[tr % NTRK].p[pid];
}

/* the stored value, as a project load sets it: clamped (an enum's blank alias: its original). Not done here, as the
 * UI does after a knob: P_SCALE / P_QUANT / P_MPCDEG shared to the other tracks (ui.c scale_share), a motion
 * capture. The device's settings (A4, BOOT, DRUM channel) are not set here: they have their own setters and save */
int pid_set(uint32_t pid, uint32_t tr, int32_t v)
{
    const pid_entry_t *e = pid_find(pid);
    const param_desc_t *d;
    if (!e || (e->flags & (PID_F_ACTION | PID_F_DEVICE))) return 0;
    if (pid >= PID_TESS(0)) {
        tessera_param_set(tr, pid - PID_TESS(0), v);
        return 1;
    }
    d = pid_desc(pid, tr);
    if (!d || d->max <= d->min) return 0;
    v = enum_orig(d, clamp(v, d->min, d->max));
    if (pid >= PID_GLOBAL(0)) song.g[pid - PID_GLOBAL(0)] = (int16_t)v;
    else trk[tr % NTRK].p[pid] = (int16_t)v;
    return 1;
}
