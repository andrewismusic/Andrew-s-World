# Milestone 2 proposal: file layout and audio engine

Status: **built; in review.** Decisions are recorded at the end, and what changed during the build is in
"As built" just below.

## As built

- **Song data reaches the engine as JSON, and the snapshot builder is C++.** The app encodes the current location
  and the master node exactly as the save writes them and posts them to `aw_engine_set_location`; the C++ builder
  resolves every lock and caches songs by their text. One implementation serves the app, the native tests and the
  offline renderer, and it builds and runs off a Mac. (The plan had the builder in Swift.)
- **The module contract** (`Sources/AWDSP/Modules/Module.hpp`): a module receives note starts with resolved
  parameters at exact times and adds into its track bus and the shared sends. Farm's drums and the bass/chords/lead
  synth are the first modules; tools from other apps arrive the same way.
- **Chords, lead and the track buildings came along early.** They share code with bass, and the demo songs use
  the buildings, so leaving them out made the songs sound wrong. Glue and Prism are still milestone 3.
- **Parity, measured:** `tools/parity/render-prototype.js` renders a song from the prototype in Chromium and
  `aw-render` renders it natively; `tools/parity/compare.py` compares them bar by bar. Old School Hip-Hop is within
  0.1 dB per bar and 1–3% in brightness; each track solo is within 0.5 dB. The other Farm songs are within about
  0.5 dB overall and up to ~10% brighter natively, mostly in the chords and reverb, worth an ear check.
- **One deliberate match of a browser quirk:** Chromium's 2× oversampled shaper delays its output by 128 samples,
  and the Mill mixes that with the dry signal, which makes a comb. That is part of how the Mill sounds, so the
  native Mill delays its wet path the same amount.
- **Pan law:** equal power with unity in the centre (the prototype skips the panner at centre, so any pan at all
  drops 3 dB there; natively a hard pan is 3 dB up on one side instead).
- **The 25 ms timer, look-ahead and Safari workarounds are gone:** notes start on their exact sample.


Milestone 2 (from the handoff): render loop, drums and bass, sequencer with swing and triplets, transport.
*Done when* the first Farm demo song (Old School Hip-Hop) plays in time with the prototype and sounds recognisably
like it. The prototype's sound was never dialled in, so it is a starting point, not a target to match exactly:
the engine is built to be easy to tune.

## File layout

Code is split by subsystem; each piece of `fractal-groovebox.html` has one home. New targets go in the same local package, so
everything except the UI builds and tests on macOS with `swift test`.

```
Packages/AWCore/
  Package.swift                    targets: AWCore, AWDSP, AWAudio (+ test targets)
  Sources/
    AWCore/                        Swift, pure, no Apple-only frameworks
      JSON/  Model/  Persistence/  (milestone 1, done)
      Music/
        Params.swift               the P / PG / SHORT registry as data: key, label, default, steps, unit, random range
        ParamFormat.swift          fmt0(): value → "1.2kHz", "−6.0dB" … (needed by the pane later; cheap to port now)
        Locks.swift                LockChain (master → song → part → lane → note), rv(), clear/count helpers
        Score.swift                musAt(), meterOf(), barSteps/barTicks/stepTicks, scales, degMidi(), mtof()
      Playback/
        SnapshotBuilder.swift      AppState → render-ready snapshot (see below), cached per song
        PlaybackContext.swift      what the sequencer needs from the UI: level, selection, loop, cue
    AWDSP/                         C++20, real-time code only; Swift sees only include/AWDSP.h
      include/AWDSP.h              plain C API: create/destroy, render, post command, swap snapshot, read playhead
      Engine.cpp/.hpp              the render entry: drain commands, swap snapshot, run sequencer, mix voices, master
      Sequencer.cpp/.hpp           12-tick clock, advance(), swing, triplets, probability, ratchets, cue, loop
      Voices/
        DrumVoice.cpp/.hpp         the 8 drum voices from drumHit()
        SynthVoice.cpp/.hpp        bass / chords / lead from synthHit() (bass in M2, the rest in M3)
        VoicePool.hpp              fixed pool, 20-synth-voice cap with oldest-steal, per-row drum choke, mono slots
      DSP/
        Automation.hpp             Web Audio AudioParam semantics: setValueAtTime, linear/exponential ramps, setTargetAtTime
        Biquad.hpp                 Web Audio (WebKit) biquad coefficients, incl. Q-in-dB for low/high-pass, detune in cents
        Wavetable.hpp              band-limited sine/tri/saw/square tables per octave (like OscillatorNode), PeriodicWave
        Noise.hpp  WaveShaper.hpp  StereoPanner.hpp  DelayLine.hpp  Oversampler.hpp
        Convolver.cpp/.hpp         partitioned FFT convolution (Accelerate vDSP) for the shared reverb
      Util/  SPSCQueue.hpp  RNG.hpp  Denormals.hpp
    AWAudio/                       Swift + AVFoundation: the bridge between the app and AWDSP
      AudioEngine.swift            AVAudioEngine + AVAudioSourceNode, session, interruptions, route/sample-rate changes
      Transport.swift              play / stop / cue / context updates → commands; snapshot publishing
      Playhead.swift               @Observable playhead, polled from the engine once per display frame
  Tests/
    AWCoreTests/                   + LocksTests, ScoreTests, ParamsTests, SnapshotBuilderTests
    AWDSPTests/                    sequencer timing (sample-exact event log), voice cap and choke, parity renders
tools/parity/render-prototype.js   renders reference audio and stats from the prototype in Chromium
App/                               SwiftUI: a play/stop button and playhead readout added to the inspector
```

