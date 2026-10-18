# Farm style page: art generators

The Farm style page (a design canvas on claude.ai, linked from `docs/DESIGN-DIRECTION.md`) uses pixel sprites made by
these scripts from text grids. They are first-pass art for reviewing the direction, not final assets.

- `sprites.py`: the prototype's own sprites (trees, pumpkin, flower, vine) in an NES and an SNES palette.
- `sprites2.py`: round 2 crops, scale-degree flowers, the farmer and the "!" bubble.
- `sprites3.py`: round 3 crops, pixel logs and fins, first corn states.
- `sprites4.py`: round 4 sand and parchment grain, kale, the final note colours, and corn drawn from four settings
  at once (decay = height, tone = husk, tune = ripeness, drive = pests).
- `asset-ids.txt`: where each uploaded sprite lives on the style page.

Run from this folder: `python3 sprites.py` (then `sprites2.py` …); each writes SVGs into an `assets*` folder.
