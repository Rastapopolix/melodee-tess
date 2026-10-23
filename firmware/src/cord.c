/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* Cords (cord.h): the destinations. Left out on purpose:
 *   GLIDE, UNISON DETUNE, VOICE MODE ..: read at a note-on (voice_alloc, mono_play), before the block's modulation
 *   FX SEND 2 and 3: read in mix_part before mod_begin
 *   ARP, PATTERN, SCALE ..: read by the sequencer, outside the block
 *   TEMPO and the other globals: one value for every track, a cord's amount would be every track's
 *   MODE, FORM, EVERY, ROLL, FOLLOW: a choice or a restart, not an amount to move by
 * tests/cord_test.c keeps the codes against tests/cord_dests.txt (frozen). */
#include "cord.h"

#define V_(c, md, n, f) {c, CK_VOICE, md, PID_NONE, n, f}
#define B_(c, p, n, f) {c, CK_BLOCK, 0, PID_TRACK(p), n, f}
#define S_(c, x, n, f) {c, CK_STEP, 0, PID_TESS(x), n, f}
static const cord_dest_t CORD_DESTS[CD_N] = {
    [CD_OFF] = {CD_OFF, CK_VOICE, 0, PID_NONE, "--", "nothing"},
    [CD_PITCH] = V_(CD_PITCH, MD_PITCH, "PITCH", "the pitch of each voice: 100 % at a full source is an octave"),
    [CD_CUTOFF] = V_(CD_CUTOFF, MD_CUT, "CUTOFF", "the filter (or the engine's brightness) of each voice"),
    [CD_SHAPE] = V_(CD_SHAPE, MD_SHP, "SHAPE", "the engine's SHAPE of each voice, as ENV DEST SHP moves it"),
    [CD_GAIN] = V_(CD_GAIN, MD_AMP, "GAIN", "each voice's loudness: up opens it from silence, down closes it"),
    [CD_LEVEL] = B_(CD_LEVEL, P_LEVEL, "LEVEL", "the track's level in the mix"),
    [CD_PAN] = B_(CD_PAN, P_PAN, "PAN", "the track's place left to right"),
    [CD_SPREAD] = B_(CD_SPREAD, P_SPRD, "SPREAD", "the stereo spread of the track's voices"),
    [CD_ATTACK] = B_(CD_ATTACK, P_ATK, "ATTACK", "the envelope's attack time"),
    [CD_DECAY] = B_(CD_DECAY, P_DEC, "DECAY", "the envelope's decay time"),
    [CD_SUSTAIN] = B_(CD_SUSTAIN, P_SUS, "SUSTAIN", "the envelope's sustain level"),
    [CD_RELEASE] = B_(CD_RELEASE, P_REL, "RELEASE", "the envelope's release time"),
    [CD_ENV_FLT] = B_(CD_ENV_FLT, P_ED_FLT, "ENV FLT", "how far the envelope moves the filter"),
    [CD_ENV_PIT] = B_(CD_ENV_PIT, P_ED_PIT, "ENV PIT", "how far the envelope moves the pitch"),
    [CD_ENV_SHP] = B_(CD_ENV_SHP, P_ED_SHP, "ENV SHP", "how far the envelope moves SHAPE"),
    [CD_LFO_RATE] = B_(CD_LFO_RATE, P_LRATE, "LFO RTE", "the track LFO's speed"),
    [CD_LFO_DEPTH] = {CD_LFO_DEPTH, CK_DEPTH, MD_DEPTH, PID_NONE, "LFO DEP", "the track LFO's depth, wherever it goes"},
    [CD_LFO_PIT] = B_(CD_LFO_PIT, P_LD_PIT, "LFO PIT", "how far the LFO moves the pitch (vibrato)"),
    [CD_LFO_FLT] = B_(CD_LFO_FLT, P_LD_FLT, "LFO FLT", "how far the LFO moves the filter"),
    [CD_LFO_SHP] = B_(CD_LFO_SHP, P_LD_SHP, "LFO SHP", "how far the LFO moves SHAPE"),
    [CD_LFO_AMP] = B_(CD_LFO_AMP, P_LD_AMP, "LFO AMP", "how far the LFO moves the level (tremolo)"),
    [CD_FX_SEND] = B_(CD_FX_SEND, P_FXSEND, "FX SEND", "how much of the track goes to the effects"),
    [CD_E1] = B_(CD_E1, P_E0, "E1", "the engine's first parameter (its name: the track's engine)"),
    [CD_E2] = B_(CD_E2, P_E1, "E2", "the engine's second parameter"),
    [CD_E3] = B_(CD_E3, P_E2, "E3", "the engine's third parameter"),
    [CD_E4] = B_(CD_E4, P_E3, "E4", "the engine's fourth parameter"),
    [CD_E5] = B_(CD_E5, P_E4, "E5", "the engine's fifth parameter"),
    [CD_E6] = B_(CD_E6, P_E5, "E6", "the engine's sixth parameter"),
    [CD_E7] = B_(CD_E7, P_E6, "E7", "the engine's seventh parameter"),
    [CD_E8] = B_(CD_E8, P_E7, "E8", "the engine's eighth parameter"),
    [CD_TUR_PROB] = S_(CD_TUR_PROB, TX_TUR_PROB, "T PROB", "Turing's PROB: how often its loop changes"),
    [CD_TUR_LEN] = S_(CD_TUR_LEN, TX_TUR_LEN, "T LEN", "Turing's loop length"),
    [CD_EUC_PULSES] = S_(CD_EUC_PULSES, TX_EUC_PULSES, "E PULSE", "Euclid's hits: more or fewer"),
    [CD_EUC_ROT] = S_(CD_EUC_ROT, TX_EUC_ROT, "E ROT", "Euclid's rotation"),
    [CD_MKV_LEAP] = S_(CD_MKV_LEAP, TX_MKV_LEAP, "M LEAP", "Markov's LEAP: how far its notes jump"),
    [CD_MKV_DENS] = S_(CD_MKV_DENS, TX_MKV_DENS, "M DENS", "Markov's density: how many steps play"),
    [CD_MRP_POS] = S_(CD_MRP_POS, TX_MRP_POS, "MP POS", "Morph's blend from rhythm A to B"),
    [CD_TW_SHIFT] = S_(CD_TW_SHIFT, TX_TW_SHIFT, "SHIFT", "TWEAK's SHIFT: transposes the generator's notes"),
    [CD_TW_DROP] = S_(CD_TW_DROP, TX_TW_DROP, "DROP", "TWEAK's DROP: the chance a hit is left out"),
    [CD_TW_HUMAN] = S_(CD_TW_HUMAN, TX_TW_HUMAN, "HUMAN", "TWEAK's HUMAN: velocity and gate variation"),
    [CD_EV_AMOUNT] = S_(CD_EV_AMOUNT, TX_EV_AMOUNT, "EV AMT", "EVOLVE's AMOUNT: how much each mutation changes"),
    [CD_FM_CONTRAST] = S_(CD_FM_CONTRAST, TX_FM_CONTRAST, "CONTR", "FORM's CONTRAST: how different a new section is"),
    [CD_GRD_X] = S_(CD_GRD_X, TX_GRD_X, "AT X", "Atlas' X: across its map of drum patterns"),
    [CD_GRD_Y] = S_(CD_GRD_Y, TX_GRD_Y, "AT Y", "Atlas' Y: down its map of drum patterns"),
    [CD_GRD_DENS] = S_(CD_GRD_DENS, TX_GRD_DENS, "AT DENS", "Atlas' density: how full its parts are"),
    [CD_BNC_SPEED] = S_(CD_BNC_SPEED, TX_BNC_SPEED, "B SPEED", "Bounce's speed: how far the balls move a step"),
    [CD_BNC_ANGLE] = S_(CD_BNC_ANGLE, TX_BNC_ANGLE, "B ANGLE", "Bounce's angle: the balls' paths (FALL: the wind)"),
    [CD_AWK_PROB] = S_(CD_AWK_PROB, TX_AWK_PROB, "A PROB", "Awake's PROB: the chance a note plays"),
    [CD_TW_BRANCH] = S_(CD_TW_BRANCH, TX_TW_BRANCH, "BRANCH", "TWEAK 2's BRANCH: the chance a hit plays on track TO"),
    [CD_QD_PROB] = S_(CD_QD_PROB, TX_QD_PROB, "Q PROB", "Quad's PROB: the chance a cell plays"),
    [CD_CS_Y] = S_(CD_CS_Y, TX_CS_Y, "C Y", "Constellation's Y: the crosshair's height (AUTO OFF)"),
    [CD_CS_SIZE] = S_(CD_CS_SIZE, TX_CS_SIZE, "C SIZE", "Constellation's SIZE: how far from the crosshair a star is caught"),
    [CD_CS_DENS] = S_(CD_CS_DENS, TX_CS_DENS, "C DENS", "Constellation's DENS: how many stars the sky holds"),
    [CD_SR_START] = S_(CD_SR_START, TX_SR_START, "S START", "Series' START: where in the infinity series it plays"),
    [CD_CS_AUTO] = S_(CD_CS_AUTO, TX_CS_AUTO, "C AUTO", "Constellation's AUTO: who steers the ship (OFF, a wander, SEEK)"),
    [CD_CH_PROG] = S_(CD_CH_PROG, TX_CH_PROG, "CH PROG", "Chord's PROG: which progression its chords follow"),
    [CD_GRD_KICK] = S_(CD_GRD_KICK, TX_GRD_KICK, "AT KICK", "Atlas' KICK: how much of its density the kick gets"),
    [CD_GRD_SNR] = S_(CD_GRD_SNR, TX_GRD_SNR, "AT SNR", "Atlas' SNR: how much of its density the snare gets"),
    [CD_GRD_HAT] = S_(CD_GRD_HAT, TX_GRD_HAT, "AT HAT", "Atlas' HAT: how much of its density the hat gets"),
    [CD_GRD_TOMS] = S_(CD_GRD_TOMS, TX_GRD_TOMS, "AT TOMS", "Atlas' TOMS: how many of the near misses play the tom"),
    [CD_AN_VARY] = S_(CD_AN_VARY, TX_AN_VARY, "AN VARY", "Answer's VARY: how much each answer changes the phrase"),
    [CD_SUB_HUMAN] = S_(CD_SUB_HUMAN, TX_SUB_HUMAN, "H TIME", "TIMING's HUMAN: how late a step may play"),
};
#undef V_
#undef B_
#undef S_

