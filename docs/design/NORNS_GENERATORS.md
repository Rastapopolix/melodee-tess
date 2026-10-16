# norns.community scripts as Tessera generators

I went through every script on norns.community tagged sequencer or generative (353 entries in the catalogue) and read the source of initenere, rebound, boingg, less concepts, nørgård, nmMelodyMagic and meadowphysics. Samplers, loopers, live-coding tools, trackers, effects and grid-only performance tools are left out.

"Ease" means how much new code and state a mode needs on the FM-1 (integer maths, small RAM), measured against what Tessera already has. Grids (PR #4) is the yardstick for a "normal" new mode.

## Easiest: small, self-contained, one track each

| # | Script | What it does | Why it's easy | State |
|---|---|---|---|---|
| 1 | **nørgård** | Per Nørgård's infinity series: a melody that is self-similar at every scale | Each note comes straight from the bits of the step number. Nothing needs storing except a start offset and an interval size | ~4 B |
| 2 | **fibonacci** | Walks the Fibonacci sequence modulo N, mapped to a scale | One addition and one modulo per step | ~4 B |
| 3 | **rebound** | Balls travel in straight lines and bounce off the walls, and each wall hit is a note (no gravity) | This is exactly the Bounce idea already on the roadmap. Integer position and velocity, with the wall picking the pitch | ~12 B |
| 4 | **awake** (already on roadmap) | A rhythm loop plus a loop of note steps that wander | Two short arrays and a random walk | ~40 B |
| 5 | **orbital** | Rings turning at different speeds, triggering when they pass a point | Phase accumulators, the same idea as Euclid but with free ratios | ~10 B |
| 6 | **less concepts** | A 1D cellular automaton (8 cells, rule 0..255) where the cells pick notes | Eight bits and one table lookup per generation. Rule, seed and range map neatly onto three knobs | ~6 B |
| 7 | **nmMelodyMagic** (Infinite Melody / Modulo Magic) | Ports of Ken Stone's Serge modules: a clocked shift register and a counter folded by modulo | Close cousins of Turing. Modulo Magic is the new and interesting half | ~6 B |
| 8 | **foulplay** | Euclid with logic (AND/OR/XOR against another track) and probability | It's mostly an upgrade to the existing Euclid mode. The logic part reads another track's hits, which FOLLOW already does for pitch | ~2 B |

## Moderate: more state, or a richer screen

| # | Script | What it does | What makes it harder |
|---|---|---|---|
| 9 | **zellen** | Conway's Game of Life on a small grid, where born cells play | 16×8 cells fit in 16 bytes, but choosing which cells sound and drawing it well takes thought |
| 10 | **meadowphysics** | Cascading counters: each row counts down, fires a note and can reset or advance other rows | Simple maths, but at its best it spans several tracks (see the four-track section below) |
| 11 | **acid-test / bline** | Generated acid basslines with slides and accents | acid-test is a trained Markov table, which overlaps the Markov mode. bline's parametric approach would be the new part. Needs slide/accent handling per voice |
| 12 | **skylines / metrix / m18s** | Metropolis-style: 8 stages, each with a pitch, a pulse count and a gate mode | Closer to a step sequencer than a generator, and needs a per-stage editing UI |
| 13 | **corners / groovecats / KoiBoi2** | Physics toys (objects moving across a field and triggering on contact) | Fun with Bounce as the base, but they lean on grid input for placing things |
| 14 | **bloom** | Eno and Chilvers' Bloom: notes you place echo back and decay | Needs a way to "place" notes. Could be driven by the keys of a STEP track |
| 15 | **flora** | L-system string rewriting turned into melody | The strings grow exponentially, so it needs a hard cap (say 128 symbols) and care to keep the cost of each step flat |

## Skip or already covered

- **cyrene**: this is Grids, already done in PR #4.
- **grurder**: Euclid plus Turing, which Tessera already has.
- **luck, jala, raindrops, delinquencer**: probability or random-note sequencers that Turing with DROP and HUMAN already covers.
- **kria / n.kria / faeng**: full sequencers in their own right, not generators. They overlap Tessera's STEP tracks.
- **thorns, drift, dunes, spirals**: I didn't verify the source closely enough to rank them. I can look if any appeal.

## A four-track mode like initenere

**Yes, it's feasible.** It's medium-sized work, about a Grids-sized change plus the shared screen. It fits better than it first looks.

### How initenere works (from its source)

- **The matrix.** A 4×4 matrix of notes.
- **Rows are voices.** Each row is a voice with its own playhead direction (pause, forward, back, random) and clock division. Every voice has a play probability.
- **Columns drift.** Each column rotates its four notes up, down or shuffled at its own division, so the melody each voice plays slowly drifts.
- **Time routings.** A voice's tick can also advance other voices.
- **Pitch handling.** Octave folding and scale snapping happen at the end.

### How it maps onto Tessera

Today `tessera_clock_step` runs once per track with that track's own state. The proposal keeps that and adds a shared part:

1. **A MATRIX generator mode, chosen per track like the others.** A track in MATRIX mode plays its own row of one shared 4×4 matrix. Because each track already steps at its own length and speed, initenere's polyrhythm comes for free. EVOLVE, TWEAK, DROP, HUMAN, FOLLOW and cords all keep working per track, and a track can leave the matrix by switching mode.
2. **A small shared block.** It holds the 16 notes, 4 column directions and rates, and a 4×4 routing mask, about 30 bytes in total, plus about 3 bytes per track. The column rotation would be clocked from the song's bar/step clock, since in initenere it belongs to no voice.
3. **One screen for all four tracks.** The MATRIX page draws the whole matrix with all four playheads whichever track is selected. The knobs edit the selected track's row (direction, rate, probability), and a second view edits columns and routings. This is the one real break from "the UI edits `song.sel`".
4. **Time routings, the fiddliest part.** When track A steps, it also advances B's playhead and plays B's note on B's instrument. That means handling B's gate-off from A's step. Tracks step in index order within a tick, so the result stays deterministic and the host tests can pin it.
5. **Save format.** The shared block means a format step (TSR4), done the same way as the Grids one: convert on reading, with no extra RAM.

### Worth building once as a general hook

The shared four-track hook is worth building once, because other scripts on this list want it too:

- quence (a probabilistic 4-track sequencer);
- meadowphysics (counters resetting each other across tracks);
- fugu / streams (several playheads over one pattern);
- foulplay's logic between tracks.

initenere would be the first mode to use it.

## Matching the norns visuals

The FM-1 screen (240×240, colour) is bigger than the norns screen (128×64, 16 grey levels) on both axes. Any norns layout fits once it's scaled up about 1.9×, with room left for a knob row underneath, and grey levels can map onto the Tessera theme colours. That makes it a design choice rather than a hardware limit, and it fits how Tessera already works: each mode has its own picture in `graph_tessera_viz` (ui_graph.c), as Grids now does with its three lanes.

What each mode would show:
- **initenere**: its 4×4 matrix of squares, a bright square for each playhead, and the direction and rate glyphs around the edges.
- **rebound / Bounce**: the box with the balls moving and a flash on the wall that was hit.
- **less concepts**: the rows of cells scrolling down, one generation per row.
- **zellen**: the Game of Life grid.
- **awake**: its two loops drawn as rows of steps with the playhead.
- **orbital**: the rings turning.
- **nørgård**: the series as a pitch contour.

Two practical limits:
- **Movement.** Smooth motion (bouncing balls, orbits) depends on how often the UI redraws that area. The current views only change on each step, so Bounce and orbital would need the area redrawn every frame, which costs CPU outside the audio interrupt. This needs measuring on the device.
- **Licensing.** If a mode uses a script's own code or layout closely, its licence applies: check each script's licence and add it to LICENSING.md, as was done for Grids. Drawing the same idea in our own way avoids that.
