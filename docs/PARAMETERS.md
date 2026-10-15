# Parameters

Every setting of melodee-tess with its number, generated from `firmware/src/pid.c` by `tests/pid_test.c` (`PID_UPDATE=1 sh tests/run_tests.sh` rewrites it; the suite fails when it is out of date). The numbers are fixed: cords and saved projects keep them.

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
| 0x01A | SCALE | CHR, MAJ, MIN, DOR, MIX, PEN, MPEN, HARM, PHRY, LYD, LOC, MEL, BLUES, WHOLE, DIMHW, DIMWH, 5EDO, 7EDO, 9EDO, 10EDO, 13EDO, 14EDO, 15EDO, 16EDO, 17EDO, 18EDO, 19EDO, 20EDO, 21EDO, 22EDO, 23EDO, 24EDO, 26EDO, 27EDO, 29EDO, 31EDO, 34EDO, 36EDO, 41EDO, 53EDO, JI-MAJ, JI-MIN, JI-12, JI-7L, JI-7, HARM8, HARM16, PARTCH, HARRIS, PYTH, MT-1/4, MT-1/3, MT-1/6, WERCK3, KIRN3, VALLOT, YOUNG2, RAST-J, SABA-J, SHRUTI, SLENDR, PELOG, BP-ET, BP-JI, RAST, BAYATI, HIJAZ, SABA, HUSAYN, SIKAH | CHR | The scale the keys and the generators play in. |
| 0x01B | QUANTIZE | OFF, SNAP, WHITE, ALL, MPC | OFF | How the keys meet the scale: OFF plays every key, SNAP moves a note into the scale, WHITE puts the scale on the white keys, ALL and MPC lay it out over the keys or MPC pads. |
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
| 0x10D | DRUM MIDI CHANNEL | OFF, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 | 10 | The MIDI channel the first drum track listens to. (The device's, not the project's.) |
| 0x10E | MIDI ROUTING | CH1-4, SEL, CH5-8, CH9-12, CH13-16 | CH1-4 | Which MIDI channels reach which tracks: four channels to the four tracks, or every channel to the selected track (SEL). |
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

## Tessera (one value per track)

| ID | Name | Range | Default | What it does |
| --- | --- | --- | --- | --- |
| 0x200 | GENERATOR MODE | STEP, TURING, EUCLID, MARKOV, MORPH | STEP | STEP plays the track's own steps; TURING, EUCLID, MARKOV and MORPH make its notes. |
| 0x201 | TURING LENGTH | 2 to 16 steps | 16 | How many steps the register loops over before it comes round again. |
| 0x202 | TURING PROB | 0 to 100 % | 0 | The chance a step changes as it comes round: 0 locks the loop, 100 is new every pass. |
| 0x203 | TURING SPAN | 1 to 3 oct | 2 | How many octaves the notes spread over. |
| 0x204 | EUCLID STEPS | 1 to 32 steps | 16 | The rhythm's length in steps. |
| 0x205 | EUCLID PULSES | 0 to 32 | 4 | How many hits, spread as evenly as they go over STEPS (at most STEPS). |
| 0x206 | EUCLID ROTATION | 0 to 31 | 0 | Turns the rhythm round by this many steps (fewer than STEPS). |
| 0x207 | MARKOV LEAP | 0 to 100 % | 25 | How far each note jumps from the last: 0 moves to its neighbours, 100 leaps widely. |
| 0x208 | MARKOV DENSITY | 5 to 100 % | 70 | The chance a step plays a note; the others rest. |
| 0x209 | MARKOV SPAN | 1 to 3 oct | 2 | How many octaves the notes spread over. |
| 0x20A | MORPH POSITION | 0 to 127 | 0 | Blends rhythm A (0) into rhythm B (full). |
| 0x20B | MORPH RHYTHM A | 0 to 7 | 0 | Rhythm A: one of eight preset patterns. |
| 0x20C | MORPH RHYTHM B | 0 to 7 | 2 | Rhythm B: one of eight preset patterns. |
| 0x20D | EVOLVE LOCK | 0 to 100 % | 0 | How much of the phrase stays when EVOLVE mutates it: 100 loops it, 0 is new every time. Turing's is its PROB turned round; Euclid has none. |
| 0x20E | EVOLVE EVERY | OFF, 1, 2, 4, 8, 16 | OFF | Bars between mutations; OFF keeps the phrase as it is. |
| 0x20F | EVOLVE AMOUNT | 0 to 100 % | 30 | How much changes at each mutation. |
| 0x210 | EVOLVE ROLL | 1 to 128 | 1 | The track's seed, 1 to 128: each one a line of its own, the same every time. Playing, a new one starts on the next bar line. |
| 0x211 | FORM | OFF, AB, ABAC, ABCB, AABA, ABCA, AABC | OFF | The order the sections play in (A, B, C), each EVERY bars long; OFF plays one phrase. |
| 0x212 | FORM CONTRAST | 0 to 100 % | 60 | How different a new section is from the one before it. |
| 0x213 | FORM FLOURISH | -12 to 12 semitones | 0 | Semitones added on the last beat of every phrase; 0 is off. |
| 0x214 | TWEAK SHIFT | -24 to 24 semitones | 0 | Transposes the notes by semitones; while following, by scale steps from the followed note. |
| 0x215 | TWEAK DROP | 0 to 100 % | 0 | The chance a hit is left out, whatever the generator. |
| 0x216 | TWEAK HUMAN | 0 to 100 % | 0 | Varies velocity and gate a little, as a player would. |
| 0x217 | TWEAK FOLLOW | OFF, T1, T2, T3, T4 | OFF | Plays another track's pitch, SHIFT away, in this track's scale; OFF is its own. |

## Cords

A cord moves a destination on any track by a source on any track: by its AMOUNT (-100 to 100 %, at a full source the destination's whole range; PITCH: an octave) plus its OFFSET. Room for 32, 16 in use.

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