const cord_dest_t *cord_dest(uint32_t code)
{
    return code < CD_N && CORD_DESTS[code].name ? &CORD_DESTS[code] : 0;
}

/* ------------------------------------------------------------ sources --- */
_Static_assert(CS_LFO == MS_LFO && CS_SLEW == MS_SLEW && CS_EXPR == MS_EXPR, "CS_LFO .. CS_SLEW == mod.c's MS_*");
static const char *const CORD_SRC_NAME[CS_N] = {"--", "LFO", "ENV", "VEL", "KEY", "RAND", "MODW", "AT", "EXPR", "S&H",
                                                "SLEW", "GATE", "NOTE", "CV", "DRIFT", "TIDE 1", "TIDE 3",
                                                "TIDE 2", "SWARM 1", "SWARM 2", "SWARM 3", "SWARM 4", "SWARM 5", "SWARM 6",
                                                "SWARM 7", "SWARM 8", "ENERGY"};   /* (TIDE by speed: CS_TORPOR 1, CS_INERTIA 2, CS_APATHY 3) */
const char *cord_src_name(uint32_t code) { return code < CS_N ? CORD_SRC_NAME[code] : 0; }

/* DRIFT: each track's slow wander, -1..1: a point every 2^14 audio blocks (11.9 s), the way between them eased
 * (smoothstep), the tracks apart in time and in their points. Its clock: the blocks since power-on (cord_tick) */
