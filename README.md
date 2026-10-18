# Andrew's World (iOS)

A native iOS port of the Andrew's World groovebox prototype. `docs/HANDOFF.md` is the brief and
`reference/fractal-groovebox.html` is the reference implementation (open it in a browser to compare; never ship it).

## Layout

| Path | What |
| --- | --- |
| `AndrewsWorld.xcodeproj` | The app. Its `App/` folder is synchronised, so new files there join the target automatically. |
| `App/` | SwiftUI app target: the store, screens, Files import/export. |
| `Packages/AWCore/` | Everything without UI. `AWCore`: the save model, its JSON codec and storage. `AWDSP`: the sound engine (C++ behind the C header `include/AWDSP.h`). `AWAudio`: connects it to the device. Tested with `swift test`. |
| `tools/parity/` | Renders songs from the prototype in Chromium and compares them with the native engine. |
| `docs/` | Handoff doc and milestone plans. |
| `reference/` | The prototype, and `demo-save.json` once you export one. |
| `tools/fixtures/` | Scripts that make the test fixtures from the prototype. |

## Build and test

Open `AndrewsWorld.xcodeproj` in Xcode 16 or later and run the AndrewsWorld scheme (⌘R), or run the core tests with `swift test` (below).
From a terminal:

```sh
swift test --package-path Packages/AWCore
# the sound engine on its own, on macOS or Linux, plus aw-render (save → WAV)
cmake -S Packages/AWCore/Sources/AWDSP -B build/awdsp && cmake --build build/awdsp && ctest --test-dir build/awdsp
build/awdsp/aw-render Packages/AWCore/Sources/AWCore/Resources/DefaultState.json out.wav --song 0 --seconds 30
xcodebuild build -project AndrewsWorld.xcodeproj -scheme AndrewsWorld -destination 'generic/platform=iOS Simulator'
```

Set your team under Signing & Capabilities to run on a device. The deployment target is iOS 17.

## Save format

The app reads and writes the prototype's save (`JSON.stringify(state)`, version 10) byte for byte: key order,
number formatting and string escaping all match JavaScript, so saves move between the web prototype and the app
unchanged. The state lives in `Application Support/AndrewsWorld/state.json`, written atomically 300 ms after the
last change; Import and Export use the Files app.
