Ctlra - A C Library for Controller Support
==========================================

Ctlra is a plain C library that provides low-level access to USB HID
controllers -- buttons, encoders, sliders, pads, and high-resolution
screens -- through a single, consistent API. The details of USB protocols and
per-device quirks are handled internally so that application code can focus on
what to *do* with the controls rather than how to talk to them.

The library targets music, video, and creative software, but works for anything
that benefits from hardware control surfaces.

Features
--------

- **Unified event model** -- buttons, encoders, sliders, and grids are
  presented as typed events regardless of the underlying USB protocol.
- **Screen support** -- full-framebuffer and partial (bounding-box) screen
  updates for devices with built-in displays.
- **LED control** -- set per-button RGB LEDs via a single call; the driver
  handles colour-space conversion.
- **Hot-plug** -- devices can be connected and disconnected at runtime.
- **Zero dependencies beyond libusb** -- the core library needs only
  `libusb-1.0` and standard C. Screen examples additionally use Cairo for
  rendering.

Supported Devices
-----------------

### Full support

| Device | VID:PID | Screens | Notes |
|--------|---------|---------|-------|
| NI Kontrol D2 | `17cc:1400` | 1 x 480x272 | Full + partial screen updates |
| NI Maschine Mk3 | `17cc:1600` | 2 x 480x272 | Dual-screen partial updates, pads, encoders |
| NI Maschine Mikro Mk2 | `17cc:1200` | 1 (monochrome) | |
| NI Maschine Mikro Mk3 | `17cc:1300` | 1 (monochrome) | |
| NI Kontrol F1 | `17cc:1120` | -- | |
| NI Kontrol X1 Mk2 | `17cc:1220` | -- | |
| NI Kontrol Z1 | `17cc:1210` | -- | |
| NI Kontrol S2 Mk2 | `17cc:1320` | -- | |
| NI Kontrol S5 | `17cc:1420` | -- | |
| 3DConnexion SpaceMouse Pro (Wireless) | | -- | |

### Experimental / in progress

- NI Maschine Jam
- Akai APC
- Generic MIDI
- Arduino / Firmata serial

Building
--------

### Prerequisites

| Package | Purpose |
|---------|---------|
| `meson` + `ninja` | Build system |
| `libusb-1.0-dev` | USB communication |
| `libudev-dev` | Device enumeration (Linux) |
| `libcairo2-dev` | Screen rendering examples (optional) |
| `pkg-config` | Finding library paths |

On Debian/Ubuntu:

```
sudo apt install meson ninja-build libusb-1.0-0-dev libudev-dev \
                 libcairo2-dev pkg-config
```

### Compile the library

```
meson setup build
meson compile -C build
```

This produces `build/ctlra/libctlra.so`.

### Compile an example (e.g. Maschine Mk3 custom graphics)

```
cd examples/maschine_mk3
bash build.sh
```

Or manually:

```
gcc -o maschine_mk3_custom_graphics maschine_mk3_custom_graphics.c \
    -I../../ -I../../ctlra \
    -L../../build/ctlra -lctlra \
    $(pkg-config --cflags --libs cairo) \
    -lm -lusb-1.0 -ludev -lpthread
```

The D2 example under `examples/d2_screens/` builds the same way (substituting
the source file and include for `ni_kontrol_d2.h`).

### USB permissions

If you get permission errors, create a udev rule so your user can access the
device. For example, for all NI devices:

```
# /etc/udev/rules.d/50-native-instruments.rules
SUBSYSTEM=="usb", ATTR{idVendor}=="17cc", MODE="0666"
```

Then reload:

```
sudo udevadm control --reload-rules && sudo udevadm trigger
```

Running
-------

Each screen-capable example ships with a launcher script that sets
`LD_LIBRARY_PATH` automatically:

```
cd examples/maschine_mk3
bash run_maschine_mk3.sh
```

```
cd examples/d2_screens
bash run_d2_graphics.sh
```

Press `Ctrl+C` to exit. On clean shutdown the screens are blanked and LEDs
turned off.

Using Ctlra in your own project
--------------------------------

### Minimal skeleton