static uint32_t cord_blocks;
void cord_tick(void)
{
    cord_blocks++;
    organic_tick();                                     /* (tessera.c, organic.c) */
    energy_tick();                                      /* (tessera.c, energy.c) */
}
static int32_t cord_point(uint32_t k)
{
    k = (k ^ (k >> 16)) * 0x7FEB352Du;
    k = (k ^ (k >> 15)) * 0x846CA68Bu;
    return clamp((int32_t)((k ^ (k >> 16)) & 0xFFFFu) - 32768, -32767, 32767);
}
static int32_t cord_drift(uint32_t tr)
{
    uint32_t b = cord_blocks + tr * 5101u, k = (b >> 14) + tr * 0x9E3779B9u, f = b & 16383u;
    int32_t a = cord_point(k), c = cord_point(k + 1u);
    int64_t w = ((int64_t)f * f * (3 * 16384 - 2 * (int64_t)f)) >> 28;   /* (Q14) */
    return a + (int32_t)(((int64_t)(c - a) * w) >> 14);
}

/* t's LFO -1..1 before its FADE, whatever its POL (voice.c folds POL UNI's to 0..1: unfolded back) */
static int32_t cord_lfo_raw(const track_t *t)
{
    int32_t v = t->lfo_val;
    if (t->p[P_LPOL]) v = v * 2 - 32768;
    return v > 32767 ? 32767 : v;
}
/* track tr's source s now (Q15): mod.c's (ENV: the latest note's envelope, one block late; the LFO with its FADE),
 * the generator's */
