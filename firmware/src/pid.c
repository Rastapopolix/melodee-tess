/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* Central parameter IDs (pid.h). A track's and the global settings keep their descriptors where Melodee has them
 * (params.c TP, GP and the engines'); this file names them, says what each is for, and adds Tessera's (TX_DESC).
 * Left out: the reserved and retired ids (param_reserved, global_reserved, DIGITAL's OP ENV / OP LEVEL) and the
 * CPU meter. tests/pid_test.c keeps the list: an id changing meaning fails it (tests/pid_ids.txt). */
#include "pid.h"

static const char *const TX_N_MODE[] = {"STEP", "TURING", "EUCLID", "MARKOV", "MORPH", "GRIDS", "BOUNCE", "AWAKE", "POLY", "QUAD", "CONSTL"};
static const char *const TX_N_QDIR[] = {"FWD", "BACK", "PEND", "RAND", "HOLD"};
static const char *const TX_N_DIV[] = {"OFF", "/1", "/2", "/3", "/4", "/5", "/6", "/7", "/8", "/9", "/10", "/11", "/12",
                                         "/13", "/14", "/15", "/16"};   /* (every n steps: no unit, "st" read as semitones) */
static const char *const TX_N_EVERY[] = {"OFF", "1", "2", "4", "8", "16"};
static const char *const TX_N_FORM[] = {"OFF", "AB", "ABAC", "ABCB", "AABA", "ABCA", "AABC"};
static const char *const TX_N_FOLLOW[] = {"OFF", "T1", "T2", "T3", "T4"};
static const char *const TX_N_BRMODE[] = {"COIN", "TOGGLE", "LATCH"};
static const char *const TX_N_LOGIC[] = {"OFF", "AND", "OR", "XOR", "NOT"};
static const char *const TX_N_COND[] = {"OFF", "1:2", "2:2", "1:4", "2:4", "3:4", "4:4", "1:8", "8:8", "FIRST", "!FIRST"};
static const char *const TX_N_NLMODE[] = {"STEP", "FIXED", "HIT"};
static const char *const TX_N_CSAUTO[] = {"OFF", "1", "2", "3", "4", "5", "6", "7", "8", "9", "SEEK"};
static const char *const TX_N_NLLEN[] = {"1/32", "1/16", "1/8", "1/4", "1/2", "1/1"};

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
    [TX_GRD_X] = PD("X", F_INT, 0, 255, 128),
    [TX_GRD_Y] = PD("Y", F_INT, 0, 255, 128),
    [TX_GRD_DENS] = PD("DENS", F_PCT, 0, 100, 50),
    [TX_BNC_SPEED] = PD("SPEED", F_INT, 1, 64, 24),
    [TX_BNC_ANGLE] = PD("ANGLE", F_INT, 0, 90, 37),
    [TX_BNC_BALLS] = PD("BALLS", F_INT, 1, 5, 1),     /* 5: FALL (gen.h GEN_BOUNCE_FALL) */
    [TX_AWK_LENA] = PD("LEN A", F_INT, 1, 16, 8),
    [TX_AWK_LENB] = PD("LEN B", F_INT, 1, 16, 7),
    [TX_AWK_PROB] = PD("PROB", F_PCT, 0, 100, 100),
    [TX_POL_DIV1] = {"DIV 1", F_ENUM, 0, 16, 4, TX_N_DIV, ""},
    [TX_POL_DIV2] = {"DIV 2", F_ENUM, 0, 16, 3, TX_N_DIV, ""},
    [TX_POL_DIV3] = {"DIV 3", F_ENUM, 0, 16, 0, TX_N_DIV, ""},
    [TX_POL_DIV4] = {"DIV 4", F_ENUM, 0, 16, 0, TX_N_DIV, ""},
    [TX_POL_SPAN] = PD("SPAN", F_INT, 1, 3, 1),
    [TX_TW_BRANCH] = PD("BRANCH", F_PCT, 0, 100, 0),
    [TX_TW_BR_TO] = PE("TO", TX_N_FOLLOW, 0),
    [TX_TW_BR_MODE] = PE("BR MODE", TX_N_BRMODE, 0),
    [TX_TW_LOGIC] = PE("LOGIC", TX_N_LOGIC, 0),
    [TX_TW_LG_WITH] = PE("WITH", TX_N_FOLLOW, 0),
    [TX_TW_COND] = PE("COND", TX_N_COND, 0),
    [TX_ORG_SPEED] = PD("SPEED", F_INT, 0, 100, 50),
    [TX_ORG_RATE] = PD("RATE", F_INT, 0, 100, 50),
    [TX_QD_DIR] = PE("DIR", TX_N_QDIR, 0),
    [TX_QD_PROB] = PD("PROB", F_PCT, 0, 100, 100),
    [TX_QD_LINK] = PE("LINK", TX_N_FOLLOW, 0),
    [TX_NL_MODE] = PE("LENGTH", TX_N_NLMODE, 0),
    [TX_NL_LEN] = PE("LEN", TX_N_NLLEN, 2),
    [TX_NL_KEYS] = PD("KEYS", F_INT, 1, 4, 4),
    [TX_CS_Y] = PD("Y", F_INT, 0, 14, 7),
    [TX_CS_DENS] = PD("DENS", F_PCT, 0, 100, 50),
    [TX_CS_SIZE] = PD("SIZE", F_INT, 0, 7, 2),
    [TX_CS_AUTO] = PE("AUTO", TX_N_CSAUTO, 0),
};

#define T_(p, n, h) {PID_TRACK(p), PID_F_TRACK, n, h}
#define G_(g, f, n, h) {PID_GLOBAL(g), f, n, h}
#define X_(x, n, h) {PID_TESS(x), PID_F_TRACK, n, h}

static const pid_entry_t PIDS[] = {             /* in id order (pid_test checks it) */
    T_(P_LEVEL, "LEVEL", "The track's volume in the mix."),
    T_(P_ATK, "ENV ATTACK", "How long a note takes to reach full level."),
    T_(P_DEC, "ENV DECAY", "How long it then takes to fall to SUSTAIN."),
    T_(P_SUS, "ENV SUSTAIN", "The level a held note stays at."),
    T_(P_REL, "ENV RELEASE", "How long a note takes to fade once let go."),
    T_(P_ED_FLT, "ENV TO FILTER", "How far the envelope opens (or, below 0, closes) the filter."),
    T_(P_ED_PIT, "ENV TO PITCH", "How far the envelope bends the pitch."),
    T_(P_ED_SHP, "ENV TO SHAPE", "How far the envelope moves the engine's SHAPE."),
    T_(P_RECQ, "RECORD QUANTIZE",
        "Moves a recorded take onto this grid when recording ends; OFF keeps the timing as played."),
    T_(P_LRATE, "LFO RATE", "How fast the track's LFO moves."),
    T_(P_LWAVE, "LFO WAVE", "The LFO's shape: sine, triangle, saw, square or a random step each cycle (S&H)."),
    T_(P_LPHASE, "LFO PHASE", "Where in its cycle the LFO starts."),
    T_(P_LFADE, "LFO FADE", "How long the LFO takes to fade in after a note starts."),
    T_(P_LD_PIT, "LFO TO PITCH", "How far the LFO moves the pitch (vibrato)."),
    T_(P_LD_FLT, "LFO TO FILTER", "How far the LFO moves the filter."),
    T_(P_LD_SHP, "LFO TO SHAPE", "How far the LFO moves the engine's SHAPE."),
    T_(P_LD_AMP, "LFO TO LEVEL", "How far the LFO moves the level (tremolo)."),
    T_(P_AMODE, "ARP MODE",
        "The arpeggio: up, down, up and down, random, in ARP ORDER, or the last note played repeated; OFF: none."),
    T_(P_ARATE, "ARP RATE", "How often the arpeggiator plays a note."),
    T_(P_AOCT, "ARP OCTAVES", "How many octaves the arpeggio climbs through."),
    T_(P_AGATE, "ARP GATE", "How long each arpeggio note lasts, as a share of its step."),
    T_(P_ASWING, "ARP SWING", "Delays every second arpeggio note, for a swung feel."),
    T_(P_APROB, "ARP PROBABILITY", "The chance each arpeggio note plays."),
    T_(P_AHOLD, "ARP HOLD", "ON keeps the arpeggio going after the keys are let go."),
    T_(P_AORDER, "ARP ORDER", "NOTE: the held notes from low to high; PLAY: in the order they were pressed."),
    T_(P_ROOT, "ROOT", "The scale's key note."),
    T_(P_SCALE, "SCALE", "The scale the keys and the generators play in."),
    T_(P_QUANT, "QUANTIZE",
        "How the keys meet the scale: OFF plays every key, SNAP moves a note into the scale, WHITE puts the "
        "scale on the white keys, ALL and MPC lay it out over the keys or MPC pads."),
    T_(P_TRANS, "TRANSPOSE", "Moves the track's notes up or down by semitones."),
    T_(P_SLEN, "PATTERN LENGTH", "How many steps the track's pattern has."),
    T_(P_SDIV, "PATTERN DIVISION", "How long one step is."),
    T_(P_SSWING, "PATTERN SWING", "Delays every second step, for a swung feel."),
    T_(P_SGATE, "PATTERN GATE", "How long each step's note lasts, as a share of the step."),
    T_(P_VOICE, "VOICE MODE",
        "POLY plays chords; MONO one note at a time; LEG(ATO) one at a time, a note played while one is held "
        "not starting again; UNI stacks detuned voices on one note."),
    T_(P_GLIDE, "GLIDE", "How long a note takes to slide to the next."),
    T_(P_PAN, "PAN", "The track's place from left to right."),
    T_(P_MUTE, "MUTE", "ON silences the track; it keeps playing in time."),
    T_(P_GLMODE, "GLIDE MODE", "RATE: the slide's speed is fixed; TIME: every slide takes the same time, however far."),
    T_(P_PRIO, "NOTE PRIORITY",
        "Which note sounds in MONO when several are held: the last, the lowest or the highest."),
    T_(P_ALLOC, "VOICE ALLOCATION",
        "ROT gives each new note the next free voice; REUSE the free voice last nearest its pitch (smoother glides)."),
    T_(P_DETUNE, "UNISON DETUNE", "How far apart UNI's stacked voices are tuned."),
    T_(P_M1SRC, "MOD 1 SOURCE",
        "MOD route 1: what moves the destination (LFO, envelope, velocity, key, random, the controllers)."),
    T_(P_M1DST, "MOD 1 DESTINATION",
        "MOD route 1: what is moved (pitch, filter, shape, level, pan, LFO rate or depth, the engine's own)."),
    T_(P_M1AMT, "MOD 1 AMOUNT", "MOD route 1: how far, and which way."),
    T_(P_M2SRC, "MOD 2 SOURCE",
        "MOD route 2: what moves the destination (LFO, envelope, velocity, key, random, the controllers)."),
    T_(P_M2DST, "MOD 2 DESTINATION",
        "MOD route 2: what is moved (pitch, filter, shape, level, pan, LFO rate or depth, the engine's own)."),
    T_(P_M2AMT, "MOD 2 AMOUNT", "MOD route 2: how far, and which way."),
    T_(P_M3SRC, "MOD 3 SOURCE",
        "MOD route 3: what moves the destination (LFO, envelope, velocity, key, random, the controllers)."),
    T_(P_M3DST, "MOD 3 DESTINATION",
        "MOD route 3: what is moved (pitch, filter, shape, level, pan, LFO rate or depth, the engine's own)."),
    T_(P_M3AMT, "MOD 3 AMOUNT", "MOD route 3: how far, and which way."),
    T_(P_M4SRC, "MOD 4 SOURCE",
        "MOD route 4: what moves the destination (LFO, envelope, velocity, key, random, the controllers)."),
    T_(P_M4DST, "MOD 4 DESTINATION",
        "MOD route 4: what is moved (pitch, filter, shape, level, pan, LFO rate or depth, the engine's own)."),
    T_(P_M4AMT, "MOD 4 AMOUNT", "MOD route 4: how far, and which way."),
    T_(P_CHRD, "CHORD",
        "One key plays a chord: in the scale (DIA3, DIA7) or a fixed kind (MAJ, MIN, 7ths, SUS4, POWER); OFF "
        "plays one note."),
    T_(P_VOIC, "CHORD VOICING",
        "How the chord's notes are spread: close, open, an inversion, or with the root an octave down."),
    T_(P_MPCDEG, "MPC DEGREE", "With QUANTIZE MPC: the scale degree the pad H02 plays."),
    T_(P_LN0, "DRUM KICK", "The kick's level in the kit."),
    T_(P_LN1, "DRUM SNARE", "The snare's level in the kit."),
    T_(P_LN2, "DRUM CLAP", "The clap's level in the kit."),
    T_(P_LN3, "DRUM CLOSED HAT", "The closed hi-hat's level in the kit."),
    T_(P_LN4, "DRUM OPEN HAT", "The open hi-hat's level in the kit."),
    T_(P_LN5, "DRUM TOM", "The tom's level in the kit."),
    T_(P_LN6, "DRUM RIM", "The rimshot's level in the kit."),
    T_(P_LN7, "DRUM BELL", "The bell's level in the kit."),
    T_(P_LSYNC, "LFO SYNC",
        "Ties the LFO to the tempo: one cycle every bar, beat or step chosen; OFF runs at its own RATE."),
    T_(P_LTRIG, "LFO TRIGGER", "NOTE starts the LFO again with each note; FREE lets it run on."),
    T_(P_LPOL, "LFO POLARITY", "BI moves both ways from the setting; UNI only one way."),
    T_(P_SPRD, "STEREO SPREAD", "Spreads the track's voices from left to right."),
    T_(P_FXTYPE, "FX 1 TYPE", "The track's insert effect 1."),
    T_(P_FXTYPE + 1, "FX 1 MIX", "How much of insert effect 1 is heard."),
    T_(P_FXTYPE + 2, "FX 1 RATE", "Insert effect 1's speed (its time or size, by the effect)."),
    T_(P_FXTYPE + 3, "FX 1 CHARACTER", "Insert effect 1's character (its depth, decay or the like, by the effect)."),
    T_(P_FX2TYPE, "FX 2 TYPE", "The track's insert effect 2."),
    T_(P_FX2TYPE + 1, "FX 2 MIX", "How much of insert effect 2 is heard."),
    T_(P_FX2TYPE + 2, "FX 2 RATE", "Insert effect 2's speed (its time or size, by the effect)."),
    T_(P_FX2TYPE + 3, "FX 2 CHARACTER", "Insert effect 2's character (its depth, decay or the like, by the effect)."),
    T_(P_FX3TYPE, "FX 3 TYPE", "The track's insert effect 3."),
    T_(P_FX3TYPE + 1, "FX 3 MIX", "How much of insert effect 3 is heard."),
    T_(P_FX3TYPE + 2, "FX 3 RATE", "Insert effect 3's speed (its time or size, by the effect)."),
    T_(P_FX3TYPE + 3, "FX 3 CHARACTER", "Insert effect 3's character (its depth, decay or the like, by the effect)."),
    T_(P_FXSEND, "FX SEND", "How much of the track goes to the effects (the first send effect when they are SPLIT)."),
    T_(P_FXSEND2, "FX SEND 2", "How much of the track goes to send effect 2 (FX MODE SPLIT)."),
    T_(P_FXSEND3, "FX SEND 3", "How much of the track goes to send effect 3 (FX MODE SPLIT)."),
    T_(P_E0, "ENGINE 1",
        "The engine's parameter 1: what it is depends on the track's engine (its name on the EDIT pages)."),
    T_(P_E1, "ENGINE 2",
        "The engine's parameter 2: what it is depends on the track's engine (its name on the EDIT pages)."),
    T_(P_E2, "ENGINE 3",
        "The engine's parameter 3: what it is depends on the track's engine (its name on the EDIT pages)."),
    T_(P_E3, "ENGINE 4",
        "The engine's parameter 4: what it is depends on the track's engine (its name on the EDIT pages)."),
    T_(P_E4, "ENGINE 5",
        "The engine's parameter 5: what it is depends on the track's engine (its name on the EDIT pages)."),
    T_(P_E5, "ENGINE 6",
        "The engine's parameter 6: what it is depends on the track's engine (its name on the EDIT pages)."),
    T_(P_E6, "ENGINE 7",
        "The engine's parameter 7: what it is depends on the track's engine (its name on the EDIT pages)."),
    T_(P_E7, "ENGINE 8",
        "The engine's parameter 8: what it is depends on the track's engine (its name on the EDIT pages)."),

    G_(G_BPM, 0, "TEMPO", "The project's speed in beats per minute."),
    G_(G_SWING, 0, "SWING", "Delays every second step of every track, for a swung feel."),
    G_(G_CLOCK, 0, "CLOCK SOURCE", "Where the tempo comes from: the FM-1 itself, USB MIDI or the TRS MIDI input."),
    G_(G_TUNE, 0, "TUNE", "Fine tuning of every track, in cents."),
    G_(G_MIDI, 0, "MIDI INPUT", "Which MIDI input plays the tracks: USB or TRS."),
    G_(G_DRUMCH, PID_F_DEVICE, "DRUM MIDI CHANNEL", "The MIDI channel the first drum track listens to and sends on (MIDI OUT)."),
    G_(G_ROUTE, 0, "MIDI ROUTING",
        "Which MIDI channels reach which tracks: four channels to the four tracks, or every channel to the "
        "selected track (SEL). MIDI OUT sends each track on the same channels (SEL: 1 to 4)."),
    G_(G_SLOT, PID_F_ACTION, "PROJECT SLOT", "The project slot LOAD and SAVE use."),
    G_(G_BOOT, PID_F_DEVICE, "BOOT PROJECT", "The project loaded when the FM-1 starts; OFF starts a new one."),
    G_(G_LOAD, PID_F_ACTION, "PROJECT LOAD", "Loads the project in the slot."),
    G_(G_SAVE, PID_F_ACTION, "PROJECT SAVE", "Saves the project to the slot."),
    G_(G_ENGSEL, PID_F_ACTION, "ENGINE", "The selected track's sound engine."),
    G_(G_A4, PID_F_DEVICE, "A4 TUNING", "The pitch of A4 for the whole device, in hertz."),
    G_(G_CLRSEQ, PID_F_ACTION, "CLEAR SEQUENCE",
        "Empties the selected track's pattern (it asks first)."),
    G_(G_INITSND, PID_F_ACTION, "INIT SOUND", "Resets the selected track's sound to its engine's starting point."),
    G_(G_MFXTYPE, 0, "SEND FX 1 TYPE", "Send effect 1, shared by every track."),
    G_(G_MFXTYPE + 1, 0, "SEND FX 1 MIX", "How much of send effect 1 is heard."),
    G_(G_MFXTYPE + 2, 0, "SEND FX 1 RATE", "Send effect 1's speed (its time or size, by the effect)."),
    G_(G_MFXTYPE + 3, 0, "SEND FX 1 CHARACTER",
        "Send effect 1's character (its depth, decay or the like, by the effect)."),
    G_(G_MFX2TYPE, 0, "SEND FX 2 TYPE", "Send effect 2, shared by every track."),
    G_(G_MFX2TYPE + 1, 0, "SEND FX 2 MIX", "How much of send effect 2 is heard."),
    G_(G_MFX2TYPE + 2, 0, "SEND FX 2 RATE", "Send effect 2's speed (its time or size, by the effect)."),
    G_(G_MFX2TYPE + 3, 0, "SEND FX 2 CHARACTER",
        "Send effect 2's character (its depth, decay or the like, by the effect)."),
    G_(G_MFX3TYPE, 0, "SEND FX 3 TYPE", "Send effect 3, shared by every track."),
    G_(G_MFX3TYPE + 1, 0, "SEND FX 3 MIX", "How much of send effect 3 is heard."),
    G_(G_MFX3TYPE + 2, 0, "SEND FX 3 RATE", "Send effect 3's speed (its time or size, by the effect)."),
    G_(G_MFX3TYPE + 3, 0, "SEND FX 3 CHARACTER",
        "Send effect 3's character (its depth, decay or the like, by the effect)."),
    G_(G_FXRETURN, 0, "FX RETURN", "The level of the send effects in the mix."),
    G_(G_FXMODE, 0, "FX MODE",
        "CHAIN: one send through all three effects in a row; SPLIT: a send to each effect of its own."),
    G_(G_BUDGET, PID_F_DEVICE, "VOICE BUDGET",
       "How much sound work the FM-1 shares out between the tracks; lower leaves more headroom, higher allows more voices."),
    X_(TX_MODE, "GENERATOR MODE", "SEQ shows the pages of the MODE only. STEP plays the track's own steps; ARP the "
                                  "keys held, as an arpeggio (the ARP button opens it too); TURING, EUCLID, MARKOV and MORPH make its notes; "
                                  "GRIDS plays kick, snare and hat (best on a DRUM track); BOUNCE plays where a ball "
                                  "hits the walls of its box; AWAKE adds two loops of its own lengths; POLY steps four notes on four rhythms; "
                                  "QUAD plays the track's row of a four by four grid the tracks share; CONSTL (CONSTELLATION) "
                                  "catches the stars of a passing sky into a loop."),
    X_(TX_TUR_LEN, "TURING LENGTH", "How many steps the register loops over before it comes round again."),
    X_(TX_TUR_PROB, "TURING PROB",
       "The chance a step changes as it comes round: 0 locks the loop, 100 is new every pass."),
    X_(TX_TUR_SPAN, "TURING SPAN", "How many octaves the notes spread over."),
    X_(TX_EUC_STEPS, "EUCLID STEPS", "The rhythm's length in steps."),
    X_(TX_EUC_PULSES, "EUCLID PULSES", "How many hits, spread as evenly as they go over STEPS (at most STEPS)."),
    X_(TX_EUC_ROT, "EUCLID ROTATION", "Turns the rhythm round by this many steps (fewer than STEPS)."),
    X_(TX_MKV_LEAP, "MARKOV LEAP",
       "How far each note jumps from the last: 0 moves to its neighbours, 100 leaps widely."),
    X_(TX_MKV_DENS, "MARKOV DENSITY", "The chance a step plays a note; the others rest."),
    X_(TX_MKV_SPAN, "MARKOV SPAN", "How many octaves the notes spread over."),
    X_(TX_MRP_POS, "MORPH POSITION", "Blends rhythm A (0) into rhythm B (full)."),
    X_(TX_MRP_SNPA, "MORPH RHYTHM A", "Rhythm A: one of eight preset patterns."),
    X_(TX_MRP_SNPB, "MORPH RHYTHM B", "Rhythm B: one of eight preset patterns."),
    X_(TX_EV_LOCK, "EVOLVE LOCK", "How much of the phrase stays when EVOLVE mutates it: 100 loops it, 0 is new every "
                                  "time. Turing's is its PROB turned round, Grids' its CHAOS; Constellation's the chance a star caught leaves the loop as it was; Euclid, Bounce, Awake, Poly and Quad have none."),
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
    X_(TX_GRD_X, "GRIDS X", "Where on the map of drum patterns, left to right; between two, a blend of both."),
    X_(TX_GRD_Y, "GRIDS Y", "Where on the map of drum patterns, top to bottom; between two, a blend of both."),
    X_(TX_GRD_DENS, "GRIDS DENSITY", "How full the kick, snare and hat are: 0 silent, 100 every hit the map offers."),
    X_(TX_BNC_SPEED, "BOUNCE SPEED", "How far the ball travels each step: faster, more wall hits, more notes. In FALL, how fast the "
                                     "leaves fall."),
    X_(TX_BNC_ANGLE, "BOUNCE ANGLE", "The ball's path from the floor, in degrees: 0 and 90 go back and forth, an angle "
                                     "between draws a longer loop. EVOLVE nudges it. In FALL it is the WIND: the middle is calm, "
                                     "lower blows the leaves left, higher right."),
    X_(TX_BNC_BALLS, "BOUNCE BALLS", "How many balls: each one more starts from a place and an angle of its own, so they "
                                     "cross; balls hitting on the same step play together, three notes at most. Past 4, FALL: eight "
                                     "leaves fall at speeds of their own and play where they land (left low, right high; "
                                     "one in seven an octave down), rest, and fall again."),
    X_(TX_AWK_LENA, "AWAKE LENGTH A", "Steps in loop A, the melody: its values are degrees of the scale or rests. ROLL "
                                      "draws them, EVOLVE changes a few; a white key picks a step: KNOB 1 its A, KNOB 2 its B."),
    X_(TX_AWK_LENB, "AWAKE LENGTH B", "Steps in loop B, which lifts the melody up the scale; a length other than A's "
                                      "shifts it against A."),
    X_(TX_AWK_PROB, "AWAKE PROB", "The chance each of A's notes plays: 100 every one, lower leaves gaps."),
    X_(TX_POL_DIV1, "POLY RHYTHM 1", "Every how many steps rhythm 1 moves the four notes on (OFF: never). Rhythms that land "
                                     "together play once, louder: 4 against 3 is a polyrhythm."),
    X_(TX_POL_DIV2, "POLY RHYTHM 2", "Every how many steps rhythm 2 moves the four notes on (OFF: never)."),
    X_(TX_POL_DIV3, "POLY RHYTHM 3", "Every how many steps rhythm 3 moves the four notes on (OFF: never)."),
    X_(TX_POL_DIV4, "POLY RHYTHM 4", "Every how many steps rhythm 4 moves the four notes on (OFF: never)."),
    X_(TX_POL_SPAN, "POLY SPAN", "Octaves the four notes spread over; ROLL draws them, EVOLVE moves them a little."),
    X_(TX_TW_BRANCH, "TWEAK BRANCH", "The chance a hit plays on track TO's sound instead of this one's, so two tracks "
                                     "answer each other; 0 is off. The tosses replay with the SEED."),
    X_(TX_TW_BR_TO, "TWEAK BRANCH TO", "The track a branched hit plays on; OFF keeps every hit here. Turned on: COIN "
                                       "tosses each hit, TOGGLE switches side on a toss won, LATCH tosses once a bar."),
    X_(TX_TW_BR_MODE, "TWEAK BRANCH MODE", "COIN tosses each hit; TOGGLE switches side on a toss won (100 alternates); "
                                           "LATCH tosses once a bar and keeps the bar's hits on one side."),
    X_(TX_TW_LOGIC, "TWEAK LOGIC", "Combines the hits with track WITH's: AND plays only when both hit, OR when either "
                                   "does, XOR when one does, NOT only when the other rests; OFF is its own."),
    X_(TX_TW_LG_WITH, "TWEAK LOGIC WITH", "The track LOGIC reads: a lower track's hit on this step, a higher one's on the "
                                          "step before."),
    X_(TX_TW_COND, "TWEAK COND", "The bars a hit may play in, counted from PLAY (with FORM on, from the section's "
                                 "start): 1:4 is the first bar of every four; FIRST only the first, !FIRST the rest."),
    X_(TX_ORG_SPEED, "SLOTH SPEED", "How fast the track's SLOTH drifts (mods: TORPOR, APATHY, INERTIA): 50 is about 17, "
                                    "1 and 3 minutes a turn; each 12.5 more twice as fast."),
    X_(TX_ORG_RATE, "OCHD RATE", "How fast the track's eight OCHD LFOs run: 50 is 64 s for OCHD 1 down to 2.4 s for "
                                 "OCHD 8; each 12.5 more twice as fast."),
    X_(TX_QD_DIR, "QUAD DIRECTION", "How the track's playhead moves along its row of the grid: forward, back, back and "
                                    "forth, at random; HOLD rests on its own steps and moves only when a LINK moves it."),
    X_(TX_QD_PROB, "QUAD PROB", "The chance each of the row's notes plays: 100 every one, lower leaves gaps."),
    X_(TX_QD_LINK, "QUAD LINK", "Another QUAD track this one's steps also move on, played for this step's gate: a "
                                "slow track pulled along by a fast one, or a HOLD track played only by this one."),
    X_(TX_NL_MODE, "NOTE LENGTH", "How long the generator holds its notes. STEP: the step's GATE. FIXED: LEN, whatever "
                                  "DIV. HIT: until the next hit, so rests let notes ring on. Grids keeps STEP."),
    X_(TX_NL_LEN, "NOTE LENGTH LEN", "FIXED's length, from a 32nd note to a bar (1/1), at the tempo the note starts."),
    X_(TX_NL_KEYS, "NOTE LENGTH KEYS", "FIXED: how many notes may hold at once. One more ends the note nearest its end; "
                                       "the same pitch again ends the one before."),
    X_(TX_CS_Y, "CONSTELLATION Y", "The height of the ship's crosshair in the sky: the stars crossing it are caught into "
                                   "the loop, so it sets where the melody sits. AUTO steers it instead."),
    X_(TX_CS_DENS, "CONSTELLATION DENS", "How many stars the sky holds: more stars, more catches, a fuller loop."),
    X_(TX_CS_SIZE, "CONSTELLATION SIZE", "How many rows either side of the crosshair a star is still caught in: 0 only its "
                                         "own row, higher a wider melody. Set by a mod (no knob of its own)."),
    X_(TX_CS_AUTO, "CONSTELLATION AUTO", "Who steers the ship: OFF your Y; 1 to 9 a wander of its own, faster further up; "
                                         "SEEK flies towards the next star."),
};
#undef T_
#undef G_
#undef X_

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
