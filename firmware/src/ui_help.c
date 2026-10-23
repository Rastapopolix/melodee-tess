/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* Help (Theo 2026-10-16; tessera-notes/docs/design/phase2/help.png): ARP held, a knob turned shows what it does, its value left as it
 * is: a card over the page, dimmed, with the parameter's name, its value and its text (pid.c). Letting ARP go closes it
 * (and opens no page: ui_input.c). The FM-1's knobs cannot sense a touch, so a turn it is; turning the same knob on
 * scrolls a text longer than the card, back up again.
 * Included by ui_pages.c. */

/* The pages' own knobs, not parameters (pid.c has those): what each one does there, as ui_input.c's handlers do it.
 * The device build packs the texts with pid.c's (tools/gen_help.py reads HELP_PAGE: its entries in HP_ order) */
enum {
    HP_NONE,                                            /* (help_find's fallback: ui_test fails on it) */
    HP_UNUSED, HP_LIST_ROW, HP_CPU, HP_TIMING_STEP, HP_SCALE_FAMILY, HP_SCALE_FAV,
    HP_SLOT_PROJECT, HP_SLOT_USER, HP_SLOT_STORE, HP_MOTION_PLAY, HP_MOTION_LANE,
    HP_NOTES_STEP, HP_NOTES_NOTE, HP_NOTES_LENGTH, HP_NOTES_VEL, HP_GRID_STEP, HP_GRID_LANE, HP_GRID_HIT, HP_GRID_ACC,
    HP_HIT_STEP, HP_HIT_DRUM, HP_HIT_PITCH, HP_HIT_LENGTH,
    HP_DRUM_SOUND, HP_DRUM_TUNE, HP_DRUM_DECAY, HP_DRUM_CHAR, HP_DRUM_LEVEL,
    HP_PATTERNS, HP_SONG_SECTION, HP_SONG_TRACK, HP_SONG_PATTERN, HP_SONG_REPEATS,
    HP_BROWSE_LIST, HP_BROWSE_SOUND, HP_BROWSE_FAV, HP_BROWSE_ENGINE,
    HP_CORD_SRC, HP_CORD_MOD, HP_CORD_DST, HP_CORD_AMT, HP_MX_FROM, HP_MX_TO, HP_MX_MOD, HP_MX_OPEN,
    HP_SV_CLEAR, HP_FORM_CLEAR, HP_QUAD_COL, HP_AWAKE_A, HP_AWAKE_B, HP_POLY_RHY, HP_CONSTEL_SPEED,
    HP_FMOP,                                            /* FM6's operator pages: FP_R1 .. FP_OP from here */
    HP_FMVOICE = HP_FMOP + FP_OP + 1,                   /* .. the voice's: FP_PR1 .. FP_TRNSP */
    HP_FMFN = HP_FMVOICE + FP_TRNSP + 1 - FP_PR1,       /* .. the functions: FN_PBUP .. FN_ENGINE */
    HP_COUNT = HP_FMFN + FM6_NFN
};
#if MELODEE_HELP_PACKED
typedef struct { const char *name; } help_t;
#define H_(id, n, t) [id] = {n}
#else
typedef struct { const char *name, *text; } help_t;
#define H_(id, n, t) [id] = {n, t}
#endif
static const help_t HELP_PAGE[HP_COUNT] = {
    H_(HP_NONE, "KNOB", "No help text for this one yet."),
    H_(HP_UNUSED, "KNOB", "Does nothing on this page."),
    H_(HP_LIST_ROW, "LIST ROW", "Which row of the list KNOB 2 sets. OCT+ on a row opens its whole list of values (on "
                                "Scale: the SCALES page)."),
    H_(HP_CPU, "CPU", "How busy the FM-1 is making the sound, in percent; nothing to set. Near 100, notes may drop: "
                      "fewer voices, effects or tracks playing help."),
    H_(HP_TIMING_STEP, "TIMING", "A generator's: on a STEP track each step has its own ratchet and chance (OCT+ on a "
                                 "note: its sheet). KNOB 3, SHAPE, shapes the steps' ratchets here too."),
    H_(HP_SCALE_FAMILY, "SCALE FAMILY", "Narrows the scales KNOB 2 picks from: all of them, Western, EDO, just, "
                                        "harmonic, historical, regional, non-octave, maqam, or the favourites."),
    H_(HP_SCALE_FAV, "SCALE FAVOURITE", "Right marks the track's scale a favourite, left unmarks it; SCALES lists the "
                                        "favourites together."),
    H_(HP_SLOT_PROJECT, "PROJECT SLOT", "Which project slot, A to D, then the template and New song. OCT+ opens its "
                                        "sheet: Load, Save here, Rename, Boot (the project the FM-1 starts with), "
                                        "Erase; on New song it starts one."),
    H_(HP_SLOT_USER, "USER SLOT", "Which user preset slot. OCT+ opens its sheet: Load, Save here, Rename, Send (over "
                                  "USB), Erase."),
    H_(HP_SLOT_STORE, "STORE SLOT", "Which user preset slot the FM6 sound goes to. OCT+ opens its sheet: Save here, "
                                    "Send (the patch over MIDI), Init sound (a plain sine to start from)."),
    H_(HP_MOTION_PLAY, "MOTION PLAY", "Right plays the knob moves recorded on the track's pattern, left stops them "
                                      "(they stay recorded). OCT+ opens the lane's sheet: Play, Clear the lane, Clear "
                                      "all motion."),
    H_(HP_MOTION_LANE, "MOTION LANE", "Which recorded parameter's lane is picked (the sheet's Clear clears it); the "
                                      "lanes scroll with it."),
    /* SEQ > NOTES (ui_input.c step_edit, notes_event_edit); on a DRUM track the grid (grid_edit) */
    H_(HP_NOTES_STEP, "NOTES STEP", "Moves along the pattern: one note at a time where notes are, one step at a time "
                                    "between them, empty steps too. OCT+ on a note opens its sheet: length, velocity, "
                                    "chance, ratchet, nudge, slide, move, copy, delete. The PRESETS knob zooms."),
    H_(HP_NOTES_NOTE, "NOTES NOTE", "The picked note's pitch, a semitone a detent. On an empty step the first turn "
                                    "writes the last note played there."),
    H_(HP_NOTES_LENGTH, "NOTES LENGTH", "How long the picked note lasts, a quarter of a step a detent; with ENV held, "
                                        "a sixteenth, for fine lengths. A long note ties over the steps after it."),
    H_(HP_NOTES_VEL, "NOTES VELOCITY", "How hard the picked note plays, 1 to 127. With ENV held instead: right slides "
                                       "into the next note, left plays it plain."),
    H_(HP_GRID_STEP, "GRID STEP", "The cursor: the step KNOB 3 and 4 edit. A white key toggles the picked lane's hit "
                                  "on its step of the page directly; black keys 10 and 11 turn the page."),
    H_(HP_GRID_LANE, "GRID LANE", "Which drum lane KNOB 3 and 4 edit: kick, snare, clap, hats, tom, rim, bell. The "
                                  "black keys 1 to 8 pick one too, and play it."),
    H_(HP_GRID_HIT, "GRID HIT", "Right puts a hit of the lane on the cursor's step, left takes it off."),
    H_(HP_GRID_ACC, "GRID ACCENT", "Right accents the lane's hit on the cursor's step (it plays louder), left takes the "
                                   "accent off. Black key 9 held with a white key does the same."),
    /* SEQ > DRUM HIT: one hit (drum_hit_edit) */
    H_(HP_HIT_STEP, "HIT STEP", "Moves through the hits one at a time, and over the steps between them. A white key "
                                "picks a step too, and adds or removes the hit there."),
    H_(HP_HIT_DRUM, "HIT DRUM", "Which of the kit's sounds the hit is: the next hit edited or added on this step is "
                                "that one."),
    H_(HP_HIT_PITCH, "HIT PITCH", "Tunes this one hit up or down, up to two octaves, in semitones; the kit's sound "
                                  "stays as it is."),
    H_(HP_HIT_LENGTH, "HIT LENGTH", "How long this one hit lasts, a sixteenth of a step a detent; FREE lets the sound "
                                    "ring its own length."),
    /* EDIT > DRUM SOUND, DRUM MIX (drum_sound_edit) */
    H_(HP_DRUM_SOUND, "DRUM SOUND", "Which of the kit's sounds the other knobs shape. Each sound keeps its own "
                                    "settings, saved with the project."),
    H_(HP_DRUM_TUNE, "DRUM TUNE", "The sound's pitch, up to two octaves up or down, in semitones."),
    H_(HP_DRUM_DECAY, "DRUM DECAY", "How long the sound rings."),
    H_(HP_DRUM_CHAR, "DRUM CHARACTER", "The sound's own colour, named by the sound: the snare's SNAPPY (its wires), "
                                       "the kick's and toms' ATTACK (their click), the others' TONE (brighter or "
                                       "darker)."),
    H_(HP_DRUM_LEVEL, "DRUM LEVEL", "The sound's level in the kit."),
    /* SEQ > PATTERNS (ui_patterns.c patgrid_edit), SONG (edit_param) */
    H_(HP_PATTERNS, "PATTERN", "This track's pattern, one of its 8: stopped, it switches at once; playing, at the end "
                               "of its bar (it breathes on the grid till then); while a song plays, not at all. OCT+ "
                               "opens the sheet: Copy to (then any knob picks the place), Delete pattern."),
    H_(HP_SONG_SECTION, "SONG SECTION", "Which section of the song, a row, the other knobs edit; the empty row after "
                                        "the last adds one. PLAY (or OCT+) plays the song; REC held takes the "
                                        "patterns playing as the song (TAKE JAM)."),
    H_(HP_SONG_TRACK, "SONG TRACK", "Which track KNOB 3 sets the pattern of (the track selected, as ALGORITHM "
                                    "does)."),
    H_(HP_SONG_PATTERN, "SONG PATTERN", "The pattern the track plays in this section. Turned on the empty row, it "
                                        "adds the section."),
    H_(HP_SONG_REPEATS, "SONG REPEATS", "How many times the section plays before the next one, 1 to 16."),
    /* SAVE > PRESETS (the browser) */
    H_(HP_BROWSE_LIST, "PRESETS LIST", "Which list the sounds come from: all of them, the favourites, the recent "
                                       "ones, or a category (bass, lead, pad, keys, drums ..)."),
    H_(HP_BROWSE_SOUND, "PRESETS SOUND", "The sounds in the list (the PRESETS knob too); the one shown loads when the "
                                         "knob rests. OCT+ keeps it, OCT- goes back to the sound from before."),
    H_(HP_BROWSE_FAV, "PRESETS FAVOURITE", "Right marks the sound shown a favourite, left unmarks it; FAV lists "
                                           "them."),
    H_(HP_BROWSE_ENGINE, "PRESETS ENGINE", "Switches the track to the next or previous sound engine, at its first "
                                           "sound."),
    /* MODS (the cords: ui_cords.c), MATRIX (ui_matrix.c) */
    H_(HP_CORD_SRC, "MOD SOURCE", "What moves it: a track's LFO, envelope, velocity, key, random, controllers, its "
                                  "generator's gate, note or CV, or DRIFT, a slow wander, or its TIDE 1-3 or SWARM "
                                  "1-8 (LFO > ORGANIC). -- leaves the mod a fixed move. OCT+: BI swings both ways "
                                  "from the knob, UNI (+) only one way."),
    H_(HP_CORD_MOD, "MOD", "Which of the 16 mods the other knobs edit."),
    H_(HP_CORD_DST, "MOD DESTINATION", "What it moves, on which track: the voices' pitch, cutoff, shape, gain, a sound "
                                       "setting, or a generator's."),
    H_(HP_CORD_AMT, "MOD AMOUNT", "How far, and which way: at a full source, 100 % is the destination's whole range "
                                  "(pitch: an octave)."),
    H_(HP_MX_FROM, "MATRIX FROM", "The track the mods come from: a row of the grid. A lit cell: a mod goes from this "
                                  "track to that one, as bright as it moves its destination now."),
    H_(HP_MX_TO, "MATRIX TO", "The track the mods go to: a column of the grid."),
    H_(HP_MX_MOD, "MATRIX MOD", "Which of the cell's mods, when it has more than one (its number in MODS)."),
    H_(HP_MX_OPEN, "MATRIX OPEN", "OCT+ opens that mod in MODS, to change it."),
    /* GEN, EVOLVE, FORM: the knobs that are no setting */
    H_(HP_SV_CLEAR, "STEP EVOLVE CLEAR", "Turn: the steps play as written again; the next round starts afresh. REC "
                                         "held instead keeps the changes playing: they become the steps (EDIT held "
                                         "undoes it)."),
    H_(HP_FORM_CLEAR, "FORM CLEAR", "Forgets sections B and C: the phrase playing is A again."),
    H_(HP_QUAD_COL, "QUAD COL", "Which of the grid's four columns KNOB 2 (MOVE) and KNOB 3 (EVERY) set. The picture "
                                "marks it."),
    H_(HP_AWAKE_A, "AWAKE STEP A", "The picked step of loop A: a rest, or a degree of the scale, 1 to 8. KNOB 2 sets "
                                   "the same step of B. The same key again lets the step go (KNOB 1 is MODE again)."),
    H_(HP_AWAKE_B, "AWAKE STEP B", "The picked step of loop B: it lifts A's note by 0 to 7 degrees of the scale. The "
                                   "same key again lets the step go (KNOB 2 is LEN A again)."),
    H_(HP_POLY_RHY, "POLY RHY", "Which of the four rhythms DIV sets."),
    H_(HP_CONSTEL_SPEED, "CONSTELLATION SPEED", "How fast the sky flies by: a column a step, so this is the track's "
                                                "DIV (PATTERN's too), from 4 bars a step to 1/32, straight divisions "
                                                "only, so the notes land on the beat. Hold OCT+ for LOCK, LEN, FILL "
                                                "and SIZE."),
    /* FM6's pages (params.c FM6_OPD, FM6_GD, FM6_FD): an operator's (OP1 .. OP6: the PRESETS knob picks it) */
    H_(HP_FMOP + FP_R1, "EG RATE 1", "How fast the operator's envelope rises from L4 to L1 when a key goes down, its "
                                     "attack: 99 at once, 0 very slowly."),
    H_(HP_FMOP + FP_R1 + 1, "EG RATE 2", "How fast the envelope then moves from L1 to L2."),
    H_(HP_FMOP + FP_R1 + 2, "EG RATE 3", "How fast it then moves from L2 to L3, where it stays while the key is held."),
    H_(HP_FMOP + FP_R1 + 3, "EG RATE 4", "How fast it returns to L4 once the key is let go: the release."),
    H_(HP_FMOP + FP_L1, "EG LEVEL 1", "The envelope's first level, reached at R1's speed: 99 the operator at full, 0 "
                                      "silent."),
    H_(HP_FMOP + FP_L1 + 1, "EG LEVEL 2", "The envelope's second level, reached at R2's speed."),
    H_(HP_FMOP + FP_L1 + 2, "EG LEVEL 3", "The level the envelope holds while the key is held: the sustain."),
    H_(HP_FMOP + FP_L1 + 3, "EG LEVEL 4", "The level the envelope starts from and returns to after the key is let go; "
                                          "above 0 the operator never quite stops."),
    H_(HP_FMOP + FP_BP, "BREAK POINT", "The key where LDEP and RDEP start to change the operator's level: LDEP below "
                                       "it, RDEP above it."),
    H_(HP_FMOP + FP_LD, "LEFT DEPTH", "How much the operator's level changes for keys below the break point, the "
                                      "further down the more (LCRV: which way)."),
    H_(HP_FMOP + FP_RD, "RIGHT DEPTH", "How much the operator's level changes for keys above the break point, the "
                                       "further up the more (RCRV: which way)."),
    H_(HP_FMOP + FP_LC, "LEFT CURVE", "How the level changes below the break point: -LIN and -EXP lower it, +LIN and "
                                      "+EXP raise it; LIN evenly, EXP faster further out."),
    H_(HP_FMOP + FP_RC, "RIGHT CURVE", "How the level changes above the break point: -LIN and -EXP lower it, +LIN and "
                                       "+EXP raise it; LIN evenly, EXP faster further out."),
    H_(HP_FMOP + FP_RS, "RATE SCALING", "Higher keys run the operator's envelope faster, as a string dies sooner up "
                                        "high; 0 every key the same."),
    H_(HP_FMOP + FP_AMS, "AMP MOD SENS", "How much the FM LFO's AMD (and a controller sent to A) reaches this "
                                         "operator: 0 none, 3 the most."),
    H_(HP_FMOP + FP_KVS, "VELOCITY SENS", "How much playing harder raises the operator's level: 0 none, 7 the most. "
                                          "On a modulator, harder playing sounds brighter."),
    H_(HP_FMOP + FP_OL, "OUTPUT LEVEL", "The operator's level: a carrier's is how loud it is, a modulator's how much "
                                        "it brightens the operator it feeds (ALG shows which is which)."),
    H_(HP_FMOP + FP_MODE, "OSCILLATOR MODE", "RATIO: the operator's frequency follows the key, a multiple of it (CRS, "
                                             "FINE). FIXED: the same frequency on every key (CRS 1, 10, 100 or 1000 "
                                             "Hz)."),
    H_(HP_FMOP + FP_FC, "FREQ COARSE", "The operator's frequency: in RATIO a multiple of the key's, 0.5, 1, 2 .. 31; "
                                       "in FIXED 1, 10, 100 or 1000 Hz."),
    H_(HP_FMOP + FP_FF, "FREQ FINE", "Raises CRS's frequency by up to almost as much again; the value shows the whole "
                                     "ratio (or the Hz)."),
    H_(HP_FMOP + FP_DET, "DETUNE", "Detunes the operator a little, -7 to +7: a slight detune between operators beats "
                                   "slowly, a fuller sound."),
    H_(HP_FMOP + FP_OP, "OPERATOR ON", "Switches the operator off (silent, and it modulates nothing) or on: to hear "
                                       "what each one adds."),
    /* .. the voice's */
    H_(HP_FMVOICE, "PITCH EG RATE 1", "How fast the pitch moves from PL4 to PL1 when a key goes down."),
    H_(HP_FMVOICE + 1, "PITCH EG RATE 2", "How fast the pitch then moves from PL1 to PL2."),
    H_(HP_FMVOICE + 2, "PITCH EG RATE 3", "How fast it then moves to PL3, where it stays while the key is held."),
    H_(HP_FMVOICE + 3, "PITCH EG RATE 4", "How fast it returns to PL4 once the key is let go."),
    H_(HP_FMVOICE + 4, "PITCH EG LEVEL 1", "The pitch envelope's first level: 50 the note's own pitch, lower below "
                                           "it, higher above."),
    H_(HP_FMVOICE + 5, "PITCH EG LEVEL 2", "The pitch envelope's second level: 50 the note's own pitch."),
    H_(HP_FMVOICE + 6, "PITCH EG LEVEL 3", "The pitch held while the key is: 50 the note's own."),
    H_(HP_FMVOICE + 7, "PITCH EG LEVEL 4", "The pitch a note starts from and returns to after the key is let go: 50 "
                                           "the note's own."),
    H_(HP_FMVOICE + FP_ALG - FP_PR1, "ALGORITHM", "How the six operators are wired, 1 to 32: which are carriers, "
                                                  "heard, and which modulate others (the picture shows it)."),
    H_(HP_FMVOICE + FP_FB - FP_PR1, "FEEDBACK", "How much the operator with a loop modulates itself, 0 to 7: from a "
                                                "pure tone to buzz and noise."),
    H_(HP_FMVOICE + FP_OKS - FP_PR1, "OSCILLATOR SYNC", "ON starts every operator's wave afresh with each note, so "
                                                        "every attack sounds the same; OFF lets them run on."),
    H_(HP_FMVOICE + FP_LFS - FP_PR1, "FM LFO SPEED", "How fast the FM LFO moves."),
    H_(HP_FMVOICE + FP_LFD - FP_PR1, "FM LFO DELAY", "How long after a note starts the FM LFO fades in."),
    H_(HP_FMVOICE + FP_LPMD - FP_PR1, "FM LFO PITCH DEPTH", "How far the FM LFO moves the pitch, vibrato (PMS scales "
                                                            "it)."),
    H_(HP_FMVOICE + FP_LAMD - FP_PR1, "FM LFO AMP DEPTH", "How far the FM LFO moves the operators' levels, tremolo or "
                                                          "a wah on the modulators (each operator's AMS scales it)."),
    H_(HP_FMVOICE + FP_LKS - FP_PR1, "FM LFO KEY SYNC", "ON starts the FM LFO again with each note; OFF lets it run "
                                                        "on."),
    H_(HP_FMVOICE + FP_LFW - FP_PR1, "FM LFO WAVE", "The FM LFO's shape: triangle, saw down, saw up, square, sine, or "
                                                    "a random step each cycle (S&H)."),
    H_(HP_FMVOICE + FP_LPMS - FP_PR1, "PITCH MOD SENS", "How much PMD, and a controller sent to P, moves the pitch: 0 "
                                                       "none, 7 the most."),
    H_(HP_FMVOICE + FP_TRNSP - FP_PR1, "TRANSPOSE", "Moves the whole sound up or down, two octaves at most, in "
                                                    "semitones."),
    /* .. the functions (the track's own, saved with the project) */
    H_(HP_FMFN + FN_PBUP, "BEND UP", "How far the pitch bend wheel bends up, in semitones."),
    H_(HP_FMFN + FN_PBDN, "BEND DOWN", "How far the pitch bend wheel bends down, in semitones."),
    H_(HP_FMFN + FN_PBSTEP, "BEND STEP", "0 bends smoothly; above, the bend moves in steps of that many semitones."),
    H_(HP_FMFN + FN_PMODE, "PORTAMENTO", "When the pitch glides from note to note: PEDAL while the portamento pedal "
                                         "(MIDI CC 65) is down, ON always. TIME sets how long."),
    H_(HP_FMFN + FN_PTIME, "PORTAMENTO TIME", "How long a glide takes (MIDI CC 5 sets it too); 0 no glide."),
    H_(HP_FMFN + FN_GLISS, "GLISSANDO", "ON: a glide moves in semitone steps, not a smooth slide."),
    H_(HP_FMFN + FN_MWR, "WHEEL RANGE", "How far the mod wheel reaches, where W.DST sends it."),
    H_(HP_FMFN + FN_MWA, "WHEEL DESTINATION", "What the mod wheel moves: P the FM LFO's pitch depth (vibrato), A its "
                                              "amp depth (tremolo), E the operators' levels (by their AMS), or a "
                                              "mix."),
    H_(HP_FMFN + FN_FCR, "FOOT RANGE", "How far a foot controller (MIDI CC 4) reaches, where F.DST sends it."),
    H_(HP_FMFN + FN_FCA, "FOOT DESTINATION", "What the foot controller moves: P vibrato, A tremolo, E the operators' "
                                             "levels, or a mix."),
    H_(HP_FMFN + FN_BCR, "BREATH RANGE", "How far a breath controller (MIDI CC 2) reaches, where B.DST sends it."),
    H_(HP_FMFN + FN_BCA, "BREATH DESTINATION", "What the breath controller moves: P vibrato, A tremolo, E the "
                                               "operators' levels, or a mix."),
    H_(HP_FMFN + FN_ATR, "AFTERTOUCH RANGE", "How far pressing a held key harder (aftertouch) reaches, where A.DST "
                                             "sends it."),
    H_(HP_FMFN + FN_ATA, "AFTERTOUCH DEST", "What aftertouch moves: P vibrato, A tremolo, E the operators' levels, or "
                                            "a mix."),
    H_(HP_FMFN + FN_VNORM, "VELOCITY SCALING", "ON scales velocity to the narrower range classic FM sounds were made "
                                               "for; OFF uses all of MIDI's range."),
    H_(HP_FMFN + FN_ENGINE, "FM ENGINE", "How the FM is computed: MODRN clean, MARK I as the first hardware "
                                         "(grittier), OPL as the small FM chips (lo-fi)."),
};
#undef H_