static int32_t cord_src(uint32_t s, uint32_t tr)
{
    const track_t *t = &trk[tr % NTRK];
    if (s == CS_GATE)
        return tr < TESSERA_TRACKS && tessera.mode[tr] != TESS_ENG_OFF && tessera.active_gate[tr] ? 32767 : 0;
    if (s == CS_NOTE)
        return tr < TESSERA_TRACKS && tessera.mode[tr] != TESS_ENG_OFF ? mod_key(tessera.last_note[tr]) : 0;
    if (s == CS_CV)
        return tr < TESSERA_TRACKS && tessera.mode[tr] != TESS_ENG_OFF ? tessera.cv[tr] : 0;
    if (s == CS_DRIFT)
        return cord_drift(tr % NTRK);
    if (s == CS_ENERGY)
        return energy_now();
    if (s >= CS_TORPOR)
        return organic_out(tr, s - CS_TORPOR);
    if (s == CS_LFO)
        return mulq15(cord_lfo_raw(t), t->lfo_fade);
    return mod_tsrc(t, s, mulq15(t->lfo_val, t->lfo_fade));
}
/* as mod.c's MS_BIPOLAR (the LFO bipolar whatever its POL: cord_lfo_raw) */
static int cord_bipolar(uint32_t s, uint32_t tr)
{
    (void)tr;
    return s == CS_NOTE || s == CS_DRIFT || s >= CS_TORPOR || s == CS_LFO || (s < CS_GATE && MS_BIPOLAR(s));   /* (CV: 0..1) */
}

/* a cord's polarity: its source bipolar, unless UNI folds it to 0..1. For the LFO the mod's BI/UNI alone decides: the
 * LFO page's POL shapes its own DEPTH routes, not the mods */
static int cord_bip(const cord_t *c) { return !c->uni && cord_bipolar(c->src, c->src_trk); }
static int cord_fold_on(const cord_t *c) { return c->uni && cord_bipolar(c->src, c->src_trk); }
static int32_t cord_fold(const cord_t *c, int32_t x) { return cord_fold_on(c) ? (x + 32768) >> 1 : x; }
/* a cord's source now, folded when UNI. A track's LFO is folded before its FADE and (own: the DEPTH gain of the track
 * the cord is on, 32767 none) its DEPTH scale it, as the LFO's own POL UNI is (voice.c): faded in or turned down, a UNI
 * mod goes to 0, not to half its amount */
static int32_t cord_val(const cord_t *c, int32_t own)
{
    if (!c->src) return 0;
    if (c->src == CS_LFO && cord_fold_on(c)) {
        const track_t *s = &trk[c->src_trk % NTRK];
        int32_t x = mulq15((cord_lfo_raw(s) + 32768) >> 1, s->lfo_fade);
        return own < 32767 ? mulq15(x, own) : x;
    }
    return cord_fold(c, cord_src(c->src, c->src_trk));
}

/* ------------------------------------------------------------ the cords --- */
static cord_t cords[CORD_MAX];
/* per destination track, the cords that reach it (bit i: cords[i]): mask in the audio block, smask at the step.
 * envsrc: bit t, a cord reads track t's ENV (its voices report it only with mod.c active: mod_begin keeps it on) */
static uint32_t cord_mask[NTRK], cord_smask[NTRK];
static uint8_t cord_envsrc;
/* cord_adopt: bit i, cords[i] came with track t's sound (a route of it, unedited since) and goes with it; sig the
 * sound it came with; fresh: the cords just loaded (a project's routes are the project's: kept) */
static uint16_t cord_adopted[NTRK];
static uint32_t cord_adopt_sig[NTRK];
static uint8_t cord_fresh = 1;