## Audio engine design

### Threads and data flow

```
 Main thread                         Builder task (background)          Audio thread (render callback)
 ───────────                         ─────────────────────────          ──────────────────────────────
 AppStore.state changes ───────────▶ SnapshotBuilder: resolve locks,   ┌▶ swap in newest snapshot at block start
                                     flatten the location's songs ─────┘   (old one goes back on a return queue)
 Transport.play/stop/cue,  ── SPSC command ring (POD messages) ──────────▶ apply commands at block start
 level/selection changes
 Playhead (display link)  ◀── atomics: song, part, bar, tick, sample time; active-note ring ──
                                     Releaser: frees returned snapshots
```

- The audio thread never touches `AppState`, Swift objects, locks or the allocator. It reads an immutable C++
  snapshot, a lock-free command ring and its own preallocated voice pool. This is the store → immutable snapshot →
  lock-free queue → playhead flow from the handoff.
- Snapshots are freed off the audio thread: the engine pushes replaced snapshots onto a return queue, and a
  background releaser frees them.
- The render block in Swift does one thing: call `aw_render(engine, frameCount, left, right)`.

### Snapshot: locks resolved before they reach the audio thread

The prototype resolves every parameter through the lock chain each time a note plays (`vals()`). Natively that work
moves to the builder, off the audio thread:

- For each song in the current location, the builder flattens parts → lanes → bars → notes into contiguous arrays:
  `{id, step, pitch, len, paramBlock, rampBlock, rampMode, ratchetRate}`, plus per-part score data (bpm, swing,
  meter, key, scale) and per-song space, master, mix and fx settings.
- A **param block** is one engine's resolved values: the `SHORT` table for that engine, 0–1 as stored. Notes with no
  locks of their own share their lane's block, so a song needs a few dozen blocks rather than thousands. Mapping
  0–1 to physical units (`hz()`, `.02·200^v` and the rest) happens in C++ when a voice starts, next to the DSP.
- Snapshots are cached per song by value. An edit rebuilds only the song it touched, so the cost per edit stays
  small.

### Sequencer

The prototype's sequencer, minus its browser workarounds:

