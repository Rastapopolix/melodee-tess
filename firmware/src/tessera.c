static const uint16_t MORPH_PRESETS[8] = {
    0x8888u, /* 4-on-floor */
    0x2222u, /* Offbeats */
    0xAAAAu, /* Straight 8ths */
    0x8924u, /* Son clave */
    0x8512u, /* Rumba clave */
    0x9249u, /* Syncopated 3-3-2 */
    0xA5A5u, /* Alternating sync */
    0xB6D7u  /* Dense poly */
};
#include "tessera.h"

tessera_state_t tessera;

/* Scale degree to semitone intervals */
static const uint8_t TESS_SCALE_MIN[8] = {0, 2, 3, 5, 7, 8, 10, 12};
static const uint8_t TESS_SCALE_MAJ[8] = {0, 2, 4, 5, 7, 9, 11, 12};
static const uint8_t TESS_SCALE_DOR[8] = {0, 2, 3, 5, 7, 9, 10, 12};
static const uint8_t TESS_SCALE_PEN[8] = {0, 3, 5, 7, 10, 12, 15, 17};

void tessera_markov_recalc(uint32_t tr)
{
    if (tr >= TESSERA_TRACKS) return;
    tessera_markov_t *mk = &tessera.markov[tr];
    uint8_t tens = mk->tension;

    for (uint32_t s = 0; s < 8; s++) {
        uint32_t raw[8];
        for (uint32_t d = 0; d < 8; d++) {
            int32_t dist = (int32_t)d - (int32_t)s;
            if (dist < 0) dist = -dist;

            if (dist == 0) {
                raw[d] = 15;
            } else if (dist == 1) {
                raw[d] = 50 - (tens * 30 / 100);
            } else if (dist == 2) {
                raw[d] = 25;
            } else {
                raw[d] = 5 + (tens * 25 / 100);
            }

            if (d == 0 || d == 7) {
                raw[d] += (100 - tens) / 5;
            }
        }

        uint32_t sum = 0;
        for (uint32_t d = 0; d < 8; d++) sum += raw[d];
        if (!sum) sum = 1;

        uint32_t accum = 0;
        for (uint32_t d = 0; d < 8; d++) {
            accum += (raw[d] * 100u) / sum;
            mk->matrix[s][d] = (accum > 100u || d == 7) ? 100 : (uint8_t)accum;
        }
    }
}

void tessera_init(void)
{
    uint8_t *p = (uint8_t *)&tessera;
    for (uint32_t b = 0; b < sizeof(tessera); b++) p[b] = 0;

    for (uint32_t i = 0; i < TESSERA_TRACKS; i++) {
        tessera.mode[i] = TESS_ENG_OFF;

        /* 1. Turing machine */
        tessera.turing[i].reg = 0xACE1u;
        tessera.turing[i].len = 16;
        tessera.turing[i].prob = 0;
        tessera.turing[i].oct_span = 2;

        /* 2. Euclidean */
        tessera.euclid[i].steps = 16;
        tessera.euclid[i].pulses = (i == 1) ? 5 : 4;
        tessera.euclid[i].rot = 0;

        /* 3. Markov */
        tessera.markov[i].cur_state = 0;
        tessera.markov[i].tension = 25;
        tessera.markov[i].density = 100;
        tessera.markov[i].oct_span = 2;
        tessera_markov_recalc(i);

        /* 4. Morph */
        tessera.morph[i].morph_pos = 0;
        tessera.morph[i].snap_a = 0x8888u;
        tessera.morph[i].snap_b = 0xA5A5u;
        tessera.morph[i].step_idx = 0;
        tessera.morph[i].oct_span = 2;
    }
}

int tessera_euclid_hit(uint8_t step, uint8_t steps, uint8_t pulses, uint8_t rot)
{
    if (!pulses || !steps) return 0;
    if (pulses >= steps) return 1;
    uint32_t s = (step + rot) % steps;
    return (((s + 1u) * (uint32_t)pulses) / (uint32_t)steps) > ((s * (uint32_t)pulses) / (uint32_t)steps);
}