static int cord_live(const cord_t *c)
{
    return c->dst != CD_OFF && (c->amount || c->offset) && cord_dest(c->dst);
}
static void cord_masks(void)
{
    uint32_t i, t;
    for (t = 0; t < NTRK; t++) cord_mask[t] = cord_smask[t] = 0;
    cord_envsrc = 0;
    for (i = 0; i < CORD_LIVE; i++) {
        const cord_t *c = &cords[i];
        if (!cord_live(c)) continue;
        if (cord_dest(c->dst)->kind == CK_STEP) cord_smask[c->dst_trk] |= 1u << i;
        else cord_mask[c->dst_trk] |= 1u << i;
        if (c->src == CS_ENV) cord_envsrc |= (uint8_t)(1u << c->src_trk);
    }
}

int cord_set(uint32_t i, const cord_t *c)
{
    uint32_t t;
    if (i >= CORD_LIVE || c->src >= CS_N || c->src_trk >= NTRK || !cord_dest(c->dst) || c->dst_trk >= NTRK ||
        c->amount < -100 || c->amount > 100 || c->offset < -100 || c->offset > 100 || c->uni > 1u)
        return 0;
    TESS_IRQ_OFF();
    cords[i] = *c;
    cord_masks();
    TESS_IRQ_ON();
    for (t = 0; t < NTRK; t++)                          /* (edited: the project's now) */
        cord_adopted[t] &= (uint16_t)~(1u << i);
    return 1;
}
const cord_t *cord_get(uint32_t i) { return i < CORD_MAX ? &cords[i] : 0; }
/* every cord off, the interrupt left as it is (tessera_reset: a project load, NEW, a restore). The masks first: the
 * audio interrupt never finds a cord it was told of half cleared */
static void cord_reset(void)
{
    uint32_t t;
    for (t = 0; t < NTRK; t++) cord_mask[t] = cord_smask[t] = cord_adopted[t] = 0;
    cord_envsrc = 0;
    cord_fresh = 1;
    memset(cords, 0, sizeof cords);
}
void cord_clear(void)
{
    TESS_IRQ_OFF();
    cord_reset();
    TESS_IRQ_ON();
}

/* ------------------------------------------------------------ saved (project.c, the Tessera object) --- */
/* the cords as saved: CORD_MAX x 6 bytes, src src_trk dst dst_trk amount offset (CORD_LIVE in use, the rest kept 0);
 * UNI is src_trk's top bit (saved before it: 0, BI as they played) */
#define CORD_PK_BYTES (CORD_MAX * 6u)
static void cord_pack(uint8_t *out)
{
    uint32_t i;
    for (i = 0; i < CORD_MAX; i++, out += 6) {
        const cord_t *c = &cords[i];
        out[0] = c->src; out[1] = (uint8_t)(c->src_trk | c->uni << 7); out[2] = c->dst; out[3] = c->dst_trk;
        out[4] = (uint8_t)c->amount; out[5] = (uint8_t)c->offset;
    }
}
/* a saved block -> got: every cord checked (a code, a track, a percentage out of range): 1 all good, 0 refused */
static int cord_parse(const uint8_t *in, cord_t *got)
{
    uint32_t i;
    for (i = 0; i < CORD_MAX; i++, in += 6) {
        cord_t *c = &got[i];
        c->src = in[0]; c->src_trk = in[1] & 0x7Fu; c->uni = in[1] >> 7; c->dst = in[2]; c->dst_trk = in[3];
        c->amount = (int8_t)in[4]; c->offset = (int8_t)in[5];
        if (c->dst == CD_RETIRED_CL_RULE) memset(c, 0, sizeof *c);   /* (to CELLS' RULE, cut: the cord gone) */
        if (c->src >= CS_N || c->src_trk >= NTRK || c->dst >= CD_N || !cord_dest(c->dst) || c->dst_trk >= NTRK ||
            c->amount < -100 || c->amount > 100 || c->offset < -100 || c->offset > 100)
            return 0;
    }
    return 1;
}
/* the checks alone, nothing changed (project.c reads a record before it replaces anything) */
static int cord_check(const uint8_t *in)
{
    cord_t got[CORD_MAX];
    return cord_parse(in, got);
}
/* a saved block -> the cords: every cord checked first (refused whole, nothing changed); 1 loaded, 0 refused. Main
 * loop */
