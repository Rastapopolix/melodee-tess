# Parameters

Every setting of Tessera with its number, generated from `firmware/src/pid.c` by `tests/pid_test.c` (`PID_UPDATE=1 sh tests/run_tests.sh` rewrites it; the suite fails when it is out of date). The numbers are fixed: mods and saved projects keep them.

The engine's eight (ENGINE 1 to 8) are named by the track's engine; their ranges here are the first track's default engine's.

## Track settings (one value per track)

| ID | Name | Range | Default | What it does |
| --- | --- | --- | --- | --- |
| 0x000 | LEVEL | 0 to 127 | 104 | The track's volume in the mix. |
| 0x001 | ENV ATTACK | 0 to 127 | 10 | How long a note takes to reach full level. |
| 0x002 | ENV DECAY | 0 to 127 | 70 | How long it then takes to fall to SUSTAIN. |
| 0x003 | ENV SUSTAIN | 0 to 127 | 90 | The level a held note stays at. |
| 0x004 | ENV RELEASE | 0 to 127 | 60 | How long a note takes to fade once let go. |
| 0x005 | ENV TO FILTER | -64 to 63 | 0 | How far the envelope opens (or, below 0, closes) the filter. |
| 0x006 | ENV TO PITCH | -64 to 63 | 0 | How far the envelope bends the pitch. |
| 0x007 | ENV TO SHAPE | -64 to 63 | 0 | How far the envelope moves the engine's SHAPE. |
| 0x008 | RECORD QUANTIZE | OFF, 1/4, 1/8, 1/16, 1/32, 8T, 16T, 1/2, 1/1, 2BAR, 4BAR | OFF | Moves a recorded take onto this grid when recording ends; OFF keeps the timing as played. |
| 0x009 | LFO RATE | 0 to 127 | 60 | How fast the track's LFO moves. |
| 0x00A | LFO WAVE | SIN, TRI, SAW, SQR, S&H | SIN | The LFO's shape: sine, triangle, saw, square or a random step each cycle (S&H). |
| 0x00B | LFO PHASE | 0 to 127 | 0 | Where in its cycle the LFO starts. |
| 0x00C | LFO FADE | 0 to 127 | 0 | How long the LFO takes to fade in after a note starts. |
| 0x00D | LFO TO PITCH | -64 to 63 | 0 | How far the LFO moves the pitch (vibrato). |
| 0x00E | LFO TO FILTER | -64 to 63 | 0 | How far the LFO moves the filter. |
| 0x00F | LFO TO SHAPE | -64 to 63 | 0 | How far the LFO moves the engine's SHAPE. |
| 0x010 | LFO TO LEVEL | 0 to 127 | 0 | How far the LFO moves the level (tremolo). |
| 0x011 | ARP MODE | OFF, UP, DN, UPDN, RND, ORD, REPEAT | OFF | The arpeggio: up, down, up and down, random, in ARP ORDER, or the last note played repeated; OFF: none. |
| 0x012 | ARP RATE | 1/4, 1/8, 1/16, 1/32, 8T, 16T, 1/2, 1/1, 2BAR, 4BAR | 1/16 | How often the arpeggiator plays a note. |
| 0x013 | ARP OCTAVES | 1 to 4 | 1 | How many octaves the arpeggio climbs through. |
| 0x014 | ARP GATE | 1 to 127 | 64 | How long each arpeggio note lasts, as a share of its step. |
| 0x015 | ARP SWING | 0 to 100 % | 0 | Delays every second arpeggio note, for a swung feel. |
| 0x016 | ARP PROBABILITY | 0 to 127 | 127 | The chance each arpeggio note plays. |
| 0x017 | ARP HOLD | OFF, ON | OFF | ON keeps the arpeggio going after the keys are let go. |
| 0x018 | ARP ORDER | NOTE, PLAY | NOTE | NOTE: the held notes from low to high; PLAY: in the order they were pressed. |
| 0x019 | ROOT | 0 to 11 | 0 | The scale's key note. |
| 0x01A | SCALE | CHR, MAJ, MIN, DOR, MIX, PEN, MPEN, HARM, PHRY, LYD, LOC, MEL, BLUES, WHOLE, DIMHW, DIMWH, 5EDO, 7EDO, 9EDO, 10EDO, 13EDO, 14EDO, 15EDO, 16EDO, 17EDO, 18EDO, 19EDO, 20EDO, 21EDO, 22EDO, 23EDO, 24EDO, 26EDO, 27EDO, 29EDO, 31EDO, 34EDO, 36EDO, 41EDO, 53EDO, JI-MAJ, JI-MIN, JI-12, JI-7L, JI-7, HARM8, HARM16, PARTCH, HARRIS, PYTH, MT-1/4, MT-1/3, MT-1/6, WERCK3, KIRN3, VALLOT, YOUNG2, RAST-J, SABA-J, SHRUTI, SLENDR, PELOG, BP-ET, BP-JI, RAST, BAYATI, HIJAZ, SABA, HUSAYN, SIKAH, IN, YO, HIRA, IWATO, KUMOI, RYUK | CHR | The scale the keys and the generators play in. |
| 0x01B | QUANTIZE | OFF, SNAP, WHITE, ALL, MPC, CHRD | OFF | How the keys meet the scale: OFF plays every key, SNAP moves a note into the scale, WHITE puts the scale on the white keys, ALL and MPC lay it out over the keys or MPC pads; CHRD snaps the keys, MIDI and the generators to the nearest note of the chord a CHORD track plays (no CHORD track: as SNAP). |
| 0x01C | TRANSPOSE | -24 to 24 semitones | 0 | Moves the track's notes up or down by semitones. |
| 0x01D | PATTERN LENGTH | 1 to 64 | 16 | How many steps the track's pattern has. |
| 0x01E | PATTERN DIVISION | 1/4, 1/8, 1/16, 1/32, 8T, 16T, 1/2, 1/1, 2BAR, 4BAR | 1/16 | How long one step is. |
| 0x01F | PATTERN SWING | 0 to 100 % | 0 | Delays every second step, for a swung feel. |
| 0x020 | PATTERN GATE | 1 to 127 | 64 | How long each step's note lasts, as a share of the step. |
| 0x025 | VOICE MODE | POLY, MONO, LEG, UNI | POLY | POLY plays chords; MONO one note at a time; LEG(ATO) one at a time, a note played while one is held not starting again; UNI stacks detuned voices on one note. |
| 0x026 | GLIDE | 0 to 127 | 0 | How long a note takes to slide to the next. |
| 0x027 | PAN | -64 to 63 | 0 | The track's place from left to right. |
| 0x028 | MUTE | OFF, ON | OFF | ON silences the track; it keeps playing in time. |
| 0x029 | GLIDE MODE | RATE, TIME | RATE | RATE: the slide's speed is fixed; TIME: every slide takes the same time, however far. |
| 0x02A | NOTE PRIORITY | LAST, LOW, HIGH | LAST | Which note sounds in MONO when several are held: the last, the lowest or the highest. |
| 0x02B | VOICE ALLOCATION | ROT, REUSE | ROT | ROT gives each new note the next free voice; REUSE the free voice last nearest its pitch (smoother glides). |
| 0x02C | UNISON DETUNE | 0 to 127 | 40 | How far apart UNI's stacked voices are tuned. |
| 0x031 | MOD 1 SOURCE | OFF, LFO, ENV, VEL, KEY, RAND, MODW, AT, EXPR, S&H, SLEW | OFF | MOD route 1: what moves the destination (LFO, envelope, velocity, key, random, the controllers). |
| 0x032 | MOD 1 DESTINATION | OFF, PITCH, CUT, SHP, AMP, PAN, RATE, VIB, E1, E2, E3, E4, E5, E6, E7, E8, DEPTH | OFF | MOD route 1: what is moved (pitch, filter, shape, level, pan, LFO rate or depth, the engine's own). |
| 0x033 | MOD 1 AMOUNT | -64 to 63 | 0 | MOD route 1: how far, and which way. |
| 0x034 | MOD 2 SOURCE | OFF, LFO, ENV, VEL, KEY, RAND, MODW, AT, EXPR, S&H, SLEW | OFF | MOD route 2: what moves the destination (LFO, envelope, velocity, key, random, the controllers). |
| 0x035 | MOD 2 DESTINATION | OFF, PITCH, CUT, SHP, AMP, PAN, RATE, VIB, E1, E2, E3, E4, E5, E6, E7, E8, DEPTH | OFF | MOD route 2: what is moved (pitch, filter, shape, level, pan, LFO rate or depth, the engine's own). |
| 0x036 | MOD 2 AMOUNT | -64 to 63 | 0 | MOD route 2: how far, and which way. |
| 0x037 | MOD 3 SOURCE | OFF, LFO, ENV, VEL, KEY, RAND, MODW, AT, EXPR, S&H, SLEW | OFF | MOD route 3: what moves the destination (LFO, envelope, velocity, key, random, the controllers). |
| 0x038 | MOD 3 DESTINATION | OFF, PITCH, CUT, SHP, AMP, PAN, RATE, VIB, E1, E2, E3, E4, E5, E6, E7, E8, DEPTH | OFF | MOD route 3: what is moved (pitch, filter, shape, level, pan, LFO rate or depth, the engine's own). |
| 0x039 | MOD 3 AMOUNT | -64 to 63 | 0 | MOD route 3: how far, and which way. |
| 0x03A | MOD 4 SOURCE | OFF, LFO, ENV, VEL, KEY, RAND, MODW, AT, EXPR, S&H, SLEW | OFF | MOD route 4: what moves the destination (LFO, envelope, velocity, key, random, the controllers). |
| 0x03B | MOD 4 DESTINATION | OFF, PITCH, CUT, SHP, AMP, PAN, RATE, VIB, E1, E2, E3, E4, E5, E6, E7, E8, DEPTH | OFF | MOD route 4: what is moved (pitch, filter, shape, level, pan, LFO rate or depth, the engine's own). |
| 0x03C | MOD 4 AMOUNT | -64 to 63 | 0 | MOD route 4: how far, and which way. |
| 0x051 | CHORD | OFF, DIA3, DIA7, MAJ, MIN, DOM7, MAJ7, MIN7, SUS4, POW | OFF | One key plays a chord: in the scale (DIA3, DIA7) or a fixed kind (MAJ, MIN, 7ths, SUS4, POWER); OFF plays one note. |
| 0x052 | CHORD VOICING | CLOSE, OPEN, INV1, INV2, +OCT | CLOSE | How the chord's notes are spread: close, open, an inversion, or with the root an octave down. |
| 0x053 | MPC DEGREE | 1 to 12 | 1 | With QUANTIZE MPC: the scale degree the pad H02 plays. |
| 0x054 | DRUM KICK | 0 to 127 | 127 | The kick's level in the kit. |
| 0x055 | DRUM SNARE | 0 to 127 | 127 | The snare's level in the kit. |
| 0x056 | DRUM CLAP | 0 to 127 | 127 | The clap's level in the kit. |
| 0x057 | DRUM CLOSED HAT | 0 to 127 | 127 | The closed hi-hat's level in the kit. |
| 0x058 | DRUM OPEN HAT | 0 to 127 | 127 | The open hi-hat's level in the kit. |
| 0x059 | DRUM TOM | 0 to 127 | 127 | The tom's level in the kit. |
| 0x05A | DRUM RIM | 0 to 127 | 127 | The rimshot's level in the kit. |
| 0x05B | DRUM BELL | 0 to 127 | 127 | The bell's level in the kit. |
| 0x05C | LFO SYNC | OFF, 4BAR, 2BAR, 1/1, 1/2, 1/4, 1/8, 8T, 1/16, 16T, 1/32 | OFF | Ties the LFO to the tempo: one cycle every bar, beat or step chosen; OFF runs at its own RATE. |
| 0x05D | LFO TRIGGER | NOTE, FREE | NOTE | NOTE starts the LFO again with each note; FREE lets it run on. |
| 0x05E | LFO POLARITY | BI, UNI | BI | BI moves both ways from the setting; UNI only one way. |
| 0x05F | STEREO SPREAD | 0 to 127 | 0 | Spreads the track's voices from left to right. |
| 0x060 | FX 1 TYPE | OFF, PHASER, WIDTH, FORMANT, TAPE, SPIT, ECHO, ROOM, REVERB, CHORUS | OFF | The track's insert effect 1. |
| 0x061 | FX 1 MIX | 0 to 127 | 0 | How much of insert effect 1 is heard. |
| 0x062 | FX 1 RATE | 0 to 127 | 60 | Insert effect 1's speed (its time or size, by the effect). |
| 0x063 | FX 1 CHARACTER | 0 to 127 | 64 | Insert effect 1's character (its depth, decay or the like, by the effect). |
| 0x064 | FX 2 TYPE | OFF, PHASER, WIDTH, FORMANT, TAPE, SPIT, ECHO, ROOM, REVERB, CHORUS | OFF | The track's insert effect 2. |
| 0x065 | FX 2 MIX | 0 to 127 | 0 | How much of insert effect 2 is heard. |
| 0x066 | FX 2 RATE | 0 to 127 | 60 | Insert effect 2's speed (its time or size, by the effect). |
| 0x067 | FX 2 CHARACTER | 0 to 127 | 64 | Insert effect 2's character (its depth, decay or the like, by the effect). |
| 0x068 | FX 3 TYPE | OFF, PHASER, WIDTH, FORMANT, TAPE, SPIT, ECHO, ROOM, REVERB, CHORUS | OFF | The track's insert effect 3. |
| 0x069 | FX 3 MIX | 0 to 127 | 0 | How much of insert effect 3 is heard. |
| 0x06A | FX 3 RATE | 0 to 127 | 60 | Insert effect 3's speed (its time or size, by the effect). |
| 0x06B | FX 3 CHARACTER | 0 to 127 | 64 | Insert effect 3's character (its depth, decay or the like, by the effect). |
| 0x06C | FX SEND | 0 to 127 | 0 | How much of the track goes to the effects (the first send effect when they are SPLIT). |
| 0x06D | FX SEND 2 | 0 to 127 | 0 | How much of the track goes to send effect 2 (FX MODE SPLIT). |
| 0x06E | FX SEND 3 | 0 to 127 | 0 | How much of the track goes to send effect 3 (FX MODE SPLIT). |
| 0x06F | ENGINE 1 | 0 to 0 | 0 | The engine's parameter 1: what it is depends on the track's engine (its name on the EDIT pages). |
| 0x070 | ENGINE 2 | 0 to 0 | 0 | The engine's parameter 2: what it is depends on the track's engine (its name on the EDIT pages). |
| 0x071 | ENGINE 3 | 0 to 0 | 0 | The engine's parameter 3: what it is depends on the track's engine (its name on the EDIT pages). |
| 0x072 | ENGINE 4 | 0 to 0 | 0 | The engine's parameter 4: what it is depends on the track's engine (its name on the EDIT pages). |
| 0x073 | ENGINE 5 | 0 to 0 | 0 | The engine's parameter 5: what it is depends on the track's engine (its name on the EDIT pages). |
| 0x074 | ENGINE 6 | 0 to 0 | 0 | The engine's parameter 6: what it is depends on the track's engine (its name on the EDIT pages). |
| 0x075 | ENGINE 7 | 0 to 0 | 0 | The engine's parameter 7: what it is depends on the track's engine (its name on the EDIT pages). |
| 0x076 | ENGINE 8 | 0 to 0 | 0 | The engine's parameter 8: what it is depends on the track's engine (its name on the EDIT pages). |

## Global settings

| ID | Name | Range | Default | What it does |
| --- | --- | --- | --- | --- |
| 0x100 | TEMPO | 40 to 240 | 120 | The project's speed in beats per minute. |
| 0x101 | SWING | 0 to 100 % | 0 | Delays every second step of every track, for a swung feel. |
| 0x102 | CLOCK SOURCE | INT, USB, TRS | INT | Where the tempo comes from: the FM-1 itself, USB MIDI or the TRS MIDI input. |
| 0x103 | TUNE | -50 to 50 | 0 | Fine tuning of every track, in cents. |
| 0x10C | MIDI INPUT | USB, TRS | USB | Which MIDI input plays the tracks: USB or TRS. |
| 0x10D | DRUM MIDI CHANNEL | OFF, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 | 10 | The MIDI channel the first drum track listens to and sends on (MIDI OUT). (The device's, not the project's.) |
| 0x10E | MIDI ROUTING | CH1-4, SEL, CH5-8, CH9-12, CH13-16 | CH1-4 | Which MIDI channels reach which tracks: four channels to the four tracks, or every channel to the selected track (SEL). MIDI OUT sends each track on the same channels (SEL: 1 to 4). |
| 0x110 | PROJECT SLOT | (a button) |  | The project slot LOAD and SAVE use. |
| 0x111 | BOOT PROJECT | OFF, A, B, C, D | OFF | The project loaded when the FM-1 starts; OFF starts a new one. (The device's, not the project's.) |
| 0x112 | PROJECT LOAD | (a button) |  | Loads the project in the slot. |
| 0x113 | PROJECT SAVE | (a button) |  | Saves the project to the slot. |
| 0x114 | ENGINE | (a button) |  | The selected track's sound engine. |
| 0x115 | A4 TUNING | 400 to 480 Hz | 440 | The pitch of A4 for the whole device, in hertz. (The device's, not the project's.) |
| 0x116 | CLEAR SEQUENCE | (a button) |  | Empties the selected track's pattern (it asks first). |
| 0x117 | INIT SOUND | (a button) |  | Resets the selected track's sound to its engine's starting point. |
| 0x11B | SEND FX 1 TYPE | OFF, PHASER, WIDTH, FORMANT, TAPE, SPIT, ECHO, ROOM, REVERB, CHORUS | OFF | Send effect 1, shared by every track. |
| 0x11C | SEND FX 1 MIX | 0 to 127 | 127 | How much of send effect 1 is heard. |
| 0x11D | SEND FX 1 RATE | 0 to 127 | 60 | Send effect 1's speed (its time or size, by the effect). |
| 0x11E | SEND FX 1 CHARACTER | 0 to 127 | 64 | Send effect 1's character (its depth, decay or the like, by the effect). |
| 0x11F | SEND FX 2 TYPE | OFF, PHASER, WIDTH, FORMANT, TAPE, SPIT, ECHO, ROOM, REVERB, CHORUS | OFF | Send effect 2, shared by every track. |
| 0x120 | SEND FX 2 MIX | 0 to 127 | 127 | How much of send effect 2 is heard. |
| 0x121 | SEND FX 2 RATE | 0 to 127 | 60 | Send effect 2's speed (its time or size, by the effect). |
| 0x122 | SEND FX 2 CHARACTER | 0 to 127 | 64 | Send effect 2's character (its depth, decay or the like, by the effect). |
| 0x123 | SEND FX 3 TYPE | OFF, PHASER, WIDTH, FORMANT, TAPE, SPIT, ECHO, ROOM, REVERB, CHORUS | OFF | Send effect 3, shared by every track. |
| 0x124 | SEND FX 3 MIX | 0 to 127 | 127 | How much of send effect 3 is heard. |
| 0x125 | SEND FX 3 RATE | 0 to 127 | 60 | Send effect 3's speed (its time or size, by the effect). |
| 0x126 | SEND FX 3 CHARACTER | 0 to 127 | 64 | Send effect 3's character (its depth, decay or the like, by the effect). |
| 0x127 | FX RETURN | 0 to 127 | 100 | The level of the send effects in the mix. |
| 0x128 | FX MODE | CHAIN, SPLIT | CHAIN | CHAIN: one send through all three effects in a row; SPLIT: a send to each effect of its own. |
| 0x129 | VOICE BUDGET | 8 to 32 pts | 16 | How much sound work the FM-1 shares out between the tracks; lower leaves more headroom, higher allows more voices. (The device's, not the project's.) |

## Tessera (one value per track)

| ID | Name | Range | Default | What it does |
| --- | --- | --- | --- | --- |
| 0x200 | GENERATOR MODE | STEP, TURING, EUCLID, MARKOV, MORPH, ATLAS, BOUNCE, AWAKE, POLY, QUAD, CONSTL, SERIES, CHORD, ANSWER | STEP | SEQ shows the pages of the MODE only. STEP plays the track's own steps; ARP the keys held, as an arpeggio (the ARP button opens it too); CHORD plays a progression's chords in the track's scale (QNT CHRD: the other tracks follow them); TURING, EUCLID, MARKOV and MORPH make its notes; ATLAS plays kick, snare, hat and toms (best on a DRUM track); BOUNCE plays where a ball hits the walls of its box; AWAKE adds two loops of its own lengths; POLY steps four notes on four rhythms; QUAD plays the track's row of a four by four grid the tracks share; CONSTL (CONSTELLATION) plays the stars of a passing sky as they cross a ship's crosshair; SERIES plays Per Norgard's infinity series; ANSWER answers what you play on its keys. |
| 0x201 | TURING LENGTH | 2 to 16 steps | 16 | How many steps the register loops over before it comes round again. |
| 0x202 | TURING PROB | 0 to 100 % | 0 | The chance a step changes as it comes round: 0 locks the loop, 100 is new every pass. |
| 0x203 | TURING SPAN | 1 to 3 oct | 2 | How many octaves the notes spread over. |
| 0x204 | EUCLID STEPS | 1 to 32 steps | 16 | The rhythm's length in steps. |
| 0x205 | EUCLID PULSES | 0 to 32 | 4 | How many hits, spread as evenly as they go over STEPS (at most STEPS). |
| 0x206 | EUCLID ROTATION | 0 to 31 | 0 | Turns the rhythm round by this many steps (fewer than STEPS). |
| 0x207 | MARKOV LEAP | 0 to 100 % | 25 | How far each note jumps from the last: 0 moves to its neighbours, 100 leaps widely. |
| 0x208 | MARKOV DENSITY | 5 to 100 % | 70 | The chance a step plays a note; the others rest. |
| 0x209 | MARKOV SPAN | 1 to 3 oct | 2 | How many octaves the notes spread over. |
| 0x20A | MORPH POSITION | 0 to 127 | 64 | Blends rhythm A (0) into rhythm B (full). |
| 0x20B | MORPH RHYTHM A | 0 to 7 | 0 | Rhythm A: one of eight preset patterns. |
| 0x20C | MORPH RHYTHM B | 0 to 7 | 2 | Rhythm B: one of eight preset patterns. |
| 0x20D | EVOLVE LOCK | 0 to 100 % | 0 | How much of the phrase stays when EVOLVE mutates it: 100 loops it, 0 is new every time. Turing's is its PROB turned round, Atlas' its CHAOS; Constellation's the chance a step plays its loop instead of the sky; Euclid, Bounce, Awake, Poly and Quad have none. |
| 0x20E | EVOLVE EVERY | OFF, 1, 2, 4, 8, 16 | OFF | Bars between mutations; OFF keeps the phrase as it is. |
| 0x20F | EVOLVE AMOUNT | 0 to 100 % | 30 | How much changes at each mutation. |
| 0x210 | EVOLVE ROLL | 1 to 128 | 1 | The track's seed, 1 to 128: each one a line of its own, the same every time. Playing, a new one starts on the next bar line. |
| 0x211 | FORM | OFF, AB, ABAC, ABCB, AABA, ABCA, AABC | OFF | The order the sections play in (A, B, C), each EVERY bars long; OFF plays one phrase. |
| 0x212 | FORM CONTRAST | 0 to 100 % | 60 | How different a new section is from the one before it. |
| 0x213 | FORM FLOURISH | -12 to 12 semitones | 0 | Semitones added on the last beat of every phrase; 0 is off. |
| 0x214 | TWEAK SHIFT | -24 to 24 semitones | 0 | Transposes the notes by semitones; while following, by scale steps from the followed note. |
| 0x215 | TWEAK DROP | 0 to 100 % | 0 | The chance a hit is left out, whatever the generator. |
| 0x216 | TWEAK HUMAN | 0 to 100 % | 0 | Varies each note's velocity and length a little, as a player would: at 100 up to 20 softer or louder and up to half as long; the same each time the piece plays. STEP tracks too. The timing: TIMING's HUMAN. |
| 0x217 | TWEAK FOLLOW | OFF, T1, T2, T3, T4 | OFF | Plays another track's pitch, SHIFT away, in this track's scale; OFF is its own. |
| 0x218 | ATLAS X | 0 to 255 | 128 | Where on the map of drum patterns, left to right; between two, a blend of both. |
| 0x219 | ATLAS Y | 0 to 255 | 128 | Where on the map of drum patterns, top to bottom; between two, a blend of both. |
| 0x21A | ATLAS DENSITY | 0 to 100 % | 50 | How full the kick, snare, hat and toms are: 0 silent, 100 every hit the map offers. Hold OCT+ on GEN for each part's own: KICK, SNR, HAT and TOMS. |
| 0x21B | BOUNCE SPEED | 1 to 64 | 24 | How far the ball travels each step: faster, more wall hits, more notes. In FALL, how fast the leaves fall. |
| 0x21C | BOUNCE ANGLE | 0 to 90 | 37 | The ball's path from the floor, in degrees: 0 and 90 go back and forth, an angle between draws a longer loop. EVOLVE nudges it. In FALL it is the WIND: the middle is calm, lower blows the leaves left (the melody leans low), higher right (high); further out, gusts hurry them down together. |
| 0x21D | BOUNCE BALLS | 1 to 5 | 1 | How many balls: each one more starts from a place and an angle of its own, so they cross; balls hitting on the same step play together, three notes at most. Past 4, FALL: eight leaves fall at speeds of their own and play where they land (left low, right high; one in seven an octave down), rest, and fall again. |
| 0x21E | AWAKE LENGTH A | 1 to 16 | 8 | Steps in loop A, the melody: its values are degrees of the scale or rests. ROLL draws them, EVOLVE changes a few; a white key picks a step: KNOB 1 its A, KNOB 2 its B. |
| 0x21F | AWAKE LENGTH B | 1 to 16 | 7 | Steps in loop B, which lifts the melody up the scale; a length other than A's shifts it against A. |
| 0x220 | AWAKE PROB | 0 to 100 % | 100 | The chance each of A's notes plays: 100 every one, lower leaves gaps. |
| 0x221 | POLY RHYTHM 1 | OFF, /1, /2, /3, /4, /5, /6, /7, /8, /9, /10, /11, /12, /13, /14, /15, /16 | /4 | Every how many steps rhythm 1 moves the four notes on (OFF: never). Rhythms that land together play once, louder: 4 against 3 is a polyrhythm. |
| 0x222 | POLY RHYTHM 2 | OFF, /1, /2, /3, /4, /5, /6, /7, /8, /9, /10, /11, /12, /13, /14, /15, /16 | /3 | Every how many steps rhythm 2 moves the four notes on (OFF: never). |
| 0x223 | POLY RHYTHM 3 | OFF, /1, /2, /3, /4, /5, /6, /7, /8, /9, /10, /11, /12, /13, /14, /15, /16 | OFF | Every how many steps rhythm 3 moves the four notes on (OFF: never). |
| 0x224 | POLY RHYTHM 4 | OFF, /1, /2, /3, /4, /5, /6, /7, /8, /9, /10, /11, /12, /13, /14, /15, /16 | OFF | Every how many steps rhythm 4 moves the four notes on (OFF: never). |
| 0x225 | POLY SPAN | 1 to 3 | 1 | Octaves the four notes spread over; ROLL draws them, EVOLVE moves them a little. |
| 0x226 | TWEAK BRANCH | 0 to 100 % | 0 | The chance a hit plays on track TO's sound instead of this one's, so two tracks answer each other; 0 is off. The tosses replay with the SEED. |
| 0x227 | TWEAK BRANCH TO | OFF, T1, T2, T3, T4 | OFF | The track a branched hit plays on; OFF keeps every hit here. Turned on: COIN tosses each hit, TOGGLE switches side on a toss won, LATCH tosses once a bar. |
| 0x228 | TWEAK BRANCH MODE | COIN, TOGGLE, LATCH | COIN | COIN tosses each hit; TOGGLE switches side on a toss won (100 alternates); LATCH tosses once a bar and keeps the bar's hits on one side. |
| 0x229 | TWEAK LOGIC | OFF, AND, OR, XOR, NOT | OFF | Combines the hits with track WITH's: AND plays only when both hit, OR when either does, XOR when one does, NOT only when the other rests; OFF is its own. |
| 0x22A | TWEAK LOGIC WITH | OFF, T1, T2, T3, T4 | OFF | The track LOGIC reads: a lower track's hit on this step, a higher one's on the step before. |
| 0x22B | TWEAK COND | OFF, 1:2, 2:2, 1:4, 2:4, 3:4, 4:4, 1:8, 8:8, FIRST, !FIRST | OFF | The bars a hit may play in, counted from PLAY (with FORM on, from the section's start): 1:4 is the first bar of every four; FIRST only the first, !FIRST the rest. |
| 0x22C | TIDE SPEED | 0 to 100 | 50 | How fast the track's TIDE drifts (mods: TIDE 1, 2, 3, three slow chaotic circuits): 50 is about 17, 3 and 1 minutes a turn; each 12.5 more twice as fast. |
| 0x22D | SWARM RATE | 0 to 100 | 50 | How fast the track's eight SWARM LFOs run: 50 is 64 s for SWARM 1 down to 2.4 s for SWARM 8; each 12.5 more twice as fast. |
| 0x22E | QUAD DIRECTION | FWD, BACK, PEND, RAND, HOLD | FWD | How the track's playhead moves along its row of the grid: forward, back, back and forth, at random; HOLD rests on its own steps and moves only when a LINK moves it. |
| 0x22F | QUAD PROB | 0 to 100 % | 100 | The chance each of the row's notes plays: 100 every one, lower leaves gaps. |
| 0x230 | QUAD LINK | OFF, T1, T2, T12, T3, T13, T23, T123, T4, T14, T24, T124, T34, T134, T234, T1234 | OFF | Other QUAD tracks this one's steps also move on, one, two or all three, each played for this step's gate: slow tracks pulled along by a fast one, or HOLD tracks played only by this one. |
| 0x231 | NOTE LENGTH | STEP, FIXED, HIT | STEP | How long the generator holds its notes. STEP: the step's GATE. FIXED: LEN, whatever DIV. HIT: until the next hit, so rests let notes ring on. Atlas keeps STEP. |
| 0x232 | NOTE LENGTH LEN | 1/32, 1/16, 1/8, 1/4, 1/2, 1/1 | 1/8 | FIXED's length, from a 32nd note to a bar (1/1), at the tempo the note starts. |
| 0x233 | NOTE LENGTH KEYS | 1 to 4 | 4 | FIXED: how many notes may hold at once. One more ends the note nearest its end; the same pitch again ends the one before. |
| 0x234 | CONSTELLATION Y | 0 to 14 | 7 | The height of the ship's crosshair in the sky: the stars crossing it play as they cross, so it sets where the melody sits. AUTO steers it instead. |
| 0x235 | CONSTELLATION DENS | 0 to 100 % | 50 | How many stars the sky holds: more stars, more notes. |
| 0x236 | CONSTELLATION SIZE | 0 to 7 | 2 | How many rows either side of the crosshair a star still plays in: 0 only its own row, higher a wider melody. OCT+ held on GEN, KNOB 4. |
| 0x237 | CONSTELLATION AUTO | OFF, 1, 2, 3, 4, 5, 6, 7, 8, 9, SEEK | OFF | Who steers the ship: OFF your Y; 1 to 9 a wander of its own, faster further up; SEEK flies towards the next star. Set by a mod (no knob of its own). |
| 0x238 | SERIES START | 0 to 127 | 0 | Where in the infinity series the melody begins: each START a different line, all of them alike at every scale. EVOLVE picks new ones. |
| 0x239 | SERIES INTV | 1 to 7 | 1 | How many degrees of the scale a step of the series is: 1 close and stepwise, higher wider leaps (folded back into two octaves). |
| 0x23A | SERIES LOOP | OFF, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 | OFF | OFF lets the series run on, never repeating; 1 to 16 plays that many steps of it round and round. |
| 0x23E | QUAD RANGE | 3 OCT, 1 OCT, 2 OCT, +-1 | 3 OCT | The octaves the row's notes fold into: 3 OCT as they are, 1 OCT or 2 OCT from the root up, +-1 an octave either side of it. OCT+ held on GEN, KNOB 4. |
| 0x23F | QUAD MOVE 1 | STOP, DOWN, UP, RAND | STOP | How a column of QUAD's grid moves its cells between the four rows: STOP, DOWN (each note a track down, track 4's to track 1), UP, or RAND (a jump of 1 to 3 rows). Rows not in QUAD turn too, keeping the notes. OCT+ held on GEN: KNOB 1 the column, KNOB 2 its MOVE. |
| 0x240 | QUAD MOVE 2 | STOP, DOWN, UP, RAND | STOP | How a column of QUAD's grid moves its cells between the four rows: STOP, DOWN (each note a track down, track 4's to track 1), UP, or RAND (a jump of 1 to 3 rows). Rows not in QUAD turn too, keeping the notes. OCT+ held on GEN: KNOB 1 the column, KNOB 2 its MOVE. |
| 0x241 | QUAD MOVE 3 | STOP, DOWN, UP, RAND | STOP | How a column of QUAD's grid moves its cells between the four rows: STOP, DOWN (each note a track down, track 4's to track 1), UP, or RAND (a jump of 1 to 3 rows). Rows not in QUAD turn too, keeping the notes. OCT+ held on GEN: KNOB 1 the column, KNOB 2 its MOVE. |
| 0x242 | QUAD MOVE 4 | STOP, DOWN, UP, RAND | STOP | How a column of QUAD's grid moves its cells between the four rows: STOP, DOWN (each note a track down, track 4's to track 1), UP, or RAND (a jump of 1 to 3 rows). Rows not in QUAD turn too, keeping the notes. OCT+ held on GEN: KNOB 1 the column, KNOB 2 its MOVE. |
| 0x243 | QUAD EVERY 1 | /1, /2, /3, /4, /6, /8, /16 | /4 | How often a column moves: every 1 to 16 steps of the first QUAD track. OCT+ held on GEN: KNOB 1 the column, KNOB 3 its EVERY. |
| 0x244 | QUAD EVERY 2 | /1, /2, /3, /4, /6, /8, /16 | /4 | How often a column moves: every 1 to 16 steps of the first QUAD track. OCT+ held on GEN: KNOB 1 the column, KNOB 3 its EVERY. |
| 0x245 | QUAD EVERY 3 | /1, /2, /3, /4, /6, /8, /16 | /4 | How often a column moves: every 1 to 16 steps of the first QUAD track. OCT+ held on GEN: KNOB 1 the column, KNOB 3 its EVERY. |
| 0x246 | QUAD EVERY 4 | /1, /2, /3, /4, /6, /8, /16 | /4 | How often a column moves: every 1 to 16 steps of the first QUAD track. OCT+ held on GEN: KNOB 1 the column, KNOB 3 its EVERY. |
| 0x247 | CONSTELLATION LEN | 1 to 16 | 16 | How many steps the loop of stars is, 1 to 16: with LOCK up a short phrase repeats. OCT+ held on GEN, KNOB 2. |
| 0x248 | CONSTELLATION FILL | OFF, ARP | OFF | What the steps between stars play: OFF a rest, ARP the last four stars again, low to high and softer. OCT+ held on GEN, KNOB 3. |
| 0x249 | CHORD PROG | POP, 50S, 2-5-1, CIRCLE, TWO, DRONE, WALK, USER | POP | The progression, on the degrees of the track's scale: POP I V vi IV, 50S I vi IV V, 2-5-1 ii V I I, CIRCLE the scale's fifths down, TWO I IV, DRONE I alone; WALK four chords of the track's SEED (ROLL another, EVOLVE changes one); USER up to eight of your own: hold OCT+ on GEN and play their roots on the keys, one key a chord (rounded down to the scale; letting OCT+ go ends it). SCL CHORD's CHRD and VOIC shape the chords (CHRD OFF: triads); hold OCT+ on GEN for OCT, CHRD and VOIC. |
| 0x24A | CHORD BARS | 1/2, 1, 2, 4 | 1 | How long each chord lasts: half a bar, 1, 2 or 4 bars. |
| 0x24B | CHORD HIT | HOLD, BEAT, STEP | HOLD | How the chord plays: HOLD once, held to the next chord; BEAT struck again every beat; STEP every step, for the step's GATE. |
| 0x24C | CHORD OCT | -2 to 2 | 0 | The chords' register, -2 to +2 octaves from their home around C3. OCT+ held on GEN, KNOB 1; KNOB 2 and 3 there are SCL CHORD's CHRD and VOIC. |
| 0x24D | ATLAS KICK | 0 to 100 % | 100 | How much of DENSITY the kick gets: 100 all of it, 50 half, 0 none (the kick silent). OCT+ held on GEN, KNOB 1. |
| 0x24E | ATLAS SNARE | 0 to 100 % | 100 | How much of DENSITY the snare gets: 100 all of it, 0 none. OCT+ held on GEN, KNOB 2. |
| 0x24F | ATLAS HAT | 0 to 100 % | 100 | How much of DENSITY the closed hat gets: 100 all of it, 0 none. OCT+ held on GEN, KNOB 3. |
| 0x250 | ATLAS TOMS | 0 to 100 % | 0 | A fourth part, on the tom or the sound PART4 picks: the hits the kick and snare only just miss, so fills follow the groove. 0 none; up, misses further under the line play too (DENSITY scales it). OCT+ held on GEN, KNOB 4; with it held, a black key picks the sound. |
| 0x251 | CHORD USER LEN | 1 to 8 | 4 | How many of USER's chords play, 1 to 8: playing them in on the keys (OCT+ held on GEN) sets it; shorten it to leave the last ones out. OCT+ held on GEN, KNOB 4. |
| 0x252 | STEP EVOLVE LOCK | 0 to 100 % | 50 | A STEP track's EVOLVE page: how much of the variation stays from round to round. 100 holds it, 0 makes a fresh one each time. |
| 0x253 | STEP EVOLVE EVERY | OFF, 1, 2, 4, 8, 16 | 1 | A STEP track's EVOLVE page: bars between rounds of changes; OFF plays the steps as written. |
| 0x254 | STEP EVOLVE AMOUNT | 0 to 100 % | 0 | A STEP track's EVOLVE page: how many steps play a change, 100 half of them: a note moved by a degree or two or an octave, a step dropped, two notes swapped, a ghost note in a rest. 0 plays the steps as written; the steps themselves never change until REC held keeps the changes. |
| 0x255 | ATLAS PART4 | TOM, CLAP, HATOP, RIM, BELL | TOM | The fourth part's sound: TOM, CLAP, HATOP (the open hat), RIM or BELL, the DRUM lanes besides the kick, snare and closed hat. Hold OCT+ on GEN and press its black key (as on the drum grid: CP OH TM RS CB). |
| 0x256 | TIMING RATCH | x1, x2, x3, x4, x5, x6, x7, x8 | x1 | Plays each step's notes 1 to 8 times in equal parts of the step, a ratchet; x1 plays them once, as written. Each hit gets its share of the step's GATE. Notes NOTE LENGTH holds (FIXED, HIT) and BRANCHed ones play once. |
| 0x257 | TIMING CHANCE | 0 to 100 % | 100 | How often a step ratchets: 100 every step, 0 none. The toss replays with the track's SEED. |
| 0x258 | CHORD BREATH | 0 to 100 % | 0 | With HIT HOLD, the chord breathes: each voice is struck again, softer, on a slow cycle of its own (11 to 19 s), and the same chord again is tied over instead of struck (DRONE: one long chord). BREATH is the chance a voice above the lowest sits a cycle out, one at a time; 0 is off. OCT+ held on GEN, KNOB 4 (USER: its LEN there). |
| 0x259 | ENERGY ARC | 0 to 100 % | 0 | The project's slow tide, the same for every track: over 3 to 6 minutes it swings from the tracks as set to sparse and back. At the sparse end the generator tracks leave out up to 70 % more of their hits and play up to 16 softer; ARC is how far it swings, 0 off. It is the cord source ENERGY too (to a filter, a send). |
| 0x25A | ENERGY RESTS | 0 to 100 % | 0 | How often the music rests: 8 to 18 s with the generator tracks silent (a CHORD track's held chord and its BREATH go on), at 100 about every 4 minutes, near 0 about every 20, sooner while ARC's tide is sparse. 0 off. |
| 0x25B | ANSWER VARY | 0 to 100 % | 30 | How much each answer changes the phrase you played: a note moved a degree or two, an octave, left out or swapped with the next. Each answer changes it a little more. 0 answers it as played. |
| 0x25C | ANSWER ECHO | 0 to 100 % | 20 | The chance a note it plays comes back once more, at half its velocity, 3 to 8 steps later, one in four an octave up. 0 off. |
| 0x25D | ANSWER FADE | 0 to 100 % | 33 | How long it remembers: 1 answer at 0 to 16 at 99, each softer than the last, then it rests until you play again. At 100 it never forgets: the notes play on as four loops of different lengths (LEN), so it never quite repeats. |
| 0x25E | ANSWER ORDER | PLAYED, REVERSE, SHUFFLE | PLAYED | The order of the notes in an answer, over the rhythm you played: PLAYED, REVERSE or SHUFFLE (a new order each answer). OCT+ held on GEN, KNOB 1. |
| 0x25F | ANSWER SHIFT | 0 to 7 | 2 | Each answer is moved up or down by up to this many degrees of the scale, a new amount each time. 0 keeps it where you played it. OCT+ held on GEN, KNOB 2. |
| 0x260 | ANSWER SPACE | 0 to 100 % | 25 | The rest before each answer: 0 answers straight away, 100 waits 4 bars. A bar without a key ends the phrase you play. OCT+ held on GEN, KNOB 3. |
| 0x261 | ANSWER LEN | SHORT, MID, LONG | MID | With FADE at 100, the lengths of the four loops the notes play on, in steps: SHORT 5 7 11 13, MID 13 17 23 29, LONG 23 29 31 37. OCT+ held on GEN, KNOB 4. |
| 0x262 | TIMING SHAPE | EVEN, ACCEL, DECEL | EVEN | Where a ratchet's hits fall in their step: EVEN in equal parts, ACCEL closer and closer (a roll that speeds up), DECEL further and further apart (a bounce). The track's, for its RATCH and for every step's Ratchet on a STEP track. |
| 0x263 | MIDI OUT CHANNEL | AUTO, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, OFF | AUTO | The MIDI channel this track's notes go out on over USB, so each track can play its own instrument. AUTO follows SYSTEM's ROUT (CH1-4: tracks on 1 to 4), a drum track the DRUM channel; OFF sends none of its notes. Saved with the project. Which notes go out at all is the menu's MIDI OUT: KEYS, ALL or OFF. GLO > MIDI OUT, a knob for each track. |
| 0x264 | KEY LINK | OWN, LINKED | LINKED | LINKED: one key for the whole project. Changing ROOT on any melodic track sets it on the others too, as the scale always is; drum tracks keep theirs. OWN: each track its own ROOT. Turning it on gives every track the selected track's key. SCL's Key link row; on in a new project, saved with it. |
| 0x265 | SOURCE | OFF, T1, T2, T3, T4 | OFF | A STEP pattern for TURING or EUCLID to play from: T1 to T4 is that track's pattern playing now (this track's own too), which stays as written. TURING loops its first LEN steps and PROB changes them a step at a time; EUCLID plays its notes in order on its hits. OFF, or a pattern without notes: the generator's own. OCT+ held on GEN, KNOB 1. |
| 0x266 | KEEP | FREE, RHYTHM, PITCH | FREE | With a SOURCE, what stays as written. FREE: TURING may change a step's pitch or its hit. RHYTHM: only the pitches change (EUCLID: the source's own rhythm, on EUCLID's pitches). PITCH: only the hits come and go. OCT+ held on GEN, KNOB 2. |
| 0x267 | TIMING HUMAN | 0 to 60 ms | 0 | Plays each step a little late, as a player behind the beat: up to this many ms, a new amount each step, never early and never more than a quarter of the step. The same each time the piece plays. 0 on the grid. STEP tracks too (with a step's NUDGE). TWEAK's HUMAN varies velocity and length. |

## Mods

A mod (the MODS page, under LFO) moves a destination on any track by a source on any track: by its AMOUNT (-100 to 100 %, at a full source the destination's whole range; PITCH: an octave) plus its OFFSET. A bipolar source (LFO at POL BI, KEY, RAND, S&H, SLEW, NOTE, DRIFT, TIDE, SWARM) swings both ways from the knob; OCT+ on MODS makes a mod UNI, the source folded to 0 to 1, so it moves one way only, as ENV does. ENV, VEL, KEY and RAND on a voice destination (PITCH, CUTOFF, SHAPE, GAIN) of their own track follow each note, as a chord's; elsewhere the latest note. Room for 32, 16 in use. They are the project's, saved with it. Melodee's MOD page is gone: the four routes a project or a sound was saved with become mods on their track, the same move (a sound's go when another sound is loaded on the track, unless one was edited).

| Source | |
| --- | --- |
| LFO | the track's LFO |
| ENV | the envelope of the track's latest note |
| VEL | the latest note's velocity |
| KEY | the latest note's pitch (C4 = 0) |
| RAND | a new random value at each note |
| MODW | MIDI mod wheel (CC1) |
| AT | MIDI channel aftertouch |
| EXPR | MIDI expression (CC11) |
| S&H | a random value held for each cycle of the track's LFO |
| SLEW | S&H, gliding from one value to the next |
| GATE | 1 while the track's generator plays a note |
| NOTE | the generator's last note (C4 = 0) |
| CV | the generator's CV: Turing its register as a number, Euclid and Morph a ramp over their cycle, Markov the degree it moved to |
| DRIFT | a slow wander of the track's own, a new point every 12 s or so, eased between |
| TIDE 1 | TIDE's slowest chaotic circuit, about 17 min a turn (ORGANIC page, SPEED 50) |
| TIDE 3 | TIDE's fastest chaotic circuit, about 1 min a turn |
| TIDE 2 | TIDE's middle chaotic circuit, about 3 min a turn |
| SWARM 1 | a free LFO, 64 s a turn (ORGANIC page, RATE 50) |
| SWARM 2 | a free LFO, 40 s a turn |
| SWARM 3 | a free LFO, 25 s a turn |
| SWARM 4 | a free LFO, 15.6 s a turn |
| SWARM 5 | a free LFO, 9.8 s a turn |
| SWARM 6 | a free LFO, 6.1 s a turn |
| SWARM 7 | a free LFO, 3.8 s a turn |
| SWARM 8 | a free LFO, 2.4 s a turn |
| ENERGY | the project's slow tide (ENERGY ARC), every track's the same: -100 % sparse .. 100 % full |

| Destination | Moved at | What it moves |
| --- | --- | --- |
| PITCH | each voice | the pitch of each voice: 100 % at a full source is an octave |
| CUTOFF | each voice | the filter (or the engine's brightness) of each voice |
| SHAPE | each voice | the engine's SHAPE of each voice, as ENV DEST SHP moves it |
| GAIN | each voice | each voice's loudness: up opens it from silence, down closes it |
| LEVEL | the audio block | the track's level in the mix |
| PAN | the audio block | the track's place left to right |
| SPREAD | the audio block | the stereo spread of the track's voices |
| ATTACK | the audio block | the envelope's attack time |
| DECAY | the audio block | the envelope's decay time |
| SUSTAIN | the audio block | the envelope's sustain level |
| RELEASE | the audio block | the envelope's release time |
| ENV FLT | the audio block | how far the envelope moves the filter |
| ENV PIT | the audio block | how far the envelope moves the pitch |
| ENV SHP | the audio block | how far the envelope moves SHAPE |
| LFO RTE | the audio block | the track LFO's speed |
| LFO DEP | the LFO's depth | the track LFO's depth, wherever it goes |
| LFO PIT | the audio block | how far the LFO moves the pitch (vibrato) |
| LFO FLT | the audio block | how far the LFO moves the filter |
| LFO SHP | the audio block | how far the LFO moves SHAPE |
| LFO AMP | the audio block | how far the LFO moves the level (tremolo) |
| FX SEND | the audio block | how much of the track goes to the effects |
| E1 | the audio block | the engine's first parameter (its name: the track's engine) |
| E2 | the audio block | the engine's second parameter |
| E3 | the audio block | the engine's third parameter |
| E4 | the audio block | the engine's fourth parameter |
| E5 | the audio block | the engine's fifth parameter |
| E6 | the audio block | the engine's sixth parameter |
| E7 | the audio block | the engine's seventh parameter |
| E8 | the audio block | the engine's eighth parameter |
| T PROB | the generator's step | Turing's PROB: how often its loop changes |
| T LEN | the generator's step | Turing's loop length |
| E PULSE | the generator's step | Euclid's hits: more or fewer |
| E ROT | the generator's step | Euclid's rotation |
| M LEAP | the generator's step | Markov's LEAP: how far its notes jump |
| M DENS | the generator's step | Markov's density: how many steps play |
| MP POS | the generator's step | Morph's blend from rhythm A to B |
| SHIFT | the generator's step | TWEAK's SHIFT: transposes the generator's notes |
| DROP | the generator's step | TWEAK's DROP: the chance a hit is left out |
| HUMAN | the generator's step | TWEAK's HUMAN: velocity and gate variation |
| EV AMT | the generator's step | EVOLVE's AMOUNT: how much each mutation changes |
| CONTR | the generator's step | FORM's CONTRAST: how different a new section is |
| AT X | the generator's step | Atlas' X: across its map of drum patterns |
| AT Y | the generator's step | Atlas' Y: down its map of drum patterns |
| AT DENS | the generator's step | Atlas' density: how full its parts are |
| B SPEED | the generator's step | Bounce's speed: how far the balls move a step |
| B ANGLE | the generator's step | Bounce's angle: the balls' paths (FALL: the wind) |
| A PROB | the generator's step | Awake's PROB: the chance a note plays |
| BRANCH | the generator's step | TWEAK 2's BRANCH: the chance a hit plays on track TO |
| Q PROB | the generator's step | Quad's PROB: the chance a cell plays |
| C Y | the generator's step | Constellation's Y: the crosshair's height (AUTO OFF) |
| C SIZE | the generator's step | Constellation's SIZE: how far from the crosshair a star is caught |
| C DENS | the generator's step | Constellation's DENS: how many stars the sky holds |
| S START | the generator's step | Series' START: where in the infinity series it plays |
| C AUTO | the generator's step | Constellation's AUTO: who steers the ship (OFF, a wander, SEEK) |
| CH PROG | the generator's step | Chord's PROG: which progression its chords follow |
| AT KICK | the generator's step | Atlas' KICK: how much of its density the kick gets |
| AT SNR | the generator's step | Atlas' SNR: how much of its density the snare gets |
| AT HAT | the generator's step | Atlas' HAT: how much of its density the hat gets |
| AT TOMS | the generator's step | Atlas' TOMS: how many of the near misses play the tom |
| AN VARY | the generator's step | Answer's VARY: how much each answer changes the phrase |
| H TIME | the generator's step | TIMING's HUMAN: how late a step may play |