- **12 ticks per beat** (scaled by the meter's denominator). Straight bars step every 3 ticks and triplet bars
  every 2. Tick times accumulate in doubles exactly as `nextT += td` does, then convert to sample offsets, so timing
  is sample-accurate inside each render block with no 25 ms timer and no look-ahead.
- **Swing:** odd steps in straight bars are delayed by `sd · swing · 0.5`. **Probability** is rolled on the
  audio thread with a fast RNG. **Ratchets:** up to 48 repeats at `RATE_ST` spacing, held in a small pending-event
  queue, with ramps interpolating each repeat's param block (`away` / `toward`).
- **`advance()` is ported line for line:** pattern and note levels loop the selected bar (and the pattern level
  does when Loop is on), the track level loops the part, and Song/Part levels play through, plus cueing. The UI
  sends the level, selection and loop flag as commands whenever they change.
- **Mute/solo** (`mix.isAudible`), tempo, meter and swing are read per part, as `musAt([master, song, part])` does.

### Voices and DSP (aim: Web Audio's character, not its node graph)

- **Fixed pool.** Synth voices are capped at 20 and the oldest is released fast (18 ms), like `MAXV`. Drums sit
  outside the cap, and each row chokes its own last hit with a 4 ms fade. Bass and chords are mono, as are lead and
  prism unless VOICES is POLY: a new note fast-releases the slot's previous one.
- **Envelopes and sweeps use `Automation`**, a tiny fixed-capacity event list with exact AudioParam semantics. That
  makes porting `drumHit()` and `synthHit()` close to mechanical: every `setTargetAtTime` in the prototype becomes
  the same call natively, so curves match.
- **Filters use Web Audio's biquad formulas.** Notably, Q is in dB for low- and high-pass and linear for band-pass;
  that is why `Q = .5 + rs²·18` sounds the way it does. Coefficients are recomputed per 16-sample sub-block while
  cutoff moves (filter envelope, LFO via detune).
- **Oscillators** use band-limited wavetables per octave, like `OscillatorNode`. FM (oscillator B into A's
  frequency), detune in cents, glide via exponential ramps, sub-oscillator and noise all follow the prototype.
- **Drive and crush** use the prototype's own curves (`driveCurve`, `crushCurve`) with WaveShaper interpolation.
- **Routing per voice**, as `route()`: an equal-power `StereoPannerNode` pan law, a delay send of `dl²·1.2` and a
  reverb send of `rv²·1.4`, into one of four track buses. Each track bus then runs its effects buildings
  (milestone 3) into the master bus.
- **Shared space:** a tempo-synced delay with a lowpass in the feedback, and a reverb that convolves with the
  prototype's generated impulse (up to 4.5 s). Convolution is partitioned and uses Accelerate's FFT (a system
  framework, not a dependency); the impulse is regenerated off-thread when size or damp change.
- **Master:** M2 ports the lowcut, highcut and saturate stages; glue (the WebKit `DynamicsCompressor`, including
  its automatic makeup gain) arrives with the other master work in M3. Volume is `v²·1.25·0.8`.
- **Real-time rules:** everything is allocated at start-up or on a sample-rate change, denormals are flushed to zero,
  and the render path doesn't log.

### iOS integration

- `AVAudioSession` uses the `.playback` category with a 5 ms preferred IO buffer. Interruptions, route changes and
  media-services resets restart the engine. A sample-rate change rebuilds the rate-dependent DSP through a command.
- Background audio is ready but off: it needs `UIBackgroundModes: audio` and your call (see the questions below).
- Transport UI in M2 is a play/stop button and a playhead readout in the inspector. The real header arrives with
  the zoom map in M4.

### How M2 gets checked

- **Sequencer tests:** offline renders log every triggered event with its sample time. Tests check straight vs
  triplet grids, swing offsets, meter changes between parts, `advance()` per level, cue and loop, against expected
  times computed with the prototype's formulas.
- **Parity:** `tools/parity/render-prototype.js` runs the prototype's own `drumHit()` / `synthHit()` against an
  `OfflineAudioContext` in Chromium to render the first bars of Old School Hip-Hop (drums and bass solo). The Swift
  test renders the same bars and compares onset times (within 1 ms), per-voice loudness (within 1.5 dB) and
  spectral centroid. Your ears are the final test.

## Decisions (agreed)

1. **Swift for the app and UI, C++ for the DSP**, in one Xcode project, joined by a plain C header. The C++ core
   stays platform-neutral (Accelerate behind a small interface with a portable fallback), so it can also be built,
   tested and rendered to WAV off a Mac, e.g. in the cloud container Claude works in.
2. **Background audio on**: playback continues with the screen locked (`UIBackgroundModes: audio`), with
   lock-screen play/stop. Easy to turn off later.
3. **Shared delay and reverb in M2**; glue in M3.
4. **Character, not exact parity.** DSP is written from the published formulas (Web Audio spec, RBJ cookbook)
   rather than ported from browser source code, so there is no licence question. Prototype renders are a reference
   for ear and for catching mistakes (wrong timing, a voice 20 dB off), not an acceptance test. Sound parameters live
   in one place so tuning is quick, and an offline render tool makes before/after comparisons easy.