static int cord_unpack(const uint8_t *in)
{
    cord_t got[CORD_MAX];
    uint32_t i;
    if (!cord_parse(in, got)) return 0;
    TESS_IRQ_OFF();
    memcpy(cords, got, sizeof cords);
    cord_masks();
    TESS_IRQ_ON();
    for (i = 0; i < NTRK; i++) cord_adopted[i] = 0;
    cord_fresh = 1;
    return 1;
}
uint32_t cord_count(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < CORD_LIVE; i++) n += cord_live(&cords[i]);
    return n;
}

/* a cord's move, as a share of the destination's range (Q15, 32768 = all of it): amount x source + offset */
static int32_t cord_share(const cord_t *c)
{
    int32_t x = cord_val(c, 32767);
    return (x * c->amount) / 100 + (32768 * c->offset) / 100;
}
/* what the mods move parameter pid of track tr by now (Q15 of its range, as cord_share; the knob cards' live marks,
 * ui_draw.c). 0: no mod reaches it (the voice destinations PITCH .. GAIN, LFO DEP have no knob: never) */
static int cord_moves(uint32_t pid, uint32_t tr, int32_t *share)
{
    uint32_t i;
    int n = 0;
    *share = 0;
    for (i = 0; i < CORD_LIVE; i++) {
        const cord_t *c = &cords[i];
        if (c->dst_trk != tr || !cord_live(c) || cord_dest(c->dst)->pid != pid) continue;
        *share += cord_share(c);
        n = 1;
    }
    return n;
}
/* that share of a range of span steps, to the nearest (25 % of 0..32: 8) */
static int32_t cord_steps(int32_t share, int32_t span) { return (share * span + 16384) >> 15; }
/* mod.c's AMT (-64..64 = the whole of it) of a percentage */
static int32_t cord_amt(int32_t pct) { return (pct * 64) / 100; }

/* mod.c mod_begin, for track t (cord_mask: some cord reaches it): the LFO DEPTH cords on g (mod.c's DEPTH gain) */
static __attribute__((noinline)) int32_t cord_depth(const track_t *t, int32_t g)
{
    uint32_t m = cord_mask[t - trk], i;
    for (i = 0; m; i++, m >>= 1) {
        const cord_t *c = &cords[i];
        if (!(m & 1u) || c->dst != CD_LFO_DEPTH) continue;
        if (c->src && c->amount)
            g = mulq15(g, mod_gain(cord_bip(c) ? MS_LFO : MS_ENV, cord_val(c, 32767),
                                   cord_amt(c->amount)));
        if (c->offset)
            g = mulq15(g, mod_gain(MS_ENV, 32768, cord_amt(c->offset)));   /* (32768: the whole) */
    }
    return g;
}

/* mod.c mod_begin, for track t: the voice cords into mod's per-track terms, the parameter cords into off[] (the
 * offsets of mod.kid's parameters, as the MOD slots' are: by mod.c's sums, so a route made a cord moves as it did).
 * lfo: t's LFO as mod_begin has it (its DEPTH on it), for t's own LFO */
