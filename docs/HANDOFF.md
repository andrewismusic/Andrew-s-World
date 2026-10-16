# Andrew's World: iOS port handoff

Oct 16, 2026 · @Andrew Huang

## Purpose

This doc hands the Andrew's World prototype (v13, a single 3,300-line HTML file) to Claude Code so it can be rebuilt as a native iOS app. The HTML stays the reference implementation: when this doc and the prototype disagree, the prototype's behaviour wins unless a section below says otherwise.

**How to use it with Claude Code**

1. Put the prototype file in the new repo as `reference/fractal-groovebox.html`. Never ship it; it is a spec you can run.
2. Paste the starter prompt (last section) into Claude Code from the repo root.
3. Work milestone by milestone (Port plan). Each milestone ends with a side-by-side check against the prototype open in Safari.
4. Keep this doc as the source of design rules. Update it when a decision changes.

Get the latest prototype from the artifact (Download as HTML) before starting; the version in the repo should match what you last approved.

## What the prototype is

Andrew's World is a four-track groovebox you explore by zooming. Every level of a song is a place on one continuous map, and any level can hold its own sound settings (locks).

| Level | What you see | What the pane edits |
| --- | --- | --- |
| Space (-1) | The planet you're orbiting, with its locations | Survey card, land |
| Song (0) | The location's song library as blocks | Master: volume, glue, saturate, rename, export |
| Part (1) | A song's parts (A, B, intro…) | Mix: level and pan per track |
| Track (2) | Four lanes of bars; or the effects page | Track sound, effects buildings |
| Pattern (3) | One bar as a piano roll / drum grid | Sound, keys, loop, triplet |
| Note (4) | One note up close | Per-note locks, ratchet, ramp, probability |

**Tracks.** Four fixed slots per location, always in this order: drums, bass, chords, lead (Farm, Laboratory) or prism (Blocks, Solucan). Drums have 8 voices: kick, snare, clap, rim, closed hat, open hat, tom, perc. Melodic tracks span 21 scale steps.

**Locations and planets.** Locations are separate libraries with their own look, engines and demo songs. Planets group locations; in space the page buttons fly between planets.

| Planet | Location | Look | Engines |
| --- | --- | --- | --- |
| Terraflor | Farm | 8-bit farm: crops, critters, buildings, characters | drums, bass, chords, lead |
| Terraflor | Blocks | Neon blocks on a night palette | drums, bass, chords, prism |
| Terraflor | Laboratory | Glass cockpit, hairline instruments | drums, bass, chords, lead |
| Rostellar | Solucan | Black rocky desert, indigo accents, worms | drums, bass, chords, prism |

**Other features to carry over.**

- Dice sliders: Sound, Notes, Locks. Drag the die; the distance sets how much is randomised.
- Undo/redo for every edit. Only the global reset asks twice, and it lives in Preferences.
- Lasso, add, duplicate, delete on the action row; Solucan adds Worms on the track and pattern levels.
- Effects as Farm buildings: Mill (distortion), Well (echo), Windmill (chorus), Barn (filter), three slots per track.
- Recording from keyboard or on-screen keys, with a one-bar count-in and a robot voice saying “get ready”.
- Farm characters with Q&A and unlockable secrets (lore), and critters that animate around notes.
- Rocket launch from the hatch, orbit view, landing flights through clouds or ring rocks.
- Preferences: count-in, metronome sound, rate and volume, interface sounds, critters.
- Export a song to audio (uses the downloads capability in the artifact; a share sheet on iOS).

## Architecture and stack

Go native: Swift and SwiftUI for the chrome, Metal (or SpriteKit) for the zoom map and planets, and a real-time render callback for all sound. A web view wrapping the HTML would ship fastest, but Web Audio latency and the per-pixel planet won't hold up on device.

&#91;embedded content: app architecture · 5 parts around one store\]

The audio thread never reads the store directly: the store sends it an immutable snapshot of the song and locks through a lock-free queue, and the audio thread reports the playhead back.

| Choice | Recommendation | Why |
| --- | --- | --- |
| Minimum OS | iOS 17 | Observation framework, modern SwiftUI |
| Devices | iPhone first, iPad in the same target | The prototype already has a two-column wide layout |
| Audio | `AVAudioEngine` + `AVAudioSourceNode`, DSP in C++ | Sample-accurate timing, no per-note node churn |
| Map | Metal with an instanced quad renderer | Thousands of blocks and notes at 120 Hz |
| State | One `@Observable` store, value-type model | Undo is a snapshot stack, like the prototype |
| Tests | Unit tests for layout, lock resolution, migrations, worms odds | These are pure functions in the prototype |