void tessera_kill(track_t *t)
{
    uint32_t tr = trk_index(t);
    if (tr >= TESSERA_TRACKS) return;
    if (tessera.active_gate[tr]) {
        trk_note_off(t, tessera.last_note[tr]);
        tessera.active_gate[tr] = 0;
    }
    t->seq_n = 0;
    seq_release(t);
}

void tessera_stop_all(void)
{
    for (uint32_t i = 0; i < TESSERA_TRACKS; i++) {
        tessera_kill(&trk[i]);
    }
}

void tessera_clock_step(track_t *t)
{
    uint32_t tr = trk_index(t);
    if (tr >= TESSERA_TRACKS) return;

    if (t->p[P_MUTE]) {
        if (tessera.active_gate[tr]) {
            trk_note_off(t, tessera.last_note[tr]);
            tessera.active_gate[tr] = 0;
        }
        seq_release(t);
        return;
    }

    if (tessera.active_gate[tr]) {
        trk_note_off(t, tessera.last_note[tr]);
        tessera.active_gate[tr] = 0;
        seq_release(t);
    }

    uint8_t note = 60;
    int should_trigger = 0;
    uint8_t vel = 100;

    switch (tessera.mode[tr]) {
    case TESS_ENG_TURING: {
        tessera_turing_t *tur = &tessera.turing[tr];
        uint8_t len = tur->len ? tur->len : 16;
        // tur->step_idx = (tur->step_idx + 1) % len; (Removed: Turing is a shift register, not a step sequencer)

        uint16_t bit0 = (tur->reg >> (len - 1)) & 1u;
        if (tur->prob > 0 && (rng() % 100u) < tur->prob) {
            bit0 ^= 1u;
        }
        tur->reg = ((tur->reg << 1) | bit0) & 0xFFFFu;

        uint8_t span = tur->oct_span ? tur->oct_span : 1;
        uint16_t tmask = (len >= 16) ? 0xFFFFu : (uint16_t)((1u << len) - 1u);
        uint16_t mreg = tur->reg & tmask;
        uint8_t raw = mreg & 0xFFu;
        uint8_t idx = raw % 7;
        uint8_t oct = (raw / 7) % span;
        note = 36 + (oct * 12) + TESS_SCALE_MIN[idx];
        vel = 90 + ((mreg >> 8) & 0x1Fu);
        if (tur->reg & 1u) {
            should_trigger = 1;
        }
        break;
    }
    case TESS_ENG_EUCLID: {
        tessera_euclid_t *euc = &tessera.euclid[tr];
        uint8_t steps = euc->steps ? euc->steps : 16;
        uint8_t curr_step = euc->step_idx;
        euc->step_idx = (curr_step + 1u) % steps;

        if (tessera_euclid_hit(curr_step, steps, euc->pulses, euc->rot)) {
            note = 48 + ((curr_step * 3) % 12);
            should_trigger = 1;
            vel = 110;
        }
        break;
    }
    case TESS_ENG_MARKOV: {
        tessera_markov_t *mk = &tessera.markov[tr];
        uint32_t r = rng() % 100u;
        uint8_t next_state = 0;
        for (uint8_t i = 0; i < 8; i++) {
            if (r < mk->matrix[mk->cur_state][i]) {
                next_state = i;
                break;
            }
        }
        mk->cur_state = next_state;

        uint8_t m_oct = mk->oct_span ? mk->oct_span : 1;
        uint8_t root = (uint8_t)(t->p[P_ROOT] % 12);
        note = 36 + root + (next_state * 2) + (12 * (rng() % m_oct));
        vel = 95 + (rng() % 20u);
        uint32_t dens = mk->density ? mk->density : 100;
        if ((rng() % 100u) < dens) {
            should_trigger = 1;
        }
        break;
    }
    case TESS_ENG_MORPH: {
        tessera_morph_t *m = &tessera.morph[tr];
        uint8_t cur_s = (m->step_idx + 1u) & 0x0Fu;
        m->step_idx = (m->step_idx & 0xF0u) | cur_s;

        uint8_t hit_a = (m->snap_a >> cur_s) & 1u;
        uint8_t hit_b = (m->snap_b >> cur_s) & 1u;

        uint32_t roll = rng() % 128u;
        int hit = (roll < m->morph_pos) ? hit_b : hit_a;

        if (hit) {
            uint8_t deg_a = (cur_s * 2u) % 8u;
            uint8_t deg_b = (cur_s * 3u + 2u) % 8u;
            uint8_t chosen_deg = (roll < m->morph_pos) ? deg_b : deg_a;
            note = 48 + TESS_SCALE_MIN[chosen_deg];
            vel = 90 + ((cur_s % 4u == 0) ? 25 : 10);
            should_trigger = 1;
        }
        break;
    }
    }

    if (should_trigger) {
        int32_t final_note = tessera_snap_note(t, (int32_t)note);

        /* Apply track transpose and clamp to MIDI range */
        final_note += (int32_t)t->p[P_TRANS];
        if (final_note < 0) final_note = 0;
        if (final_note > 127) final_note = 127;

        trk_note_on(t, (uint32_t)final_note, vel);
        tessera.last_note[tr] = (uint8_t)final_note;
        tessera.active_gate[tr] = 1;
        t->seq_notes[0] = (uint8_t)final_note;
        t->seq_n = 1;

        uint32_t period = seq_div_samples((uint32_t)(t->p[P_SDIV] ? t->p[P_SDIV] : 1));
        t->seq_off = period * (uint32_t)(t->p[P_SGATE] ? t->p[P_SGATE] : 64) / 128u;
    }
}

