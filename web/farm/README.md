# Farm (web build)

The Farm world as a private claude.ai artifact, for working on the composition flow and the sound before there is a
phone build. It starts from the prototype's workflow (`reference/fractal-groovebox.html`) with three changes:

- **Music plays through the native engine.** `awdsp.wasm` is `Packages/AWCore/Sources/AWDSP` compiled to
  WebAssembly (`./build.sh`), running in an AudioWorklet (`engine-worklet.js`): the same C++ the iOS app uses.
  Interface sounds, ambience and scale previews still use Web Audio.
- **Songs are kept in the artifact's private storage** (`db`, under the viewer's own folder), as well as in the browser.
- **The backpack**: collected sounds (planted on drum rows, stored with the artifact) and gear.

`farm.html` is now the source for this build; it no longer tracks the prototype. Decisions made here are the spec
for the SwiftUI screens.

Publish: `farm.html` as the page, with `engine-worklet.js` and `awdsp.wasm` as files; capabilities
`{db: {}, user: {}, assets: {}, downloads: true}`.