static struct {
    uint8_t on, knob;
    uint8_t top;                                        /* the text's first line shown */
    uint8_t card;                                       /* 1: only the card changed (a scroll): drawn alone */
    uint16_t pid;                                       /* PID_NONE: a page's own knob, HELP_PAGE[hp] */
    uint16_t hp;
    const char *name;                                   /* (the pid's or HELP_PAGE's name, or one made up) */
} help;
static uint32_t help_tr;                                /* the track the card's value is (MIDI OUT: its knob's) */
static uint8_t help_inner;                              /* 1: help_find for a list's row (its page, not the list) */
static char help_nbuf[24];                              /* (a name made up: "KNOB 3", "OP2 EG RATE 1", "T3 PATTERN") */
static int list_help_row(uint32_t *page, uint32_t *slot);   /* ui_list.c */
static uint32_t slot_kind(void);                        /* ui_slots.c */
static int motion_page(void);                           /* ui_motion.c */

/* HELP_PAGE[hp]'s text (packed: pid.c's buffer, expanded after pid.c's texts, kept until another one's) */
static const char *help_page_text(uint32_t hp)
{
#if MELODEE_HELP_PACKED
    _Static_assert(HP_COUNT == HELP_PAGE_N, "build/gen/pid_help.h is another ui_help.c's (tools/gen_help.py)");
    return pid_help_at(PID_HELP_N + hp % HP_COUNT);
#else
    return HELP_PAGE[hp % HP_COUNT].text;
#endif
}
static void help_hp(uint32_t hp)
{
    help.pid = PID_NONE;
    help.hp = (uint16_t)hp;
    help.name = HELP_PAGE[hp].name;
}
static void help_named(uint32_t hp, const char *prefix, uint32_t digit)   /* "T3 PATTERN", "OP2 EG RATE 1", "KNOB 3":
                                                                          * the prefix's 1 the digit, then hp's name */
{
    uint32_t n = str_len(prefix), i;
    help_hp(hp);
    str_cpy(help_nbuf, prefix, sizeof help_nbuf);
    for (i = 0; i < n && help_nbuf[i] != '1'; i++)
        ;
    if (i < n) help_nbuf[i] = (char)('1' + digit);
    if (hp != HP_UNUSED)
        str_cpy(help_nbuf + n, HELP_PAGE[hp].name, sizeof help_nbuf - n);
    help.name = help_nbuf;
}
static void help_unused(uint32_t k) { help_named(HP_UNUSED, "KNOB 1", k); }

