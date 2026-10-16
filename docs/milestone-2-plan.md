# Milestone 2 proposal: file layout and audio engine

Status: **proposal, waiting for review.** Nothing here is built yet.

Milestone 2 (from the handoff): render loop, drums and bass, sequencer with swing and triplets, transport.
*Done when* the first Farm demo song (Old School Hip-Hop) plays in time with the prototype.

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
  spectral centroid. Safari remains the ear test.

## Decisions I need from you before starting

1. **C++ for the DSP** (recommended), behind a plain C header, versus Swift with unsafe buffers. C++ keeps ARC and
   the Swift runtime out of the render callback, and its biquad, compressor and convolution code ports directly
   from WebKit's Web Audio sources.
2. **Background audio:** should playback continue with the screen locked? It's cheap to turn on now; it affects App
   Review and the session setup.
3. **Reverb in M2 or M3?** I'd include the shared delay and reverb in M2, because the demo's kit and bass sends are
   part of how "in time with the prototype" sounds; glue stays in M3.
