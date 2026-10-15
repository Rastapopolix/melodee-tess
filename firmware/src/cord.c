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
};
#undef V_
#undef B_
#undef S_

const cord_dest_t *cord_dest(uint32_t code)
{
    return code < CD_N && CORD_DESTS[code].name ? &CORD_DESTS[code] : 0;
}
