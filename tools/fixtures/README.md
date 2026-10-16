# Test fixtures from the prototype

The AWCore tests compare the app's save output with what the web prototype writes. These scripts make the
fixtures from `reference/fractal-groovebox.html` in headless Chromium, so they are the prototype's own output.

| Script | Writes |
| --- | --- |
| `export-prototype-saves.js` | `prototype-demo-save.json` (a fresh `demo()` state, also copied to `DefaultState.json`, the app's first-launch state) and `prototype-edited-save.json` (the same state with triplets, ramps, mute, effects, master locks, a selected note and awkward strings) |
| `js-format-cases.js` | `js-format-cases.json`: numbers and strings with what `JSON.stringify` makes of them |

Run from the repo root after updating the prototype:

```sh
NODE_PATH=$(npm root -g) node tools/fixtures/export-prototype-saves.js   # needs Playwright + Chromium
node tools/fixtures/js-format-cases.js
```

Your own save goes in `reference/demo-save.json` (in the prototype, run `copy(JSON.stringify(state))` in
the browser's developer console and paste it into the file). `testReferenceDemoSaveRoundTrips` picks it up.