static __attribute__((noinline)) void cord_block(track_t *t, int32_t *off, int32_t lfo)
{
    uint32_t m = cord_mask[t - trk], i, j, tr = (uint32_t)(t - trk);
    for (i = 0; m; i++, m >>= 1) {
        const cord_t *c = &cords[i];
        const cord_dest_t *d;
        int32_t x;
        if (!(m & 1u)) continue;
        d = cord_dest(c->dst);
        if (c->src == CS_LFO && c->src_trk == tr && !cord_fold_on(c)) {   /* its own LFO, BI: as mod_begin has it */
            int32_t g = mod.ldep < 32767 ? mulq15(t->lfo_fade, mod.ldep) : t->lfo_fade;
            x = t->p[P_LPOL] ? lfo * 2 - g : lfo;      /* (POL UNI's 0..g back to -g..g: cord_lfo_raw) */
        } else
            x = cord_val(c, c->src_trk == tr ? mod.ldep : 32767);
        if (d->kind == CK_VOICE) {
            mod.amp |= d->md == MD_AMP;
            if (c->src && c->amount && MS_VOICE(c->src) && c->src_trk == (uint8_t)(t - trk) && !cord_fold_on(c) &&
                mod.nv < NMSLOT + CORD_LIVE) {          /* its own voices' ENV VEL KEY RAND: each voice its own, as */
                mod.vsrc[mod.nv] = c->src;              /* MOD's routes (mod_voice) */
                mod.vdst[mod.nv] = (uint8_t)d->md;
                mod.vamt[mod.nv++] = (int8_t)cord_amt(c->amount);
            } else if (c->src && c->amount)
                mod_voice_add(cord_bip(c) ? MS_LFO : MS_ENV, d->md, x, cord_amt(c->amount), &mod.pit,
                              &mod.cut, &mod.shp, &mod.gain);
            if (c->offset)
                mod_voice_add(MS_ENV, d->md, 32768, cord_amt(c->offset), &mod.pit, &mod.cut, &mod.shp, &mod.gain);
        } else if (d->kind == CK_BLOCK) {
            uint32_t id = d->pid;
            const param_desc_t *pd = id >= P_E0 ? &ENGINES[t->engine]->edit[id - P_E0] : &TP[id];
            for (j = 0; j < mod.nk && mod.kid[j] != id; j++)
                ;
            if (j == mod.nk) {
                mod.kid[j] = (uint8_t)id;
                mod.keep[j] = t->p[id];
                off[j] = 0;
                mod.nk++;
            }
            if (c->src && c->amount)                    /* (mod_begin's: AMT 64 the whole range) */
                off[j] += (((x * cord_amt(c->amount)) >> 6) * (pd->max - pd->min)) >> 15;
            if (c->offset)
                off[j] += cord_steps((32768 * c->offset) / 100, pd->max - pd->min);
        }
    }
}

/* ------------------------------------------------------------ the default patch --- */
/* a new project's mods: none (automation is opt-in; until 1.3.3 each track's CUTOFF and FX SEND wandered by DRIFT).
 * Old projects and templates keep their own (none: as saved) */
void cord_defaults(void)
{
    cord_clear();
}

/* ------------------------------------------------------------ MOD's routes --- */
/* Tessera: MODS (the cords) take the place of MOD's four routes a track. A route that comes in (a project or a sound
 * saved with one, the editor's) becomes a cord on its track, the same move: SRC the source, DST its destination
 * (VIB: LFO PIT, DEPTH: LFO DEP), AMT (-64..63, 64 = all) the percentage whose cord_amt is AMT again. The routes of a
 * sound go with it: loading another sound on the track takes the cords that came with the last one away, unless one
 * was edited. A project's are its own. Main loop (ui_input); returns the routes left (no free cord: they stay, and
 * play as before, until one is) */
static const uint8_t CORD_OF_MD[MD_N] = {[MD_PITCH] = CD_PITCH, [MD_CUT] = CD_CUTOFF, [MD_SHP] = CD_SHAPE,
    [MD_AMP] = CD_GAIN, [MD_PAN] = CD_PAN, [MD_RATE] = CD_LFO_RATE, [MD_VIB] = CD_LFO_PIT, [MD_E1] = CD_E1,
    [MD_E1 + 1] = CD_E2, [MD_E1 + 2] = CD_E3, [MD_E1 + 3] = CD_E4, [MD_E1 + 4] = CD_E5, [MD_E1 + 5] = CD_E6,
    [MD_E1 + 6] = CD_E7, [MD_E1 + 7] = CD_E8, [MD_DEPTH] = CD_LFO_DEPTH};
