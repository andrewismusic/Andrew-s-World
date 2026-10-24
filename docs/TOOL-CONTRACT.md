# The tool contract

What every tool in Andrew's World has to do so that one piece of work can be three products: a place in the game, a
web tool and a standalone app or plugin. Check each tool (Relay, Diptych, the spectral processor, PIXY, Slicefield,
the Farm) against this list. A tool doesn't have to pass all of it today, but each "no" is work to plan, and the
earlier it's known the cheaper it is.

Web-first is fine. The contract is about how the web version is built, so the other versions come from the same
code instead of a rewrite.

## What "native engine" means

A tool has two halves: the **engine**, the code that makes or changes the sound, and the **interface**, the
controls you see. On the web there are two ways to build the engine:

- **Web Audio only**: the sound is built from the browser's own pieces (`OscillatorNode`, `BiquadFilterNode`,
  `ConvolverNode`, `AudioBufferSourceNode` and so on) or written in JavaScript. Quick to build and test, but those
  pieces exist only in a browser. A plugin or an iOS app would need the engine written again.
- **Portable engine** (what I called "native"): the sound code is written in C++ (or Rust), compiled to
  WebAssembly and run in an `AudioWorklet` on the web. The same source code compiles as-is for iOS, Mac and Windows,
  and inside a VST3, AU or CLAP plugin. The Farm works this way: `AWDSP` is C++, and the web page runs it as
  `awdsp.wasm`.

So the question for each thread is: does any sound pass through a Web Audio node or JavaScript maths, or does it all
happen in compiled code? A tool can start as Web Audio only and move later; the cost is rewriting the engine once.

## The checklist

### 1. Engine

- [ ] All sound processing is in portable C++ (or Rust), compiled to WebAssembly for the web. No Web Audio nodes or
      JavaScript in the sound path (playing a finished buffer for a preview is fine).
- [ ] No platform code in the engine: no browser, iOS or file-system calls. It takes numbers in and gives numbers
      out.
- [ ] One processing call: given a block of input audio (if any) and a number of frames, it fills a block of output
      audio. It works with any block size from 1 to 4096 frames.
- [ ] The sample rate is passed in, and it sounds the same at 44.1, 48 and 96 kHz.
- [ ] Safe for the audio thread: inside the processing call it never allocates memory, waits on a lock, reads a
      file or logs.
- [ ] It can render faster than real time with the same code (for bounces), and an offline render sounds exactly
      like live playback.
- [ ] Randomness comes from a seed that's part of the tool's state, so a bounce, an undo or a reload gives the same
      result.

### 2. Parameters and state

- [ ] Every control is a parameter in one table: a stable ID that never changes once shipped, a name, a range, a
      default, a unit and a curve (linear, exponential, stepped).
- [ ] Parameter changes are smoothed inside the engine, so automation and fast knob moves don't click.
- [ ] Any parameter can be automated from outside (a plugin host needs this, and so does the studio).
- [ ] The whole state (parameters, presets, the seed, anything recorded or drawn) saves as plain data (JSON), with a
      version number and a way to upgrade old saves.
- [ ] Audio the tool holds is referenced by an ID, not stored inside the state, so the game can keep it in the
      backpack.
- [ ] Nothing important lives only in the interface. Restoring a saved state restores the sound exactly, which is
      what makes undo, presets and saving inside a song work.

### 3. Audio in and out (the backpack)

- [ ] Audio comes in as a buffer by ID: 32-bit float, mono or stereo, with its sample rate.
- [ ] What the tool makes goes out the same way, so the game can put it in the backpack. (Today the pack stores up
      to two minutes per sound.)
- [ ] The tool never calls another tool. Tools only meet through audio in the backpack.
- [ ] If it uses a live input (a microphone, an instrument), it says so, and still works without one.

### 4. Time

- [ ] It can follow a host's clock: tempo, position in beats, time signature, and whether it's playing. That's how
      it syncs in a DAW and with the game's songs.
- [ ] If it plays notes, it takes them as a list of events with exact sample positions. The game doesn't use MIDI,
      but the plugin version will receive MIDI, and this is the same thing.

### 5. Interface

- [ ] The interface only talks to the engine through the parameter table and messages. It never reads or writes
      the engine's memory directly.
- [ ] Controls are laid out once, as data (what, where, how big), and drawn by a skin. The game draws it in a
      planet's style, and the standalone version is one more skin.
- [ ] It works at phone width with touch, and in a resizable plugin window with a mouse.
- [ ] No song or arrangement mode of its own. Song structure is the game's job (and the host's, in a plugin).

### 6. Building and shipping

- [ ] The same engine source builds for the web (WebAssembly), iOS (in the app, and possibly as an AUv3) and desktop
      plugins (VST3, AU, CLAP), whether through JUCE or a thin wrapper of our own.
- [ ] Every library it uses has a licence that allows selling it closed-source (no GPL).
- [ ] The engine has its own tests that run without a browser.

### 7. In the game

- [ ] It's marked portable (lives in a backpack slot, works anywhere) or stationed (stays in its place; you visit it).
- [ ] It lists what it takes from the backpack and what it gives back.
- [ ] It can point at its own controls by ID, so a character can teach it ("turn this one").
- [ ] It has an icon for its backpack slot and a silhouette for before it's found.

## Questions to send each thread

1. Does any sound go through Web Audio nodes or JavaScript, or is it all compiled code (C++ or Rust to WebAssembly)?
2. Is there a single table of parameters with stable IDs, ranges and defaults?
3. Can the whole state be saved as JSON and restored to sound exactly the same?
4. Does the engine allocate memory, lock or log while processing audio?
5. Can it render offline, faster than real time, and match live playback exactly?
6. Is any randomness seeded and saved with the state?
7. How does audio get in and out? Could it take and return a buffer by ID?
8. Does it follow an outside tempo and position?
9. Is the interface separate from the engine, talking only through parameters and messages?
10. Which libraries does it use, and under what licences?
