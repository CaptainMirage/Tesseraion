# Versioning

Tesseraion versions each component independently with a three-part number,
`x.y.z`, on its own custom scheme (not SemVer).

## What each part means

| Part | Name                    | Meaning                                                       | Range   |
| ---- | ----------------------- | ------------------------------------------------------------- | ------- |
| `x`  | Full release / overhaul | A finished release or a ground-up overhaul.                   | 0..100  |
| `y`  | Major update            | A change that touches a lot, or adds new content of any kind. | 0..100  |
| `z`  | Minor update            | Small changes and fixes all around (climbs the fastest).      | 0..200  |

Each part starts at `0` and has a cap: `z` at `200`, `y` and `x` at `100`. Hitting
the cap and bumping again carries into the next part up and resets, like an
odometer: a `z` past `200` becomes `0` and bumps `y`; a `y` past `100` becomes `0`
and bumps `x`.

## Reset rules

- Bumping `x` (full release / overhaul) resets both `y` and `z` to `0`.
- Bumping `y` (major update) resets `z` to `0`.
- Bumping `z` (minor update) changes nothing else.

So `0.4.12` plus a major update becomes `0.5.0`; plus an overhaul becomes `1.0.0`.

## Pre-1.0 means beta

While `x` is `0` the component has not had its first full release yet, so it is
beta: APIs, config keys, the shader uniform contract, and behavior may still
change. The first full release of a component is `1.0.0`.

## Components and source of record

Each component keeps its version in a plain `VERSION` file holding a single
`<component>-x.y.z` line: the component name is part of the version string, not
just the tag, so a bare version reads unambiguously (`core-0.1.6`, `gnome-0.2.0`).
That file is the source of truth for the component, and components advance on their
own cadence (their numbers are unrelated).

| Component                              | Version file    | Version string | Mirrored in                                    |
| -------------------------------------- | --------------- | -------------- | ---------------------------------------------- |
| Render core (+ GLFW dev host)          | `VERSION`       | `core-x.y.z`   | (read by the build/release tooling)            |
| GNOME wallpaper host + Shell extension | `gnome/VERSION` | `gnome-x.y.z`  | `gnome/extension/metadata.json` `version-name` |

## Releases and tags

Releases are tagged per component, mirroring its `VERSION` file with a `v` inserted
before the number:

- core:  `core-vx.y.z`   (e.g. `core-v0.1.6`,  version string `core-0.1.6`)
- gnome: `gnome-vx.y.z`  (e.g. `gnome-v0.2.0`, version string `gnome-0.2.0`)

To cut a release: bump the component's `VERSION` file (and, for the GNOME host, the
`version-name` in `gnome/extension/metadata.json`) in the same change, then tag.

## Current

- core:  `core-0.1.6` (beta)
- gnome: `gnome-0.2.1` (beta)