static uint8_t cord_sound_gen[NTRK];
void cord_sound_load(uint32_t tr) { cord_sound_gen[tr % NTRK]++; }   /* ui.c load_begin: a sound loads on track tr */
static int cord_empty(const cord_t *c) { return !c->src && !c->dst && !c->amount && !c->offset; }
uint32_t cord_adopt(void)
{
    uint32_t tr, k, i, left = 0, fresh = cord_fresh;
    cord_fresh = 0;
    for (tr = 0; tr < NTRK; tr++) {
        track_t *t = &trk[tr];
        uint32_t sig = cord_sound_gen[tr];
        if (cord_adopted[tr] && sig != cord_adopt_sig[tr]) {   /* another sound: the last one's cords go */
            TESS_IRQ_OFF();
            for (i = 0; i < CORD_LIVE; i++)
                if ((cord_adopted[tr] >> i) & 1u) memset(&cords[i], 0, sizeof cords[i]);
            cord_masks();
            TESS_IRQ_ON();
            cord_adopted[tr] = 0;
        }
        for (k = 0; k < 4u; k++) {
            int16_t *p = &t->p[P_M1SRC + 3u * k];
            uint32_t s = (uint32_t)p[0], d = (uint32_t)p[1];
            int32_t a = p[2], pct = (a < 0 ? -a : a) * 100;
            cord_t c = {0};
            if (!p[0] && !p[1] && !p[2])
                continue;
            if (s && s < MS_N && d < MD_N && CORD_OF_MD[d] && a) {
                c.src = (uint8_t)s; c.src_trk = (uint8_t)tr;
                c.dst = CORD_OF_MD[d]; c.dst_trk = (uint8_t)tr;
                pct = clamp((pct + 63) / 64, 0, 100);   /* (up: cord_amt rounds down, to AMT again) */
                c.amount = (int8_t)(a < 0 ? -pct : pct);
                for (i = 0; i < CORD_LIVE && memcmp(&cords[i], &c, sizeof c); i++)
                    ;                                   /* (the same cord there: an undo, the sound again) */
                if (i == CORD_LIVE)
                    for (i = 0; i < CORD_LIVE && !cord_empty(&cords[i]); i++)
                        ;
                if (i == CORD_LIVE) {
                    left++;
                    continue;
                }
            } else
                i = CORD_LIVE;                          /* (a route that moves nothing: gone) */
            TESS_IRQ_OFF();
            if (i < CORD_LIVE) {
                cords[i] = c;
                cord_masks();
            }
            p[0] = p[1] = p[2] = 0;
            TESS_IRQ_ON();
            if (i < CORD_LIVE && !fresh) {
                cord_adopted[tr] |= (uint16_t)(1u << i);
                cord_adopt_sig[tr] = sig;
            }
        }
    }
    return left;
}

/* ------------------------------------------------------------ at the generator's step --- */
static struct { uint8_t n, x[CORD_MAX]; int16_t base[CORD_MAX], set[CORD_MAX]; } cord_held;

/* tessera_clock_step, before the step: the Tessera cords of track tr into its settings (tessera_param_poke: no side
 * effect; PULSES within STEPS ..) */
static __attribute__((noinline)) void cord_step_begin(uint32_t tr)
{
    uint32_t m = cord_smask[tr], i, j;
    int32_t off[CORD_MAX];
    cord_held.n = 0;
    for (i = 0; m; i++, m >>= 1) {
        const cord_t *c = &cords[i];
        const cord_dest_t *d;
        const param_desc_t *pd;
        uint32_t x;
        if (!(m & 1u)) continue;
        d = cord_dest(c->dst);
        x = d->pid - PID_TESS(0);
        pd = pid_desc(d->pid, tr);
        for (j = 0; j < cord_held.n && cord_held.x[j] != x; j++)
            ;
        if (j == cord_held.n) {
            cord_held.x[j] = (uint8_t)x;
            cord_held.base[j] = (int16_t)tessera_param_get(tr, x);
            off[j] = 0;
            cord_held.n++;
        }
        off[j] += cord_steps(cord_share(c), pd->max - pd->min);
    }
    for (j = 0; j < cord_held.n; j++) {
        tessera_param_poke(tr, cord_held.x[j], cord_held.base[j] + off[j]);
        cord_held.set[j] = (int16_t)tessera_param_get(tr, cord_held.x[j]);
    }
}
/* after it: the stored settings back; one the step itself changed (an EVOLVE mutation): that change kept */
static __attribute__((noinline)) void cord_step_end(uint32_t tr)
{
    uint32_t j;
    for (j = 0; j < cord_held.n; j++) {
        int32_t now = tessera_param_get(tr, cord_held.x[j]);
        tessera_param_poke(tr, cord_held.x[j], now == cord_held.set[j] ? cord_held.base[j] :
                                               now - (cord_held.set[j] - cord_held.base[j]));
    }
    cord_held.n = 0;
}
/* a Tessera setting of track tr as its cords make it now (seq_sub.c: HUMAN, outside the generator's step) */
static int32_t cord_tess_get(uint32_t tr, uint32_t x)
{
    int32_t v;
    if (tr >= TESSERA_TRACKS || !cord_smask[tr]) return tessera_param_get(tr, x);
    cord_step_begin(tr);
    v = tessera_param_get(tr, x);
    cord_step_end(tr);
    return v;
}
