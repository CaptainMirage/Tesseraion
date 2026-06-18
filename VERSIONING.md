# Versioning

Tesseraion versions each component independently, using Semantic Versioning
(`MAJOR.MINOR.PATCH`).

## Components and source of record

Each component keeps its version in a plain `VERSION` file holding a single
`MAJOR.MINOR.PATCH` line. That file is the source of truth for the component.

| Component                              | Version file                | Mirrored in                                  |
| -------------------------------------- | --------------------------- | -------------------------------------------- |
| Render core (+ GLFW dev host)          | `VERSION` (repo root)       | (read by the build/release tooling)          |
| GNOME wallpaper host + Shell extension | `gnome/VERSION`             | `gnome/extension/metadata.json` `version-name` |

Components advance on their own cadence; their numbers are unrelated (the core can
be at 0.1.6 while the GNOME host is at 0.1.0).

## Pre-1.0 means beta

Anything below `1.0.0` is beta: the API, config keys, shader uniform contract, and
behavior may change between releases. The first stable release of a component is
`1.0.0`. No `-beta` / `-rc` suffixes are used; a `0.x` number already signals beta.

## Releases and tags

Releases are tagged per component, mirroring its `VERSION` file:

- core:  `core-vMAJOR.MINOR.PATCH`   (e.g. `core-v0.1.6`)
- gnome: `gnome-vMAJOR.MINOR.PATCH`  (e.g. `gnome-v0.1.0`)

To cut a release: bump the component's `VERSION` file (and, for the GNOME host, the
`version-name` in `gnome/extension/metadata.json`) in the same change, then tag.

## Current

- core:  `0.1.6` (beta)
- gnome: `0.1.0` (beta)