```c
#include "ctlra.h"
#include "devices/ni_maschine_mk3.h"   /* device-specific defines */

void my_event_func(struct ctlra_dev_t *dev, uint32_t num_events,
                   struct ctlra_event_t **events, void *ud)
{
    for (uint32_t i = 0; i < num_events; i++) {
        struct ctlra_event_t *e = events[i];
        switch (e->type) {
        case CTLRA_EVENT_BUTTON:
            printf("button %d %s\n", e->button.id,
                   e->button.pressed ? "down" : "up");
            break;
        case CTLRA_EVENT_ENCODER:
            printf("encoder %d delta %f\n", e->encoder.id,
                   e->encoder.delta_float);
            break;
        case CTLRA_EVENT_SLIDER:
            printf("slider %d = %f\n", e->slider.id, e->slider.value);
            break;
        case CTLRA_EVENT_GRID:
            printf("pad %d pressure %f\n", e->grid.pos,
                   e->grid.pressure);
            break;
        default: break;
        }
    }
}

void my_feedback_func(struct ctlra_dev_t *dev, void *ud)
{
    /* Draw to screens, update LEDs, etc. */
}

int accept_func(struct ctlra_t *ctlra, const struct ctlra_dev_info_t *info,
                struct ctlra_dev_t *dev, void *ud)
{
    ctlra_dev_set_event_func(dev, my_event_func);
    ctlra_dev_set_feedback_func(dev, my_feedback_func);
    return 1;   /* accept the device */
}

int main(void)
{
    struct ctlra_t *ctlra = ctlra_create(NULL);
    ctlra_probe(ctlra, accept_func, NULL);

    while (!done)
        ctlra_idle_iter(ctlra);

    ctlra_exit(ctlra);
}
```

### Screen updates

For devices with screens (D2, Maschine Mk3), the recommended workflow is:

1. **Get the pixel buffer** -- call the device-specific
   `screen_get_pixels()` to obtain a pointer to the driver's internal
   framebuffer.
2. **Render into it** -- use Cairo, raw pixel writes, or any method you
   like. The format is 480x272 BGR565 big-endian (2 bytes per pixel).
3. **Diff and send** -- compare each pixel against a `prev_frame` buffer
   to find the bounding box of changed pixels, then call `screen_blit_zone()`
   with just that rectangle. Send a full `screen_blit()` every ~60 frames as
   a keyframe to recover from any USB packet loss.

This approach keeps USB bandwidth minimal and allows both Mk3 screens to
update smoothly. See `examples/maschine_mk3/maschine_mk3_custom_graphics.c`
for a complete working implementation.

### LEDs

```c
/* Turn a button LED on (full white) */
ctlra_dev_light_set(dev, NI_MASCHINE_MK3_BTN_PLAY, 0xFF00FF00);

/* Turn it off */
ctlra_dev_light_set(dev, NI_MASCHINE_MK3_BTN_PLAY, 0);

/* Push changes to hardware */
ctlra_dev_light_flush(dev, 1);
```

The colour value is 0xAARRGGBB. The driver converts it to whatever the
hardware expects internally.

### Linking

```
gcc -o myapp myapp.c \
    -I/path/to/openAV-Ctlra -I/path/to/openAV-Ctlra/ctlra \
    -L/path/to/openAV-Ctlra/build/ctlra -lctlra \
    -lusb-1.0 -ludev -lpthread -lm
```

Add `$(pkg-config --cflags --libs cairo)` if you use Cairo for screen
rendering.

At runtime, make sure `libctlra.so` is on the library search path:

```
export LD_LIBRARY_PATH=/path/to/openAV-Ctlra/build/ctlra:$LD_LIBRARY_PATH
```

Project layout
--------------

```
openAV-Ctlra/
  ctlra/                      Core library source
    ctlra.h                   Public API (events, probing, idle loop)
    devices/
      ni_maschine_mk3.c/.h    Maschine Mk3 driver + screen API
      ni_kontrol_d2.c/.h      Kontrol D2 driver + screen API
      ...                     Other device drivers
    usb.c                     USB transport (sync + async paths)
    impl.h                    Internal driver interface
  examples/
    maschine_mk3/             Mk3 dual-screen Cairo demo
    d2_screens/               D2 screen Cairo demo
    simple/                   Minimal probe-and-print example
    vegas_mode/               LED party mode
    ...
  resources/
    maschine_mk3/             Mk3-specific docs (mappings, screen protocol)
    d2_screens/               D2-specific docs (mappings, screen protocol)
  build/                      Meson build output
```

Further documentation
---------------------

- [Maschine Mk3 control mappings](resources/maschine_mk3/MASCHINE_MK3_MAPPINGS.md)
- [Maschine Mk3 partial screen updates](resources/maschine_mk3/PARTIAL_SCREEN_UPDATES.md)
- [Kontrol D2 partial screen updates](resources/d2_screens/PARTIAL_SCREEN_UPDATES.md)
- [Kontrol D2 custom graphics guide](resources/d2_screens/D2_Custom_Graphics_README.md)
- [Device-specific notes](ctlra/devices/README.md)

Contact
-------

Harry van Haaren <harryhaaren@gmail.com>
OpenAV Productions http://openavproductions.com

License
-------

BSD. See source file headers for the full text.
