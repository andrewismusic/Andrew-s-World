// Exports saves from the web prototype exactly as it writes them (JSON.stringify(state)), for the AWCore tests.
// Usage, from the repo root:  node tools/fixtures/export-prototype-saves.js
// Needs Playwright with Chromium (NODE_PATH pointing at a global install works).
const path = require('path');
const fs = require('fs');
const { chromium } = require('playwright');

const root = path.resolve(__dirname, '../..');
const fixtures = path.join(root, 'Packages/AWCore/Tests/AWCoreTests/Fixtures');
const resources = path.join(root, 'Packages/AWCore/Sources/AWCore/Resources');

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage();
  const errors = [];
  page.on('pageerror', e => errors.push(String(e)));
  await page.goto('file://' + path.join(root, 'reference/fractal-groovebox.html'));
  await page.waitForTimeout(1500);

  // a fresh start: the prototype's demo() state, also the app's first-launch state
  const demo = await page.evaluate(() => JSON.stringify(state));

  // the same state with the optional fields exercised: a triplet bar, a ramp toward, mute, effects,
  // master locks, a selected note, a name that needs escaping, and numbers JavaScript writes oddly
  const edited = await page.evaluate(() => {
    const s = state.worlds[0].songs[0];
    s.name = 'Ünïcode “quotes” \\ slash / tab\t';
    s.mix.m[2] = 1;
    const bar = s.sections[1].lanes[1].bars[0];
    bar.trip = true;
    const nt = bar.notes[0];
    nt.locks['rat.rt'] = 0.4;
    nt.ramp = { '1.bass.cf': 0.123456789 };
    nt.rm = 'toward';
    state.master.locks['master.vol'] = 0.7;
    state.master.locks['g.bpm'] = 1e-7;
    state.master.locks['0.drums.d0.tu'] = 0.000001234;
    s.fx[1] = ['mill', null, 'well'];
    state.sel.splice(0, 5, 0, 1, 1, 0, 0);
    state.wsel = 2;
    return JSON.stringify(state);
  });
  await browser.close();
  if (errors.length) throw new Error('prototype errors: ' + errors.join('\n'));

  fs.writeFileSync(path.join(fixtures, 'prototype-demo-save.json'), demo);
  fs.writeFileSync(path.join(fixtures, 'prototype-edited-save.json'), edited);
  fs.writeFileSync(path.join(resources, 'DefaultState.json'), demo);
  console.log('wrote prototype-demo-save.json, prototype-edited-save.json and DefaultState.json');
})().catch(e => { console.error(e); process.exit(1); });