void tessera_knob(uint32_t k, int32_t s)
{
    tessera_ensure_init();
    uint32_t tr = song.sel;
    if (tr >= TESSERA_TRACKS || !s) return;

    uint32_t mode = tessera.mode[tr];

    if (k == 0u) {
        int32_t m = (int32_t)mode + s;
        if (m < 0) m = 0;
        if (m >= TESS_ENG_COUNT) m = TESS_ENG_COUNT - 1;
        if ((uint8_t)m != tessera.mode[tr]) {
            seq_release(&trk[tr]);
            tessera.active_gate[tr] = 0;
        }
        tessera.mode[tr] = (uint8_t)m;
        return;
    }

    if (mode == TESS_ENG_TURING) {
        tessera_turing_t *tur = &tessera.turing[tr];
        if (k == 1u) {
            int32_t len = (int32_t)tur->len + s;
            if (len < 2) len = 2;
            if (len > 16) len = 16;
            tur->len = (uint8_t)len;
            if (tur->step_idx >= tur->len) tur->step_idx = 0;
        } else if (k == 2u) {
            int32_t p = (int32_t)tur->prob + (s * 2);
            if (p < 0) p = 0;
            if (p > 100) p = 100;
            tur->prob = (uint8_t)p;
        } else if (k == 3u) {
            int32_t oct = (int32_t)tur->oct_span + s;
            if (oct < 1) oct = 1;
            if (oct > 3) oct = 3;
            tur->oct_span = (uint8_t)oct;
        }
    } else if (mode == TESS_ENG_EUCLID) {
        tessera_euclid_t *euc = &tessera.euclid[tr];
        if (k == 1u) {
            int32_t st = (int32_t)euc->steps + s;
            if (st < 1) st = 1;
            if (st > 32) st = 32;
            euc->steps = (uint8_t)st;
            if (euc->pulses > euc->steps) euc->pulses = euc->steps;
            if (euc->rot >= euc->steps) euc->rot = euc->steps > 1 ? (euc->steps - 1) : 0;
        } else if (k == 2u) {
            int32_t p = (int32_t)euc->pulses + s;
            if (p < 0) p = 0;
            if (p > (int32_t)euc->steps) p = (int32_t)euc->steps;
            euc->pulses = (uint8_t)p;
        } else if (k == 3u) {
            int32_t max_r = euc->steps > 1 ? (int32_t)euc->steps - 1 : 0;
            int32_t r = (int32_t)euc->rot + s;
            if (r < 0) r = 0;
            if (r > max_r) r = max_r;
            euc->rot = (uint8_t)r;
        }
    } else if (mode == TESS_ENG_MARKOV) {
        tessera_markov_t *mk = &tessera.markov[tr];
        if (k == 1u) {
            int32_t tens = (int32_t)mk->tension + (s * 4);
            if (tens < 0) tens = 0;
            if (tens > 100) tens = 100;
            mk->tension = (uint8_t)tens;
            tessera_markov_recalc(tr);
        } else if (k == 2u) {
            int32_t dens = (int32_t)mk->density;
            if (dens < 5 || dens > 100) dens = 100;
            dens += (s * 5);
            if (dens < 5) dens = 5;
            if (dens > 100) dens = 100;
            mk->density = (uint8_t)dens;
        } else if (k == 3u) {
            int32_t oct = (int32_t)mk->oct_span + s;
            if (oct < 1) oct = 1;
            if (oct > 3) oct = 3;
            mk->oct_span = (uint8_t)oct;
        }
    } else if (mode == TESS_ENG_MORPH) {
        tessera_morph_t *m = &tessera.morph[tr];
        if (k == 1u) {
            int32_t pos = (int32_t)m->morph_pos + (s * 4);
            if (pos < 0) pos = 0;
            if (pos > 127) pos = 127;
            m->morph_pos = (uint8_t)pos;
        } else if (k == 2u) {
            int32_t pa = ((int32_t)(m->step_idx >> 4) & 7) + s;
            if (pa < 0) pa = 0;
            if (pa > 7) pa = 7;
            m->step_idx = (m->step_idx & 0x0Fu) | ((uint8_t)pa << 4);
            m->snap_a = MORPH_PRESETS[pa];
        } else if (k == 3u) {
            int32_t pb = ((int32_t)m->oct_span & 7) + s;
            if (pb < 0) pb = 0;
            if (pb > 7) pb = 7;
            m->oct_span = (uint8_t)pb;
            m->snap_b = MORPH_PRESETS[pb];
        }
    }
}

