# Maschine Mk3 Mappings

This document details how the various controls on the Native Instruments Maschine Mk3 map to the `ctlra` library paradigms.

## Overview

The `ni_maschine_mk3.c` driver integrates the device natively.
The device exposes:
- 1 main infinite encoder (with push)
- 8 endless encoders beneath the screens
- 71 various function buttons across the device
- 16 RGB velocity & pressure-sensitive pads
- 1 touchstrip
- 2 high-resolution screens (480x272 BGR565)

## Encoders (`CTLRA_EVENT_ENCODER`)

| Internal ID | Description |
| ----------- | ----------- |
| 0 | Main large encoder |
| 1 - 8 | The 8 individual endless encoders under the screens |

*Note*: Encoder events emit floating point delta values (`delta_float`) or integer deltas depending on the specific knob.

## Grid / Pads (`CTLRA_EVENT_GRID`)

The 16 pads generate a variety of data.
They respond with 0-15 positions.
Their velocity and pressure trigger changes ranging from `0.0` to `1.0` in the `pressure` field.
The LEDs corresponding to these pads can be set with traditional LED indexing or through grid updates via `ni_maschine_mk3.c` logic.

## Buttons (`CTLRA_EVENT_BUTTON`)

The Maschine Mk3 features a wide array of buttons that are addressed directly via standard button IDs in `ctlra`.
A few notable mapped buttons:
- `40`: PLAY
- `41`: REC
- `42`: STOP
- `36`: RESTART
- `37`: ERASE
- `38`: TAP
- `43`: MACRO
- `44`: SETTINGS
- `46`: SAMPLING
- `47`: MIXER
- `48`: PLUG-IN
- `49`: CHANNEL
- `50`: ARRANGER
- `51`: BROWSER
- `55` to `62`: The 8 top screen buttons

## Sliders (`CTLRA_EVENT_SLIDER`)

- ID `0` is the Touchstrip slider, returning a floating point value. 

## Screen Infrastructure

The Maschine Mk3 features two screens, enumerated as index `0` (Left) and `1` (Right).
To update these efficiently without sending 2 full framebuffers of data iteratively, the partial API has been added. See the `PARTIAL_SCREEN_UPDATES.md` file in this directory.
