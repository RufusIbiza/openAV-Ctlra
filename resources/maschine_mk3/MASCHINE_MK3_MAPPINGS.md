# Maschine Mk3 Control Mappings

Hardware control mappings for the Native Instruments Maschine Mk3 driver
(`ctlra/devices/ni_maschine_mk3.c`). All defines are in
`ctlra/devices/ni_maschine_mk3.h`.

## Device overview

| Feature | Details |
|---------|---------|
| USB VID:PID | `17cc:1600` |
| Screens | 2 x 480x272 BGR565 (left = index 0, right = index 1) |
| Encoders | 1 main (push-capable) + 8 endless under the screens |
| Buttons | 71 mapped buttons across the panel |
| Pads | 16 velocity/pressure-sensitive RGB pads (4x4 grid) |
| Touchstrip | 1 capacitive strip, reported as a slider |

## Encoders (`CTLRA_EVENT_ENCODER`)

| Define | ID | Physical control |
|--------|----|------------------|
| `NI_MASCHINE_MK3_ENC_MAIN` | 0 | Large encoder (push = separate button event) |
| `NI_MASCHINE_MK3_ENC_1` - `NI_MASCHINE_MK3_ENC_8` | 1-8 | 8 endless encoders below the screens |

Encoder events carry `delta` (integer tick count) or `delta_float` depending on
the encoder. Check `e->encoder.flags & CTLRA_EVENT_ENCODER_FLAG_INT` to
distinguish.

## Pads / Grid (`CTLRA_EVENT_GRID`)

The 16 pads report as a 4x4 grid via `CTLRA_EVENT_GRID`:

- `e->grid.pos`: pad index 0-15
- `e->grid.pressed`: 1 on hit, 0 on release
- `e->grid.pressure`: 0.0-1.0 continuous pressure

Pad LEDs can be set per-pad through the grid LED interface or via
`ctlra_dev_light_set()`.

## Buttons (`CTLRA_EVENT_BUTTON`)

### Transport

| Define | ID | Label |
|--------|----|-------|
| `NI_MASCHINE_MK3_BTN_RESTART` | 36 | RESTART |
| `NI_MASCHINE_MK3_BTN_ERASE` | 37 | ERASE |
| `NI_MASCHINE_MK3_BTN_TAP` | 38 | TAP |
| `NI_MASCHINE_MK3_BTN_PLAY` | 40 | PLAY |
| `NI_MASCHINE_MK3_BTN_REC` | 41 | REC |
| `NI_MASCHINE_MK3_BTN_STOP` | 42 | STOP |

### Mode / Navigation

| Define | ID | Label |
|--------|----|-------|
| `NI_MASCHINE_MK3_BTN_MACRO` | 43 | MACRO |
| `NI_MASCHINE_MK3_BTN_SETTINGS` | 44 | SETTINGS |
| `NI_MASCHINE_MK3_BTN_SAMPLING` | 46 | SAMPLING |
| `NI_MASCHINE_MK3_BTN_MIXER` | 47 | MIXER |
| `NI_MASCHINE_MK3_BTN_PLUGIN` | 48 | PLUG-IN |
| `NI_MASCHINE_MK3_BTN_CHANNEL` | 49 | CHANNEL |
| `NI_MASCHINE_MK3_BTN_ARRANGER` | 50 | ARRANGER |
| `NI_MASCHINE_MK3_BTN_BROWSER` | 51 | BROWSER |

### Screen buttons (above the displays)

| Define | ID | Position |
|--------|----|----------|
| `NI_MASCHINE_MK3_BTN_TOP_1` - `NI_MASCHINE_MK3_BTN_TOP_8` | 55-62 | Left-to-right above the screens |

## Touchstrip / Slider (`CTLRA_EVENT_SLIDER`)

| ID | Description |
|----|-------------|
| 0 | Capacitive touchstrip, returns 0.0-1.0 |

## LEDs

Button and pad LEDs are set via:

```c
ctlra_dev_light_set(dev, button_id, 0xAARRGGBB);
ctlra_dev_light_flush(dev, 1);
```

The driver internally converts ARGB to the device's HSV-based LED protocol.
Pass `0` to turn an LED off.

## Screen infrastructure

See [PARTIAL_SCREEN_UPDATES.md](PARTIAL_SCREEN_UPDATES.md) for the full screen
API documentation, including partial updates via frame-diffing.
