# The world: how to flesh it out and build it

A working plan, started October 2026 from the world spreadsheet ("ANDREW'S WORLD") and the first conversation about
it. Decisions get moved into `DESIGN-DIRECTION.md` once they're made; this file holds the proposal and the open
questions.

## What it is

Andrew's World is a platform for Andrew's ideas about how music can be made. The game is optional: someone can come
in and just use the tools. The game parts exist to make the parts of production that usually feel like work more
fun:

| Usually feels like work | How the world turns it into play |
| --- | --- |
| Keeping writing and mixing apart | Planets are where you write; the ship's studio is where you mix and finish. Multitracks travel one way. |
| Making variations on a part | Dice, Ripworms, Relay's random macros |
| Learning a complex tool | A character on the planet teaches it (the Wormkeeper, the gem keeper, the Farm's characters) |
| Finishing a whole track | The speed-run computer's timed full-track run |
| Ear training | Camilla's lighthouse, keeper's quarters and fuel shed |
| Finessing transitions | The jittery computer's quick assignments (see below) |

Each tool also ships on its own (app, web tool or plugin), with a bundle price. Tools are found in the game and are
free to use there, so finding one does not unlock its standalone version. How the standalone versions can be earned
is an open question (below).

## Three layers

Keep these apart in design and in code, so each can move at its own pace:

1. **Tools and instruments**: the products. Each has a sound engine and an interface, and runs in the game and on its
   own. Relay, Diptych, the spectral processor, PIXY and Slicefield are in progress in other threads.
2. **Places**: where a tool lives in the game. A place gives a tool its planet's look, the character who teaches
   it, and what it trades with the backpack. A place can hold a zoom DAW (Farm, Cobalt, Solucan, Acid Ocean, Ondine,
   Fungi Forest, Arcade), a tool, a sound source, a shop or a minigame.
3. **Progression and story**: why you'd go somewhere, and what you get for it. Kept light and optional.

## Proposed decisions

### 1. Write a tool contract now, before the separate tools drift apart

This is the most important decision for the build: it's what lets one tool be an app, a plugin and a place in the
game. The full checklist is in `TOOL-CONTRACT.md`. In short, a tool is:

- **A sound engine in portable C++**, like `AWDSP`: the same code builds for iOS, WebAssembly and a plugin format
  (CLAP or VST3 through JUCE or a thin wrapper of our own).
- **Its state as plain data**: parameters, presets and any recorded audio, so the game can save it inside a song,
  undo it and restore it.
- **Audio in and audio out through the backpack**: it can take a sound from the pack and put sounds back. This is
  the only way tools talk to each other, so no tool depends on another.
- **An interface that can be reskinned**: its controls are laid out once, and each place draws them in its planet's
  style. A tool's standalone look is one more skin. Song structure is the game's job, so a tool's own song mode
  goes, as already decided.

All the tools are being built web-first. That's fine as long as each one's sound code is portable (C++ or Rust
compiled to WebAssembly) rather than built from Web Audio nodes, which exist only in a browser.

### 2. Two kinds of tool

- **Portable tools** go in the backpack and work anywhere: the jar of worms, Relay, Voltai, the spectral processor,
  Slicefield, reverb, the Transmitter, the jittery computer. These fill the sixteen slots, which start empty.
- **Stationed instruments** stay where they are and you visit them: Moonglass, the atmosphere generator, the cloud
  pads, the chord generator, Rhiza, Orbital, the zoom DAWs. What they make goes in the pack as sounds.

Sixteen slots matches the portable tools in the sheet. Some may move between the two lists.

### 3. What travels where

- **Sounds go anywhere.** Bounces, imports, gems from Selena and melodies from Simone all go in the backpack, and
  any place can use them.
- **Multitracks go one way**, from a planet's DAW to the ship's studio, as audio stems. Once a song is in the studio
  it's mixed and finished there, not sent back to be rewritten. That keeps writing and mixing apart.
- **The studio can resample**: a bounce from the studio goes back into the pack as a sound, which is a normal
  producer move and keeps the one-way rule about multitracks.
- **Finished songs leave** through the Transmitter: export, stems and the space station's radio.

### 4. The ship's studio

Decided: an eight-track DAW for audio only. It's the one place in the game where you can record long takes of your
own (vocals, instruments), so writing happens on the planets and recording and finishing happen at home. It needs:

- recording long takes, possibly with take lanes to comp the best parts
- basic clip editing: trim, split, move, fade, gain
- effects on each track, and automation for them
- stems in from the planets, and bounces out to the pack and the Transmitter

### 5. Transitions: the jittery computer

Decided: transitions are a minigame. The tiny jittery computer (the speed-run computer on Adeline) is a tool you find
and keep in the backpack. It gives you quick assignments, with a timer, that keep a track moving: "build a riser into
the chorus in 90 seconds", "drop everything but the drums for two bars", "add a fill before the last part". The
timed full-track run becomes one more of its assignments.

### 6. Nothing important is locked behind the story

Every planet can be reached from early on. The story is a reason to go somewhere, not a gate. What is earned:

- **Tools**: found, given by a character or bought in a market. A tool found in the game works in the game for good.
  It does not unlock the standalone version.
- **Sounds**: bought with an in-game currency that's easy to earn (making music, finishing parts, ear-training
  games). No real money buys currency, so the game never feels like it's selling to you.
