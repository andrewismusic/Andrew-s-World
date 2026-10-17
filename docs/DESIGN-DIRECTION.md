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
- From the first style page (Light World direction chosen over the NES one):
  - **Sand is the ground; grass patches are where you play.** No black backgrounds.
  - **No textures behind text.** Grid lines and patterns stay inside the playing areas.
  - **Pattern grids stay flat and crisp**: square cells and hard furrow lines, so the notes read first.
  - **Zoom rail on the left.**
  - **Panes are light and neutral** (parchment, dark ink), so the playing area keeps the contrast. Strong colour in
    the pane is only for what matters in the current mode: pink for locks, red for play and record, gold for on.
  - **Buttons are raised blocks** like the song tiles: outline, top highlight, bottom shadow. "On" changes the face
    colour and drops the shadow, so nothing moves.
  - **Two slider styles:** a block slider (groove, wood fill, gripped handle) for continuous settings, and a strip of
    square LEDs for levels and amounts.
  - **All sprites have pixel outlines**, logs and fins included.
  - **Font:** a pixel font with plain, straight letterforms (the comparison sheet is on the style page).
  - **Lanes are terrain:** drums are tilled soil, bass is vine logs on grass, chords are the sea, lead is a meadow.
  - **Every drum voice has its own crop, named by the producer's shorthand,** so the row labels are just letters:
    K kohlrabi, SN snow pea, CP corn, RM radish, CH chili, OH onion, TM tomato, PC pumpkin.
  - **Note colours by pitch class,** the same in every octave: C yellow, D green, E blue, F pink, G black, A red,
    B brown. Chord creatures vary by chord type. Bass logs stay one look, since bass is mono.
  - **Crops show their sound.** At the note level the crop is drawn large and its look follows its settings (a
    brighter clap is corn with more husk peeled back); the small grid sprites carry the same states.
  - **Characters** walk the map at the song and part levels, where there is room. While you sequence they wait on
    the sand, off the grid; a "!" means they have something to say, and tapping opens the dialogue. They never
    cover notes or move a control.

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