/* the parameter knob k reaches on the page shown: Tessera's pages by their knobs, CORDS and CLEAR by words, the others
 * by where the knob's value lives (a track's p, the globals); a page's own knobs by what its handler does with them */
static void help_find(uint32_t k)
{
    static const uint8_t EV[4] = {TX_EV_LOCK, TX_EV_EVERY, TX_EV_AMOUNT, TX_EV_SEED};
    static const uint8_t FM[4] = {TX_FM_FORM, TX_FM_CONTRAST, 0xFF, TX_FM_FLOURISH};
    static const uint8_t TW[4] = {TX_TW_SHIFT, TX_TW_DROP, TX_TW_HUMAN, TX_TW_FOLLOW};
    static const uint8_t TW2[4] = {TX_TW_BRANCH, TX_TW_BR_TO, TX_TW_LOGIC, TX_TW_COND};
    static const uint8_t NL[4] = {TX_NL_MODE, TX_NL_LEN, TX_NL_KEYS, 0xFF};   /* (GATE: PATTERN's, below) */
    static const uint8_t ENG[TESS_ENG_COUNT][3] = {{0xFF, 0xFF, 0xFF}, {TX_TUR_LEN, TX_TUR_PROB, TX_TUR_SPAN},
        {TX_EUC_STEPS, TX_EUC_PULSES, TX_EUC_ROT}, {TX_MKV_LEAP, TX_MKV_DENS, TX_MKV_SPAN}, {TX_MRP_POS, TX_MRP_SNPA, TX_MRP_SNPB},
        {TX_GRD_X, TX_GRD_Y, TX_GRD_DENS}, {TX_BNC_SPEED, TX_BNC_ANGLE, TX_BNC_BALLS},
        {TX_AWK_LENA, TX_AWK_LENB, TX_AWK_PROB}, {0xFF, TX_POL_DIV1, TX_POL_SPAN},
        {TX_QD_DIR, TX_QD_PROB, TX_QD_LINK}, {TX_CS_Y, TX_CS_DENS, TX_CS_AUTO},
        {TX_SR_START, TX_SR_INTV, TX_SR_LOOP}, {TX_CH_PROG, TX_CH_BARS, TX_CH_HIT}, {TX_AN_VARY, TX_AN_ECHO, TX_AN_FADE}};
    const page_t *pg = ui.home ? stage_page() : cur_page();
    uint32_t tr = song.sel, x = 0xFFu, lp, ls;
    int16_t *vp;
    help.pid = PID_NONE;
    help.hp = HP_NONE;
    help.name = 0;
    k &= 3u;
    help_tr = song.sel;
    if (!help_inner && list_help_row(&lp, &ls)) {       /* a list (SCL, VOICE ..): KNOB 1 the row, KNOB 2 its value */
        if (k == 1u) {                                  /* (the row's own help, as its page has it) */
            uint8_t was = ui.page;
            help_inner = 1;
            ui.page = (uint8_t)lp;
            help_find(ls);
            ui.page = was;
            help_inner = 0;
        } else if (k) {
            help_unused(k);
        } else {
            help_hp(HP_LIST_ROW);
        }
        return;
    }
    if (slot_kind()) {                                  /* PROJECT, USER, STORE: KNOB 1 / 2 the slot (ui_slots.c) */
        if (k < 2u) help_hp(HP_SLOT_PROJECT + (slot_kind() - 1u) % 3u);   /* (SK_PROJECT, SK_USER, SK_FM6) */
        else help_unused(k);
        return;
    }
    if (motion_page()) {                                /* (ui_motion.c motion_knob) */
        if (k < 2u) help_hp(HP_MOTION_PLAY + k);
        else help_unused(k);
        return;
    }
    if (!ui.home && pg->scope == SC_STEP) {             /* NOTES (a DRUM track's: the grid) */
        help_hp((grid_on() ? HP_GRID_STEP : HP_NOTES_STEP) + k);
        return;
    }
    if (pg->scope == SC_DRUMHIT) {
        help_hp(HP_HIT_STEP + k);
        return;
    }
    if (pg->scope == SC_DRUM) {                         /* (ids: 0 the sound, 1..3 its TUNE DECAY character, 4 LEVEL) */
        if (pg->id[k] > 4u) help_unused(k);
        else help_hp(HP_DRUM_SOUND + pg->id[k]);
        return;
    }
    if (pg->scope == SC_TRK) {                          /* MIXER: KNOB k track k's LEVEL, OCT+ held its PAN */
        help.pid = (uint16_t)PID_TRACK(((fm1_in.buttons >> panel.btn[B_OCTUP]) & 1u) ? P_PAN : P_LEVEL);
        help_tr = k;
        return;
    }
    if (pg->graph == GR_PATGRID) {                      /* PATTERNS: KNOB k track k's */
        help_named(HP_PATTERNS, "T1 ", k);
        return;
    }
    if (pg->graph == GR_SONG || pg->graph == GR_BROWSE) {
        help_hp((pg->graph == GR_SONG ? HP_SONG_SECTION : HP_BROWSE_LIST) + k);
        return;
    }
    if (pg->graph == GR_SCALE_PICKER && k == 0u) {      /* (SCALES: KNOB 2 to 4 SCALE ROOT QNT, below) */
        help_hp(HP_SCALE_FAMILY);
        return;
    }
    if (scale_settings_page(pg) && k == 1u) {           /* SCL's FAV */
        help_hp(HP_SCALE_FAV);
        return;
    }
    if ((pg->scope == SC_FM6 || pg->scope == SC_FMOP) && pg->graph != GR_FMSTORE) {   /* (FM6's patch, functions) */
        uint32_t id = pg->id[k];
        if (id == 0xFFu) help_unused(k);
        else if (pg->scope == SC_FMOP) help_named(HP_FMOP + id % (FP_OP + 1u), "OP1 ", fm6_opsel % 6u);
        else help_hp(id >= FP_SIZE ? HP_FMFN + (id - FP_SIZE) % FM6_NFN : HP_FMVOICE + (id - FP_PR1) % (HP_FMFN - HP_FMVOICE));
        return;
    }
    if (pg->graph == GR_TIMING && k < 2u && tr < TESSERA_TRACKS && tessera.mode[tr] == TESS_ENG_OFF) {
        help_hp(HP_TIMING_STEP);                        /* (a STEP track: SHAPE only, ui_input.c) */
        help.name = k ? "TIMING CHANCE" : "TIMING RATCH";
        return;
    }
    if (pg->scope == SC_GLOBAL && pg->id[k] == G_INFO) {   /* SYSTEM's CPU: a meter, nothing to set */
        help_hp(HP_CPU);
        return;
    }
    if (pg->graph == GR_CORDS || pg->graph == GR_MATRIX) {
        help_hp((pg->graph == GR_CORDS ? HP_CORD_SRC : HP_MX_FROM) + k);
        return;
    }
    if (pg->graph == GR_EVOLVE && tessera_step_on(tr)) {   /* STEP EVOLVE */
        static const uint8_t SV[3] = {TX_SV_LOCK, TX_SV_EVERY, TX_SV_AMOUNT};
        if (k == 3u) {
            help_hp(HP_SV_CLEAR);
            return;
        }
        x = SV[k];
    } else if (pg->graph == GR_EVOLVE) x = EV[k];
    else if (pg->graph == GR_TWEAK) x = TW[k];
    else if (pg->graph == GR_TWEAK2) x = TW2[k];
    else if (pg->graph == GR_NOTELEN) x = NL[k];
    else if (pg->graph == GR_TIMING) x = k == 3u ? TX_SUB_HUMAN : k == 2u ? TX_SUB_SHAPE : k ? TX_SUB_CHANCE : TX_SUB_RATCH;
    else if (pg->graph == GR_ENERGY && k < 2u) x = k ? TX_EN_RESTS : TX_EN_ARC;
    else if (pg->graph == GR_MIDICH) { x = TX_MIDI_CH; help_tr = k; }   /* (KNOB k: track k's) */
    else if (pg->graph == GR_KEYLINK && k == 0u) x = TX_KEY_LINK;
    else if (pg->graph == GR_ORGANIC && k < 2u) x = k ? TX_ORG_RATE : TX_ORG_SPEED;
    else if (pg->graph == GR_FORM) {
        x = FM[k];
        if (x == 0xFFu) {
            help_hp(HP_FORM_CLEAR);
            return;
        }
    } else if (pg->graph == GR_STEPS && pg->fam == FAM_GEN && tr < TESSERA_TRACKS && (k == 0u || tessera.mode[tr] != TESS_ENG_OFF))
    {
        if (!k && quad_cols_on()) {
            help_hp(HP_QUAD_COL);
            return;
        }
        x = k == 0u ? (uint32_t)TX_MODE : ENG[tessera.mode[tr] % TESS_ENG_COUNT][k - 1u];
        if (tessera.mode[tr] == TESS_ENG_AWAKE && k <= 1u && tessera_awake(tr)->edit) {   /* (a step picked) */
            help_hp(HP_AWAKE_A + k);
            return;
        }
        if (tessera.mode[tr] == TESS_ENG_POLY && k == 1u) {   /* (RHY: the page's choice, no setting of its own) */
            help_hp(HP_POLY_RHY);
            return;
        }
        if (k && quad_cols_on()) {                      /* (QUAD, OCT+ held: the columns' and RANGE) */
            uint32_t c = tessera.qcol_sel & 3u;
            help.pid = (uint16_t)PID_TESS(k == 1u ? TX_QD_MOVE1 + c : k == 2u ? TX_QD_EVERY1 + c : TX_QD_RANGE);
            return;
        }
        if (ch_layer_on()) {                            /* (CHORD, OCT+ held: OCT, the track's CHRD and VOIC) */
            if (k < 3u) help.pid = (uint16_t)(k == 0u ? PID_TESS(TX_CH_OCT) : k == 1u ? PID_TRACK(P_CHRD) : PID_TRACK(P_VOIC));
            else help.pid = (uint16_t)PID_TESS(tessera_chord(tr)->prog == GEN_CH_USER ? TX_CH_ULEN : TX_CH_BREATH);
            return;
        }
        if (gr_layer_on()) {                            /* (GRIDS, OCT+ held: KICK, SNR, HAT, TOMS) */
            help.pid = (uint16_t)PID_TESS(TX_GRD_KICK + (k & 3u));
            return;
        }
        if (sd_layer_on()) {                            /* (TURING, EUCLID, OCT+ held: SOURCE, KEEP) */
            if (k < 2u) help.pid = (uint16_t)PID_TESS(TX_SD_SOURCE + k);
            else help_unused(k);
            return;
        }
        if (an_layer_on()) {                            /* (ANSWER, OCT+ held: ORDER, SHIFT, SPACE, LEN) */
            help.pid = (uint16_t)PID_TESS(TX_AN_ORDER + (k & 3u));
            return;
        }
        if (cs_layer_on()) {                            /* (CONSTELLATION, OCT+ held: LOCK, LEN, FILL, SIZE) */
            static const uint8_t CSL[4] = {TX_EV_LOCK, TX_CS_LEN, TX_CS_FILL, TX_CS_SIZE};
            help.pid = (uint16_t)PID_TESS(CSL[k & 3u]);
            return;
        }
        if (tessera.mode[tr] == TESS_ENG_CONSTEL && k == 3u) {   /* (SPEED: the track's DIV, in another order) */
            help_hp(HP_CONSTEL_SPEED);
            return;
        }
        if (tessera.mode[tr] == TESS_ENG_POLY && k == 2u) x += tessera_poly(tr)->sel & 3u;
    }
    if (x != 0xFFu) {
        help.pid = (uint16_t)PID_TESS(x);
        return;
    }
    if (page_desc(pg, k, &vp) && vp) {
        if (vp >= TSEL->p && vp < TSEL->p + P_COUNT) help.pid = (uint16_t)PID_TRACK(vp - TSEL->p);
        else if (pg->scope == SC_GLOBAL && pg->id[k] < G_COUNT) help.pid = (uint16_t)PID_GLOBAL(pg->id[k]);
    }                                                   /* (the device's: A4, BUDGET, BOOT, DRUM in copies of their own) */
    if (help.pid == PID_NONE && pg->id[k] == 0xFFu) {   /* (an empty column) */
        help_unused(k);
        return;
    }
    if (help.pid == PID_NONE || !pid_find(help.pid)) {   /* (none of the above: ui_test's help check fails on it) */
        const param_desc_t *d = page_desc(pg, k, &vp);
        help_hp(HP_NONE);
        if (d && d->label && d->label[0]) help.name = d->label;
    }
}