- **Keepsakes**: things characters give you that mark what you've done.

The asteroid at the end appears once you have every tool, and you beat it with them. That's the one hard gate, and
it's at the end.

### 7. A template for every place

Each location gets one page that answers the same questions:

- Planet and location; the fantasy in a sentence
- The music job it makes fun (from the table above)
- What you play with: instrument or tool, and its controls
- What it takes from the backpack and what it gives back
- Who's there and what they teach
- How you unlock or earn what's there
- Whether it's a standalone product, and which thread is building it
- Build status

The sheet already answers most of this for about half the rows. Turning each row into one of these pages is the
fastest way to find the gaps.

### 8. Build a vertical slice before going wide

One complete loop, end to end, before more places:

1. **The ship**: the cockpit to fly from, and a first version of the studio (stems in, levels, export).
2. **Terraflor**: the Farm (exists), the Market (buy sounds and one tool with currency) and a few more locations
   still to be designed.
3. **The backpack** carrying sounds and tools between them (exists in the Farm build).
4. **One character** who gives you an item.
5. **The Transmitter** sending a finished song out (exists).

That proves the core idea (write on a planet, carry it home, finish it in the studio) and gives the other places a
pattern to follow. Rostellar (Solucan and the Wormkeeper) is the natural second planet, since its DAW and the worms
already exist.

The space station's live community (other players' music, swapping loops) comes after launch.

## Gaps in the sheet

- **Gemelar has no DAW**. Decided that's fine: the cloud pads are enough, and a DAW can come later.
- **Mycor has two DAWs** (Fungi Forest and the 8-bit Arcade). Proposal: one DAW per planet, so each planet keeps one
  sound. Fungi Forest is Mycor's DAW. The Arcade becomes a stationed 8-bit instrument hidden behind a warp pipe in
  the forest: the Mario nod stays, and what you make there goes in the pack as sounds.
- **Unnamed or empty locations**: Vypra, Carnage, Constance, several "?" rows, and the living quarters.
- **The living quarters** are home: keepsakes on shelves, a record shelf of your finished songs, and the mailbox
  where new sample packs and challenges arrive. Then the cockpit is for travel and listening
  to the mixes from every planet, and the space station is the community.
- **The ship's AI** (the sheet's "ship computer AI?") could be the guide who appears everywhere, and the voice of the
  received transmissions.
- **The currency** needs a name and a look that fit every planet.

## Open questions

1. How can a player earn a tool's standalone version, if finding it in the game doesn't? Ideas:
   - **Make something real with it**: finish a song that uses the tool and send it out through the Transmitter.
   - **Master it**: the tool's character sets one hard final challenge.
   - **Buy the game, get the bundle**: the game is paid, and owning it includes the standalone versions; the free
     tools are the demo.
2. Which of the separate tools have a portable engine, and which use Web Audio nodes? (Check each against
   `TOOL-CONTRACT.md`.)
3. Should the Farm build grow into the vertical slice, or should the slice start as the iOS app?
4. Which other locations does Terraflor need?
