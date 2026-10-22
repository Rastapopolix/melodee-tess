/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess)
 * RATCH after Felucca's per-step RATCH (Leo Kuroshita, Hügelton Instruments; github.com/hugelton/Felucca, 1.4-1.6.1,
 * GPL-3.0-only): the idea and its x1..x4 in equal parts of a step (x8 ours), our own code */
/* SUB, sub-step timing (Tessera; included by seq.c): a step's notes again within the step. RATCH plays them HITS times
 * (x1..x8) in equal parts of the step, after Felucca's per-step RATCH, each hit its share of the step's gate. A step records
 * the notes it plays on its own voice (seq_sub_note), the sequencer arms the repeats after it (seq_sub_arm) and fires
 * them as the step goes on (seq_sub_tick, from seq_grid_tick). x1, or nothing recorded: nothing armed, the step plays
 * as before. Generator tracks: the track's RATCH (the TIMING page); STEP tracks: each step's (step_ratch, its sheet).
 * Not repeated: notes NOTE LENGTH holds (FIXED, HIT), BRANCHed and LINKed notes (another track's voice), and a
 * step that slides or ties into the next. In MIDI clock mode the positions are its musical units, as seq_pos's */
#define SUB_NOTES (4u + NLANE)
#define SUB_HITS_MAX 8u
#define SUB_GATE_MIN 32u                                /* samples: a repeat always sounds */
#ifndef SUB_NOTE_ON
#define SUB_NOTE_ON trk_note_on                         /* (tests/hostsim.c: logged as a generator's notes) */
#endif
typedef struct {
    uint32_t next, every, gate;     /* seq_pos of the next repeat, the space between repeats, each hit's gate */
    uint8_t left, n;                /* repeats still to come; the notes recorded */
    uint8_t note[SUB_NOTES], vel[SUB_NOTES];
} seq_sub_t;
static seq_sub_t seq_sub[NTRK] __attribute__((section(".pool")));
static uint8_t seq_sub_rec;         /* bit per track: its step is recording into seq_sub[] (the pool is not cleared at boot) */
static uint8_t seq_sub_armed;       /* bit per track: repeats to come */

/* PLAY, STOP: nothing recorded or armed on any track */
static void seq_sub_clear(void) { seq_sub_rec = seq_sub_armed = 0; }

/* A step starts: what it plays is recorded from here, the last step's repeats are dropped */
static void seq_sub_begin(const track_t *t)
{
    uint32_t tr = trk_index(t);
    seq_sub[tr].n = 0;
    seq_sub[tr].left = 0;
    seq_sub_rec |= (uint8_t)(1u << tr);
    seq_sub_armed &= (uint8_t)~(1u << tr);
}

/* A note the step plays on its own voice: repeated if the step ratchets (a note already recorded is not doubled) */
static void seq_sub_note(const track_t *t, uint32_t note, uint32_t vel)
{
    uint32_t tr = trk_index(t), i;
    seq_sub_t *s = &seq_sub[tr];
    if (!(seq_sub_rec & (1u << tr)) || s->n >= SUB_NOTES) return;
    for (i = 0; i < s->n; i++)
        if (s->note[i] == note) return;
    s->note[s->n] = (uint8_t)note;
    s->vel[s->n] = (uint8_t)(vel < 1u ? 1u : vel > 127u ? 127u : vel);
    s->n++;
}

/* Something to repeat: notes recorded and still sounding (seq_notes, the step's GATE), no slide or tie held over */
static int seq_sub_can(const track_t *t)
{
    uint32_t tr = trk_index(t);
    return (seq_sub_rec & (1u << tr)) && seq_sub[tr].n && t->seq_n && !t->seq_hold;
}

/* After the step played: hits (1..8) in equal parts of span (the step's length, swing included); the first hit, which
 * sounds already, gets its share of the gate too. Recording ends here */
static void seq_sub_arm(track_t *t, uint32_t hits, uint32_t span)
{
    uint32_t tr = trk_index(t);
    seq_sub_t *s = &seq_sub[tr];
    int can = seq_sub_can(t);
    seq_sub_rec &= (uint8_t)~(1u << tr);
    if (!can || hits < 2u) return;
    if (hits > SUB_HITS_MAX) hits = SUB_HITS_MAX;
    s->every = span / hits;
    s->gate = t->seq_off / hits;
    if (s->gate > s->every) s->gate = s->every;
    if (s->gate < SUB_GATE_MIN) s->gate = SUB_GATE_MIN;
    s->next = s->every;                                 /* (from the step's start, its 0: a block late is still in time) */
    s->left = (uint8_t)(hits - 1u);
    t->seq_off = s->gate;
    seq_sub_armed |= (uint8_t)(1u << tr);
}

/* As the step goes on (seq_pos moved, cur_len the step's length): each repeat due plays the recorded notes again */
static void seq_sub_tick(track_t *t, uint32_t cur_len)
{
    uint32_t tr = trk_index(t), i;
    seq_sub_t *s = &seq_sub[tr];
    if (!(seq_sub_armed & (1u << tr))) return;
    while (s->left && t->seq_pos >= s->next && s->next < cur_len) {
        seq_release(t);
        for (i = 0; i < s->n; i++) {
            SUB_NOTE_ON(t, s->note[i], s->vel[i]);
            t->seq_notes[i] = s->note[i];
        }
        t->seq_n = s->n;
        t->seq_off = s->gate;
        s->next += s->every;
        s->left--;
    }
    if (!s->left) seq_sub_armed &= (uint8_t)~(1u << tr);
}
