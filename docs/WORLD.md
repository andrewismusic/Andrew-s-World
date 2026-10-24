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
| Finessing transitions | **No location yet.** See gaps below. |

Each tool also ships on its own (app, web tool or plugin), with a bundle price. Tools earned in the game are a reason
to play, and can stand in for buying them.

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
game. A tool is:

- **A sound engine in portable C++**, like `AWDSP`: the same code builds for iOS, WebAssembly and a plugin format
  (CLAP or VST3 through JUCE or a thin wrapper of our own).
- **Its state as plain data**: parameters, presets and any recorded audio, so the game can save it inside a song,
  undo it and restore it.
- **Audio in and audio out through the backpack**: it can take a sound from the pack and put sounds back. This is
  the only way tools talk to each other, so no tool depends on another.
- **An interface that can be reskinned**: its controls are laid out once, and each place draws them in its planet's
  style. A tool's standalone look is one more skin. Song structure is the game's job, so a tool's own song mode
  goes, as already decided.

The other threads are worth checking against this before they go further: which of them are web only, which have a
native engine, and how each one saves its state.

### 2. Two kinds of tool

- **Portable tools** go in the backpack and work anywhere: the jar of worms, Relay, Voltai, the spectral processor,
  Slicefield, reverb, the Transmitter. These fill the sixteen slots.
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

### 4. Nothing important is locked behind the story

Every planet can be reached from early on. The story is a reason to go somewhere, not a gate. What is earned:

- **Tools**: found, given by a character or bought in a market. A tool earned in the game works in the game for
  good. Whether it also unlocks the standalone version (or a discount on it) is the business question below.
- **Sounds**: bought with an in-game currency that's easy to earn (making music, finishing parts, ear-training
  games). No real money buys currency, so the game never feels like it's selling to you.
- **Keepsakes**: things characters give you that mark what you've done.

The asteroid at the end appears once you have every tool, and you beat it with them. That's the one hard gate, and
it's at the end.

### 5. A template for every place

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

### 6. Build a vertical slice before going wide

One complete loop, end to end, before more places:

1. **The ship**: the cockpit to fly from, and a first version of the studio (stems in, levels, export).
2. **Terraflor**: the Farm (exists) and the Market (buy sounds and one tool with currency).
3. **The backpack** carrying sounds and tools between them (exists in the Farm build).
4. **One character** who teaches part of the Farm and gives a keepsake.
5. **The Transmitter** sending a finished song out (exists).

That proves the core idea (write on a planet, carry it home, finish it in the studio) and gives the other places a
pattern to follow. Rostellar (Solucan and the Wormkeeper) is the natural second planet, since its DAW and the worms
already exist.

## Gaps in the sheet

- **Transitions** have no home, though they're one of the main goals. One possibility: Megarrah's river cave, where a
  boat ride between two parts is the transition and steering it shapes the risers, fills and drops. Or bridges on the
  Farm between fields.
- **Gemelar has no DAW**, while Mycor has two (Fungi Forest and Arcade). That may be right (the gas giant is all sound
  sources), but it's worth choosing on purpose.
- **Unnamed or empty locations**: Vypra, Carnage, Constance, several "?" rows, and the living quarters.
- **The living quarters** could be home in the game sense: keepsakes on shelves, a record shelf of your finished
  songs, and the mailbox where new sample packs and challenges arrive. Then the cockpit is for travel and listening
  to the mixes from every planet, and the space station is the community.
- **The ship's AI** (the sheet's "ship computer AI?") could be the guide who appears everywhere, and the voice of the
  received transmissions.
- **The currency** needs a name and a look that fit every planet.

## Open questions

1. Does a tool earned in the game unlock its standalone version, give a discount on it, or only work in the game?
2. Which of the separate tools have a native engine, and which are web only? (This decides how much work the
   contract is.)
3. Is the studio a full DAW (eight tracks, effects, mastering, stems), or a deliberately small finishing room?
4. Is the space station live (other players' music, swapping loops) at launch, or later?
5. Should the Farm build grow into the vertical slice, or should the slice start as the iOS app?