void tessera_ensure_init(void)
{
    static uint8_t done;
    if (!done) { done = 1; tessera_init(); }
}

uint32_t tessera_sig(uint32_t tr)
{
    if (tr >= TESSERA_TRACKS || tessera.mode[tr] == TESS_ENG_OFF) return 0;
    uint32_t h = tessera.mode[tr] * 1009u + tessera.last_note[tr] * 53u + tessera.active_gate[tr];
    switch (tessera.mode[tr]) {
    case TESS_ENG_TURING: { const tessera_turing_t *a = &tessera.turing[tr];
        h ^= a->reg * 31u + a->len * 7u + a->oct_span * 131u; break; }
    case TESS_ENG_EUCLID: { const tessera_euclid_t *a = &tessera.euclid[tr];
        h ^= a->steps * 31u + a->pulses * 7u + a->rot * 131u + a->step_idx * 17u; break; }
    case TESS_ENG_MARKOV: { const tessera_markov_t *a = &tessera.markov[tr];
        h ^= a->cur_state * 31u + a->tension * 7u + a->density * 131u; break; }
    case TESS_ENG_MORPH:  { const tessera_morph_t *a = &tessera.morph[tr];
        h ^= a->snap_a * 31u + a->snap_b * 7u + a->morph_pos * 131u + a->step_idx * 17u; break; }
    }
    return h;
}

/* Force a note down into the nearest active scale degree of the track */
int32_t tessera_snap_note(const track_t *t, int32_t n)
{
    uint32_t mask = scale_mask(t), guard = 12;
    while (guard-- && !((mask >> (uint32_t)((n - t->p[P_ROOT] + 120) % 12)) & 1u))
        n--;
    return n;
}

/* The pitch Euclid plays on a step, as 0..14 (mirrors tessera_clock_step: 48 + (step*3)%12, snapped) */
uint32_t tessera_euclid_pitch(const track_t *t, uint32_t step)
{
    int32_t n = tessera_snap_note(t, 48 + (int32_t)((step * 3u) % 12u));
    n -= 45;
    if (n < 0) n = 0;
    if (n > 14) n = 14;
    return (uint32_t)n;
}
