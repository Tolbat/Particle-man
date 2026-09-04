# Particle-Man

Particle-Man is a procedural neon maze-chase game for the Atari Jaguar, built with JagStudio and RAPTOR.

Navigate a 50-level campaign of evolving mazes, collect fruit, master rechargeable abilities, evade increasingly dangerous enemies, and compete for a place in the Hall of Sparks.

## Development Status

Particle-Man is currently a release candidate undergoing final testing on real Atari Jaguar hardware.

The gameplay and campaign are feature-complete. A final ROM will be released separately after the hardware acceptance process is complete.

This repository contains the source and assets needed to compile the game. It intentionally does not include a compiled ABS or ROM.

## Features

- 50-level campaign
- Ten five-level gameplay chapters
- Ten distinct maze layouts
- Four classic ghost personalities
- Four advanced arcade threats:
  - Vector Wraith
  - Crawler Shade
  - Otto Echo
  - Flipper Phantom
- Articulated Ghost Centipede
- Spark Chain scoring system
- Fruit Fever pellet magnet
- Warp Gates
- Ghost Alarm
- Prism Pressure
- Particle Storm
- Rechargeable Dash
- Prism Pulse
- Protective Shield
- Procedurally generated fruit silhouettes
- Two-fruit objectives on levels 1–25
- Three-fruit objectives on levels 26–50
- Persistent top-five Hall of Sparks scores
- Persistent campaign progression
- NTSC and PAL timing support
- Hidden arcade discoveries

## Controls

| Control | Action |
| --- | --- |
| D-pad | Move and buffer turns |
| C | Shield |
| B | Prism Pulse |
| A | Dash |
| Pause or Option | Pause or resume |
| STAR or HASH | Reset the game and return to the title |
| Keypad 8 | Toggle sound |

The face-button controls are presented in the Jaguar controller’s physical **C B A** order.

## Abilities

### Shield — C

Creates a protective field that absorbs one enemy collision. Power pellets refill Shield.

### Prism Pulse — B

Temporarily freezes vulnerable threats and clears accumulated Prism Pressure. Collecting fruit adds one Pulse charge.

### Dash — A

Provides a short burst of speed. Dash recharges through the visible HUD meter, and power pellets refill it immediately.

Completing the fruit objective unlocks unlimited Dash for the remainder of the current level.

## Procedural Presentation

Particle-Man does not use pre-rendered gameplay sprites.

The game generates its visual identity through code, including:

- Particle-Man animation
- Ghost and specialist-enemy animation
- Ghost Centipede body segments
- Fruit silhouettes
- Maze graphics
- Collectibles and power pellets
- Player and enemy energy fields
- Particle effects
- Event sound effects

The Jaguar Object Processor displays the persistent procedural playfield and actors. RAPTOR’s GPU manages a bounded transparent particle surface for event effects.

All gameplay sound effects are generated into memory during startup. The absence of background music is intentional and preserves the focused arcade soundscape.

## Campaign

Every five levels begins a new chapter.

Chapters introduce new maze geometry, enemy behavior, scoring rules, hazards, movement systems, or combinations of previously introduced mechanics. Difficulty progression involves more than palette and speed changes.

Level 50 contains the campaign finale, final Particle Bonus, ending presentation, score handling, progression save, and return to the Hall of Sparks.

## Scores and Progression

Particle-Man stores:

- The five highest qualifying scores
- The highest legitimately unlocked level

A complete high-score run begins at level 1.

Previously unlocked levels can be selected from the title screen for continued campaign play. Continued runs begin with a fresh score and cannot enter the Hall of Sparks, but they can unlock additional levels and complete the campaign.

## Building

Particle-Man is distributed as a JagStudio C project.

1. Install and configure JagStudio.
2. Create a `ParticlePac` folder under the JagStudio C projects directory.
3. Copy the repository contents into that folder while preserving the `assets` directory structure.
4. Build `ParticlePac` using the normal JagStudio C-project workflow.
5. Test the resulting build using compatible Atari Jaguar hardware or development equipment.

The expected project location is:

`jagstudio/projects/c/ParticlePac`

The repository includes the required source, Jaguar assembly integration, generated credit data, font assets, palette asset, and JagStudio asset manifest.

## Repository Contents

- `ParticlePac.c` — main game, presentation, AI, scoring and procedural-generation source
- `common.h` — shared Jaguar object and memory declarations
- `credit_art.h` — embedded publication and technology artwork
- `rapapp.s` — RAPTOR object and memory definitions
- `rapinit.s` — Jaguar initialization configuration
- `rapu235.s` — JagStudio audio sample-bank hook
- `assets.txt` — JagStudio asset manifest
- `assets/` — required font and palette assets
- `README.md` — public project information

## Hardware Target

The authoritative target is a real Atari Jaguar.

Development decisions involving timing, performance, controls, audio volume, television-distance readability, and Object Processor behavior are evaluated using physical Jaguar hardware.

Emulator behavior may differ from the final hardware result.

## Credits

Published by **Tolbat Games**.

Developed by **GPT-5.6 Sol** using **OpenAI Codex**.

Built with **JagStudio** and **RAPTOR**.

Development was completed through an extended collaboration with Tolbat Games involving game design, programming, source review, performance investigation, publishing decisions, and repeated testing on real Atari Jaguar hardware.

## Release

This repository currently contains pre-release source code.

A verified public build will be provided separately after final Atari Jaguar hardware testing is complete.

Some arcade discoveries are intentionally left undocumented.