## Data model and save format

Keep the prototype's JSON shape exactly, so a save exported from the web version imports into the app unchanged. Model it as Codable Swift structs and write a round-trip test against a save taken from the prototype.

```
State  { v: 10, wsel, sel: [song, part, slot, note|null, bar], master: { locks }, worlds: [Location] }
Location { skin: "farm"|"classic"|"cockpit"|"solucan", name, songs: [Song] }
Song   { name, style, hue, prog: [4 chord degrees], locks, mix: { m:[4], s:[4] }, fx: [4][3] building|null, sections: [Part] }
Part   { name, hue, type, locks, lanes: [Lane x4] }
Lane   { locks, bars: [Bar] }            // lanes own their bars; max 8 bars
Bar    { notes: [Note], trip?: bool }    // trip = triplet grid
Note   { id, step, pitch, len, locks, ramp?, rm?: "away"|"toward" }
```

**Rules that matter**

- Pitches are stored as scale steps, never MIDI notes. Changing key or scale re-voices everything.
- Drum pitch = voice row 0–7 (0 = kick).
- Locks are a flat map of `"<slot>.<engine>.<param>"` (or `rat.rt`, `g.bpm`, `master.*`) to a 0–1 value. The effective value walks the chain master → song → part → lane → note; the deepest lock wins.
- Meter: numerator 2–16 in `g.ts`, denominator 16th/8th/quarter in `g.td`; numerator is capped at twice the denominator.
- `state.worlds` is still the key name for locations (renamed in the UI only). Keep it to stay save-compatible.
- Older saves (v5–v9) migrate forward in the prototype. Port only the v10 reader unless old saves matter; a 3-location save gets Solucan appended.
- Preferences and lore (characters unlocked, locations visited) are stored separately from songs.

**Storage on iOS.** One JSON file in Application Support, written atomically after each change (debounced \~300 ms), plus an import/export of the same JSON through the Files app.

## Audio engine

The prototype synthesises everything live with Web Audio; there are no samples. On iOS, rebuild it as a real-time render loop in C++ (or Swift with unsafe buffers) inside an `AVAudioSourceNode`, not as a graph of `AVAudioUnit` nodes: the prototype creates nodes per note, which won't scale natively.

