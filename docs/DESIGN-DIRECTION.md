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
- **Tools from Andrew's other apps** (a chord generator, an XY-pad drum machine, a breakbeat mangler, and more to
  come) live in locations. In the game they take the look of the world they're in, not their standalone app's look,
  and they can be pared down: song structure, for example, is handled by the game, so a tool's own song mode goes.
- **Where unlocked things can be used is still open.** The leaning: big locations like Farm stay where they are and
  you travel to them; smaller finds (an effect, a tool) can come home, or live in a backpack you carry everywhere.

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
    the pane is only for what matters in the current mode: pink for locks, red for play and record, coin yellow
    (#ffc818) for on and selected.
  - **Sand and parchment carry a faint pixel grain**, sparse enough that text over it stays clean.
  - **Buttons are raised blocks** like the song tiles: outline, top highlight, bottom shadow. "On" changes the face
    colour and drops the shadow, so nothing moves.
  - **Block sliders** (groove, wood fill, gripped handle) for settings. A locked setting turns pink and keeps a dark
    notch at the value it would have without the lock.
  - **LED meters only for audio levels** (mix, master). Dragging sets how many lights can glow; the sound passing
    through lights them green, then yellow and orange near the top. No outlines, even spacing.
  - **All sprites have pixel outlines**, logs and fins included.
  - **Fonts:** Silkscreen for titles, labels and buttons; Jersey 15 for values, body text and dialogue.
  - **Lanes are terrain:** drums are tilled soil, bass is vine logs on grass, chords are the sea, lead is a meadow.
  - **Every drum voice has its own crop, named by the producer's shorthand,** so the row labels are just letters:
    K kale, SN snap pea, C corn, R radish, CH chili, OH onion, TM tomato, PC pumpkin.
  - **Note colours by pitch class,** the same in every octave: C bright yellow, D green, E royal blue, F hot pink,
    G dark grey, A red, B brown. Note names sit right next to the grid; no separate key. Chord creatures vary by chord type. Bass logs stay one look, since bass is mono.
  - **Crops show their sound, with no captions.** Several visual traits each follow a setting and combine freely;
    for the clap: decay sets height, tone peels the husk, tune sets ripeness (colour), drive brings pests. At the
    note level the crop is drawn large; the 8-pixel grid sprites carry the same traits in miniature.
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

## Composition workflow (decided in the Farm web build)

- **Drums:** notes have no length. Tapping plants a hit; dragging across empty ground plants a hit on every cell you
  pass. Dragging a hit moves it.
- **No triplet grid.** Bars are straight sixteenths; triplet feels come from ratcheting a note at 1/24. Older triplet
  bars convert to the nearest sixteenths.
- **The pattern view stays on the bar you chose** while the part keeps playing; a marker shows which bar is playing.
  Loop still holds playback to the one bar.
- **Melodic notes can run past the bar line** into the bars after it: an arrow marks where a note continues, and the
  next bar shows its tail, faded.
- **Bars are added and duplicated at the track level only**, where you can see what happens; the pattern level has no
  plus or duplicate.
- **Randomizing notes:** an empty bar gets a fresh pattern; a bar with notes is reworked around them. Some notes go,
  some move to another pitch at the same moment, some move to another step, and a few new ones join. A bigger roll
  changes more. The fuller the bar, the more notes go and the fewer join, so rolling again and again doesn't fill it
  up. Bass, chords and lead come out monophonic, and chords only rarely ring for a long time.
- **The dice always do something you can hear**, even at their smallest; Locks also works on a single note.
- **Randomizing locks:** a bigger roll locks more notes and more settings and moves them further; at the top it can
  set any value.
- **Locked notes are hatched in pink**, like an empty bar's hatching (an outline read as a selection). There are no
  lock counts on tracks.
- **A fader shows and edits what you hear.** With Lock off it changes the value where the sound comes from: the song,
  or a lock that already wins at this level (so a lock set by the sound die can always be adjusted). With Lock on it
  pins the value at this level. With notes lassoed in the pattern view, faders and Clear locks work on those notes;
  turning the lasso off deselects.
- **Edits are heard while you drag**, including on notes already sounding (level, mix, filter, LFOs, effects, sends).
- **Dragging a note's end past the bar's edge turns the page** to the next bar, and the end stays under your finger.
  The faded tail in the next bar can be grabbed to change where the note ends.
- **Row names sit in a gutter** at the left of every bar, so they never cover a step.
- **Moving to another level ends a recording.**
- **LFO critters keep their size**: the mushroom grows out of the log and back in time with its LFO. On the track view
  at most one critter of each kind shows per track, drawn small.
- **Parts are drawn at one scale on every page**: four rows of eight bars. Parts have names (letters by default) that
  you can change on the part page.
- **The Farm has no All button** (Solucan keeps it). Keys is a piano icon.
- **Chords are ducks**: a mother duck leads at the start of the chord and her ducklings trail to the right, so a
  note looks natural at any length.
- **A note that runs past the bar line shows no end there**, and its continuation shows no start: ends are drawn
  only where a note really ends.
- **Zooming into a note dives into one pixel of its main colour**, which becomes the note view's ground.
- **Effects are buildings you walk to**: paging pans sideways across the farm, and opening an effect zooms you up to
  its wall, where the controls are plaques. A magnifier zooms back out.

## Style rule: zooming is moving closer

Every change of level should feel like moving closer to (or further from) the thing you tapped, never like cutting
to a new screen:

- What you zoom into grows smoothly from exactly how it looked. Zooming into a note shows that note's own sprite (the
  log, pumpkin, ducks or flower) growing in its own field, with square pixels all the way, until you're looking at it
  "under the microscope".
- Text never just vanishes. A name on the thing you're entering grows with it and slides out of view; names elsewhere
  move away with their ground.
- Paging is walking sideways: the old view slides out as the new one slides in.

## The backpack (decided)

- **One tap to bounce.** The arrow beside the pack renders what you're looking at (the note, the bar of a track, the
  part, the song) to audio in the background while you keep working. The arrow fills as it renders; when it's done a
  little sound flies into the pack, the pack jumps with a chime and counts what's new. No dialog, no pause.
- **Audio only.** The game doesn't export MIDI: committing ideas to audio is part of how it works.
- **Opening the pack zooms into it**: a fixed, full-screen inventory with pockets for Sounds (in folders; bounces
  file themselves by location, imports into "Imported"; imported audio keeps its first minute), Patterns (found, not
  made), Tools and Keepsakes.
- **Tools work two ways.** Some act on what you're working on (the jar of worms eats, darkens and stutters notes on
  the pattern or part you're on). Others are places you take audio to: the Transmitter sends a sound out of the game
  or to the home studio; Slicefield (not met yet) will cut and rearrange audio and hand it back.
- **Direction under discussion:** locations are where particular sounds and tools are found; the general DAW tools
  live in the home studio, so a song moves through stages: ideas, development, structure, then commit and polish.

## Style rules

- **Zooms only move and scale things.** Nothing fades or turns transparent while zooming in or out; text that leaves
  slides out of view.
- **Popups never change size** with what they show. The backpack fills the screen.
- **Sound parameters are knobs**, in every location; faders are kept for mix levels.
