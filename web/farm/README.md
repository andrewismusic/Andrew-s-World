# Farm (web build)

The Farm world as a private claude.ai artifact, for working on the composition flow and the sound before there is a
phone build. It starts from the prototype's workflow (`reference/fractal-groovebox.html`) with three changes:

- **Music plays through the native engine.** `awdsp.wasm` is `Packages/AWCore/Sources/AWDSP` compiled to
  WebAssembly (`./build.sh`), running in an AudioWorklet (`engine-worklet.js`): the same C++ the iOS app uses.
  Interface sounds, ambience and scale previews still use Web Audio. Edits post just the song that changed
  (`aw_engine_set_song`), so dragging a control is heard as you drag.
- **Songs are kept in the artifact's private storage** (`db`, under the viewer's own folder), as well as in the browser.
- **The backpack**: one tap on the tape bounces what you're looking at to audio in a worker (`render-worker.js`, the
  same engine) and drops it in the pack. Opening the pack dives into it; the inventory shows sixteen tool slots and
  holds sounds (in folders, stored with the artifact) and keepsakes; the Transmitter sends a sound out as a WAV (zipped inside Claude, whose downloads don't take
  .wav).

`farm.html` is now the source for this build; it no longer tracks the prototype. Decisions made here are the spec
for the SwiftUI screens.

Publish: `farm.html` as the page, with `engine-worklet.js`, `render-worker.js` and `awdsp.wasm` as files; capabilities
`{db: {}, user: {}, assets: {}, downloads: true}`.
