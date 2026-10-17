# Design direction

Decisions from the creative director, October 2026. Where this disagrees with `HANDOFF.md`, this wins.

## The game

- **Planets set the look; locations set the music.** Every location on a planet shares that planet's visual style.
  Each location brings its own way of making music: different instruments and different controls (buttons,
  sliders, XY-pad gestures), and some record audio while others only synthesise.
- **So locations are not reskins.** There is a shared core: the zoom map, transport, undo, the song → part → track
  → pattern → note levels, and the interaction rules in `HANDOFF.md`. Each location can add its own screens,
  instruments and engines on top. The code is structured as shared features plus per-location modules, not as one
  layout with swappable colours.
- **Laboratory is home.** It sits on the main planet: the player's home studio. You explore other planets to collect
  things (sounds, stems, songs you make there, perhaps abilities) and bring them back to the studio to finish them.
- **Blocks is set aside for now.**

## Planets and their hero locations

| Planet | Role | Hero location | Look |
| --- | --- | --- | --- |
| Main planet (name TBD) | Home base | Laboratory | Closest to the rest of iOS, a portal into the game: Liquid Glass |
| Terraflor | Explore | Farm | Early Nintendo |
| Rostellar | Explore | Solucan | Dark, sleek sci-fi |

### Farm: early Nintendo

- References: **Super Mario Bros. 3** world map, **The Legend of Zelda** (NES), **A Link to the Past** (SNES).
  Not Super Mario World, which blends styles instead of holding one identity.
- The colour space doesn't have to be NES-authentic; use what serves the look.
- A pixel font throughout.

### Solucan: dark sci-fi

- Dark, with **purple/indigo as the hero colour**, on the cooler, bluer side.
- References: **Marathon** (Bungie reboot) for being hyper-real yet with many completely textureless surfaces;
  **Ghost in the Shell** and **Into the Spider-Verse** for a cool cartoon style that is bold yet detailed, with high
  colour contrast and things that glow; **Cyberpunk 2077**; **The Designers Republic**; **NERV's screens in
  Evangelion**.

### Space

- Keep the planets roughly as they are, but give them a more defined identity, staying in **3D CG**, not pixel art.
- The interface around them needs a lot of work; today it is very plain. The earlier space references (Star Fox,
  Elite, Outer Wilds) didn't land.

## Platform

- **Feels like a game, fully custom.** No stock iOS chrome, except Liquid Glass as Laboratory's deliberate style.
- **Haptics:** a vibration matching rocket take-off and landing; something subtle on the zoom slider; more where it
  makes the experience better.
- **Reference style only.** All art, fonts and sounds are original; nothing is copied from the references.