/* The card: 228 x 214 under the header, over the page. Its text in one size, 15 px (Theo 2026-10-23: 10 and 12 px were
 * too small to read); a text longer than the card scrolls */
#define HELP_X 6
#define HELP_Y 22
#define HELP_W 228
#define HELP_H 214
#define HELP_TW 204                                     /* (the text's width) */
#define HELP_T0 34                                      /* (the text's first line, its last above HELP_T1) */
#define HELP_T1 188
#define HELP_FONT (&AF_M)
#define HELP_LH (AF_M.h + 2u)                           /* (a line's height) */
#define HELP_ROWS ((HELP_T1 - HELP_T0) / HELP_LH)       /* (the lines the card shows) */

/* text's words wrapped to lines of HELP_TW px in font f: each line's start and length (at most max of them); returns
 * how many lines it takes (all of them, also past max) */
static uint32_t help_wrap(const aafont_t *f, const char *text, uint16_t *at, uint8_t *len, uint32_t max)
{
    char line[64];
    uint32_t i = 0, start = 0, n = 0, lines = 0;
    while (text && text[start]) {
        uint32_t end = i;
        while (text[end] && text[end] != ' ') end++;
        if (end - start < sizeof line) {
            memcpy(line, text + start, end - start);
            line[end - start] = 0;
        }
        if ((end - start >= sizeof line || text_w(f, line) > HELP_TW) && n) {   /* (the line up to the last word) */
            if (lines < max) at[lines] = (uint16_t)start, len[lines] = (uint8_t)(n - start);
            lines++;
            start = i = n + 1u;
            n = 0;
            continue;
        }
        if (!text[end]) {
            if (lines < max) at[lines] = (uint16_t)start, len[lines] = (uint8_t)(end - start < 63u ? end - start : 63u);
            return lines + 1u;
        }
        n = end;
        i = end + 1u;
    }
    return lines;
}
static const char *help_text(void)
{
    const pid_entry_t *e = help.pid != PID_NONE ? pid_find(help.pid) : 0;
    return e ? pid_help(e) : help_page_text(help.hp);
}
static uint32_t help_count(void)
{
    uint16_t at[1];
    uint8_t len[1];
    return help_wrap(HELP_FONT, help_text(), at, len, 0);
}