**Engines to port** (parameter lists live in the prototype's `PG` and `P` tables)

- **Drums:** 8 voices built from tones and noise. Kick is a sine with a fast pitch drop; tone, decay, punch, level per voice; a kit page for drive, crush, delay, reverb, level, pan. Each voice chokes its own last hit. Kick tuning is never randomised.
- **Bass, chords, lead:** oscillator → filter (low/band/high, cutoff, resonance) → amp envelope, plus glide, octave, drive, crush, delay, reverb, pan, probability. Chords voice a scale-degree progression per bar. Lead adds additive partials in Farm.
- **Prism:** wavefolding oscillator (shape, fold, drift, tilt) with envelope, vibrato, poly/mono.
- **Effects buildings per track:** distortion, echo (tempo-synced divisions), chorus, filter, up to 3 in series.
- **Master:** volume, glue (bus compressor), saturate; one shared reverb (impulse capped at 4.5 s).
- **Ratchets and ramps:** a note can repeat at 1/16 to 1/64; a ramp fades the repeats away or toward.

**Sequencer.** 12 ticks per beat; straight bars step every 3 ticks, triplet bars every 2. Swing applies to off-steps. Schedule with a sample-accurate clock on the audio thread; the prototype's 25 ms look-ahead timer is a browser workaround. Cap active voices at 20 like the prototype.

**Other sounds.** Metronome (woodblock, clicks, beeps), the “get ready” voice (a tiny formant synth rendered once to a buffer), interface blips (off when the song plays), and the rocket, landing, travel, worm and space-ambience sounds. Interface sounds follow the preference; rocket and space sounds always play.

**Parity test.** Render one bar of each demo song offline in both versions and compare loudness and spectrum. Exact sample match isn't the goal; matching character is.

## Zoom engine and rendering

The zoom is the product, so it gets its own renderer. Use SwiftUI for the chrome (header, rail, action row, pane, sheets) and a single Metal or SpriteKit view for the map, so zooming runs at 120 Hz on ProMotion.

**How the prototype zooms**

- `z` is a continuous depth from -1 (space) to 4 (note). The current level L is `floor(z)`; layer A draws level L and layer B draws level L+1 inside the selected rectangle of A.
- For fraction t = z − L, the selected rect r grows from its own size to full screen with `size = r^(1−t)`, which feels like a constant-speed dive. Port `applyZoom()` maths as-is.
- Corner radius and outlines interpolate so blocks morph into the next level's frame; the song outline thickens so the part view's corners never cover it.
- Releasing a drag springs to the nearest level (`z += d·0.17` per frame). Timed flights (launch, landing, planet switch) run on the clock with a cubic ease in and out.
- Overlay items (labels, note names, badges, critters, trees, selection boxes) are positioned per frame from the same transform, and stay a fixed pixel size rather than stretching. Trees keep their proportions and spread apart as you zoom.

**Layout per level** comes from pure functions (`songRects`, `sectionRects`, `laneRects`, `cellRects`, `noteRects`) returning unit-square rects. Port them first and unit test them; everything else hangs off them.

**Pattern view.** Always exactly one bar wide; only its height scrolls through the 21 rows. Tap to place, drag to lengthen, tap a note to delete, lasso to select.

**Space.** The planet is ray-cast per pixel at low resolution into a buffer (a perfect fit for a fragment shader). Clouds on Terraflor and ring rocks on Rostellar have an altitude and slide past the camera during landing using `par = (1 − 1/kp) / (1 − k/kp)`. The HUD (degree ring, orbit with a satellite that turns with the planet, sweep, brackets, readouts) is vector line work.

**Pixel art.** Farm sprites are text grids with a palette (`SPR`, `PAL`, `pix()`). Export them to PNG atlases with nearest-neighbour sampling, or keep the grids and generate textures at launch.

## Locations, skins and space

Each location is a skin: a token set (paper, card, well, edge, ink, sel, lock, radii, border width, shadow, fonts) plus fill functions for songs, parts, lanes and bars, and an engine set. Build a `Skin` protocol with one implementation per location; Solucan currently borrows Laboratory's rules and recolours them, so it can start as a subclass.

| Location | Fonts | Accent | Signature details |
| --- | --- | --- | --- |
| Farm | Fredoka, Nunito; Press Start 2P for note labels | blue select, pink lock | pumpkins, vine-logs, sharks and whales, flowers on vines; trees; critters; buildings for effects; characters (Keeper, Taco, Magwood, Charles, Addison, Patti) |
| Blocks | Unbounded, IBM Plex Sans Condensed | gold select, gold lock | flat neon blocks, gold-outlined rail thumb |
| Laboratory | B612 Mono, B612 | cyan select, magenta lock | hairline grid, annunciator-style buttons |
| Solucan | IBM Plex Mono, IBM Plex Sans Condensed | indigo select, violet lock | dark rock texture, Worms button |
| Space (station) | Unbounded, IBM Plex Mono; Press Start 2P title | violet | riveted control panel, LCD planet name |

Bundle the fonts (all are open licence on Google Fonts) and register them in Info.plist.

**Worms (Solucan, track and pattern levels).** Pick 30% of notes in view. Of those, 50–90% are deleted, 0–30% get a darkening tone lock (cutoff, drum tone or prism tilt scaled to 30–65% of its current value), 0–15% get a random ratchet; the rest are left. Kicks never get ratchets. Worms climb over each chosen note's top edge and eat along it in \~0.76 s with random stagger; the edit lands as one undo step.

**Space.** Launch: open the hatch, tap the rocket (shake, lift-off, 1.7 s climb). In orbit, drag the spin strip to rotate; tap a location to open its survey card (Music, Engines, Climate, Resources, Lifeforms, Library, Position); Back and Land. Page buttons fly to the next planet in 1.15 s with the camera panning across stars. Landing is a 3.4 s eased descent. Entering space fades the panel background in over 0.9 s rather than switching.

## Interaction rules and design preferences

These came out of 13 rounds of review and should be treated as requirements.

- **No jitter.** Layouts are fixed: nothing shifts when a label, value or state changes. Dice sliders hold position while rolling.
- **Undo, not confirms.** Deleting anything is instant and undoable. The only double-ask is “reset every location” in Preferences.
- **Lock model.** Lock off = edits go to the song's sound. Lock on = edits stay on this level (track, pattern, note). Lock is unavailable at Song and Part. Clear locks removes this level's locks.
- **Notes are scale steps.** Never store MIDI pitch.
- **Sentence case in Farm**; the dark locations use uppercase small labels.
- **Highlights change the background**, never just the text colour, so on-state buttons stay readable in dark skins.
- **Sound etiquette.** No sound for adding or deleting notes. Interface sounds are quiet and silent while a song plays.
- **Page buttons** page whatever the level holds: songs, parts, bars or effects, bars, notes; planets in space.
- **Zoom rail.** Evenly spaced levels, with the hatch one step below Song measured edge to edge.
- **Touch.** Sliders never scroll the page or move under the finger; pinch and drag zoom the map; long-press is not used.

**iOS additions to decide:** haptics (the prototype vibrates on launch and landing; map to `UIImpactFeedbackGenerator`), background audio (should playback continue with the screen locked?), and MIDI/Bluetooth keyboards for recording.

## Port plan

Build sound and data first, then the zoom, then each location's skin; space and characters come last. Each milestone ends with a side-by-side check against the prototype.

1. **Skeleton.** Xcode project (SwiftUI, iOS 17+), Codable model, load/save, import the demo state exported from the prototype. *Done when* a prototype save round-trips byte-equivalent.
2. **Audio core.** Render loop, drums and bass, sequencer with swing and triplets, transport. *Done when* the first Farm demo song plays in time with the prototype.
3. **All engines and effects.** Chords, lead with partials, prism, the four buildings, master glue and saturate, ratchets, ramps, probability, lock resolution. *Done when* every demo song in every location sounds right.
4. **Zoom map.** Layout functions, A/B layer zoom, rail, spring and timed flights, selection, page buttons, in one plain skin. *Done when* zooming Song → Note is smooth at 120 Hz on device.
5. **Editing.** Pattern grid, note view, pane faders and pages, Lock, Keys, Loop, Triplet, lasso, add/dup/delete, dice sliders, undo/redo, recording with count-in.
6. **Skins.** Blocks and Laboratory first (flat), then Farm sprites, trees, critters and buildings, then Solucan and Worms.
7. **Space.** Shader planets, clouds, rings, HUD, launch, planet switching, landing, survey cards, ambience.
8. **Characters and polish.** Chat with characters and lore, preferences, export through the share sheet, haptics, accessibility (VoiceOver labels from the prototype's aria labels), App Store assets.

## Known issues and open questions

**Carried over from the prototype**

- On 360 px-wide phones the action row runs about 20 px past the right edge (all locations). Fix in the native layout.
- While worms eat a note, its text label stays until the edit lands.
- The Prism engine once froze in an IDM stress case that couldn't be reproduced; the 20-voice cap is a safeguard, not a fix. Profile it in the native engine.
- Space planets render per pixel on the CPU; on iOS this must move to a shader.

**Risks**

- Audio parity: Web Audio's biquads, compressor and convolver have specific curves. Port the filter maths (RBJ cookbook) and match by ear, not by sample.
- The prototype is one file with global state. Untangle by subsystem; don't translate line by line.
- Fonts and sprite art are all original or open licence, but confirm before App Store submission.

**Open questions**

- [ ] Minimum iOS version and iPad support (the layout already has a wide two-column mode).
- [ ] Keep save compatibility with the web prototype, or start a fresh native format?
- [ ] Export formats: WAV only, or also stems and MIDI?
- [ ] Should songs sync across devices (iCloud)?
- [ ] Does playback continue in the background?

## Starter prompt for Claude Code

Paste this from the repo root, with the prototype at `reference/fractal-groovebox.html` and this doc saved as `docs/HANDOFF.md`.

```
We're porting a web prototype to a native iOS app. Read docs/HANDOFF.md fully, then read
reference/fractal-groovebox.html, which is the reference implementation: when in doubt, match
its behaviour. Don't translate it line by line; split it by subsystem.

Stack: Swift, SwiftUI for chrome, Metal (or SpriteKit) for the zoom map and planets, a
real-time C++ or Swift render callback in an AVAudioSourceNode for all synthesis. No
third-party dependencies without asking me.

Start with milestone 1 in the Port plan: create the Xcode project, the Codable data model
matching the prototype's save format exactly, and a round-trip test using a save exported
from the prototype (I'll add it at reference/demo-save.json). Then propose a file layout and
the audio engine design for milestone 2 before writing it.

Follow the 'Interaction rules and design preferences' section as requirements. At the end
of each milestone, list what to compare side by side with the prototype and stop for my review.
```

To get `demo-save.json`, open the prototype, run `copy(JSON.stringify(state))` in Safari's Web Inspector console, and paste it into the file.