/* ui_input.c: knob k turned s with ARP held; 1 = its help shown (the turn does nothing else). The first turn shows it;
 * turning on the same knob scrolls a text longer than the card, back up again. A scroll draws the card alone
 * (help.card), not the page under it again: drawing that first showed the page for a moment, a flicker */
static int help_knob(uint32_t k, int32_t s)
{
    if (!(fm1_in.buttons & (1u << panel.btn[B_ARP])) || ui.menu || ui.confirm || ui.layer)   /* (a popup: ui_input.c) */
        return 0;
    if (!help.on || help.knob != k) {
        help_find(k);
        help.knob = (uint8_t)k;
        help.top = 0;
        help.on = 1;
        ui.force = 1;
        return 1;
    }
    if (s > 0 && help.top + HELP_ROWS < help_count()) help.top++;
    else if (s < 0 && help.top) help.top--;
    else return 1;
    help.card = 1;
    return 1;
}

/* a strip of the card (a canvas holds 240 x 124 at most), each drawing the whole card at cv_oy = -its top: the name in
 * the theme's colour, the value right, the text, what the knob does next under a line */
static void help_strip(int32_t top, int32_t h, const char *name, const char *val, const char *text)
{
    const aafont_t *f = HELP_FONT;
    uint16_t at[64];
    uint8_t len[64];
    char line[64];
    uint32_t n = help_wrap(f, text, at, len, 64), rows = HELP_ROWS, i;
    int32_t y = HELP_T0;
    const char *hint;
    cv_begin(HELP_W, (uint32_t)h, T_BG);
    cv_oy = -top;                                       /* (as ui_motion.c: the card drawn whole, this strip of it) */
    cv_rrect(0, 0, HELP_W, HELP_H, 8, T_THEME, T_BG);
    cv_rrect(1, 1, HELP_W - 2, HELP_H - 2, 7, T_SURF, T_THEME);
    cv_free_text(12, 10, &AF_S, name ? name : "", T_THEME, T_SURF, 140);
    if (val[0]) cv_text_r(HELP_W - 12, 12, &AF_X, val, T_TEXT, T_SURF);
    for (i = help.top; i < n && i < 64u && i < help.top + rows; i++, y += (int32_t)HELP_LH) {
        memcpy(line, text + at[i], len[i]);
        line[len[i]] = 0;
        cv_text_on(12, y, f, line, T_TEXT, T_SURF);
    }
    if (n > rows) {                                     /* (a scroll bar: the lines shown of all of them) */
        int32_t th = HELP_T1 - HELP_T0 - 4, bh = th * (int32_t)rows / (int32_t)n;
        cv_rect(HELP_W - 7, HELP_T0, 2, th, T_LINE);
        cv_rect(HELP_W - 7, HELP_T0 + th * (int32_t)help.top / (int32_t)n, 2, bh > 6 ? bh : 6, T_THEME);
    }
    if (n > rows && help.top + rows < n) hint = "Clockwise: more.  Back: up.";
    else if (help.top) hint = "Back: up.";
    else hint = "ARP held: turn a knob for its help";
    cv_rect(12, HELP_H - 24, HELP_W - 24, 1, T_LINE);
    cv_free_text(12, HELP_H - 19, &AF_X, hint, T_DIM, T_SURF, HELP_W - 24);
    cv_oy = 0;
    cv_blit(HELP_X, (uint32_t)(HELP_Y + top));
}
/* the card's value: as the knobs' columns show it, a name whole ("TURING": the columns cut one to 5) */
static void help_value(const pid_entry_t *e, char *val, uint32_t size)
{
    const param_desc_t *d = e && !(e->flags & PID_F_ACTION) ? pid_desc(e->pid, help_tr) : 0;
    const char *unit = "";
    int32_t v;
    val[0] = 0;
    if (!d)
        return;
    v = pid_get(e->pid, help_tr);
    param_format(d, v, val, &unit);
    if (d->fmt == F_ENUM && d->names && d->names != N_ROUTE)
        str_cpy(val, d->names[clamp(v, d->min, d->max)], size);
    if (unit[0] && str_len(val) + str_len(unit) + 2u < size) {
        str_cpy(val + str_len(val), " ", 2);
        str_cpy(val + str_len(val), unit, 8);
    }
}
static void help_draw(void)
{
    const pid_entry_t *e = help.pid != PID_NONE ? pid_find(help.pid) : 0;
    char val[24];
    if (!ui.force && !help.card)
        return;
    help.card = 0;
    help_value(e, val, sizeof val);
    help_strip(0, 124, e ? e->name : help.name, val, help_text());
    help_strip(124, HELP_H - 124, e ? e->name : help.name, val, help_text());
}
