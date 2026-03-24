# Partial Screen Updates for NI Maschine Mk3

This document covers the partial screen update system for the Native Instruments
Maschine Mk3's dual 480x272 BGR565 screens.

## Background

Each Mk3 screen holds 261,120 bytes (480 x 272 x 2). The device shares a single
USB bulk endpoint between both screens and can process roughly 30 full-frame
transfers per second total. Sending two full framebuffers every frame therefore
saturates the endpoint and causes one screen to stall.

Partial updates solve this by transmitting only the bounding box of pixels that
actually changed since the previous frame. A small animation that touches 10% of
the screen sends ~26 KB instead of ~261 KB, leaving bandwidth for the second
screen (and keeping USB latency low for button/pad input).

## Screen API

All functions are declared in `ctlra/devices/ni_maschine_mk3.h`.

### Get the pixel buffer

```c
uint8_t *ni_maschine_mk3_screen_get_pixels(struct ctlra_dev_t *dev,
                                            uint8_t screen_idx);
```

Returns a pointer to the device's internal pixel buffer for `screen_idx`
(`0` = left, `1` = right). The buffer is 480 x 272 x 2 bytes, laid out as
row-major BGR565 in big-endian byte order. Write your rendered pixels here
before calling one of the blit functions.

### Full-screen blit

```c
void ni_maschine_mk3_screen_blit(struct ctlra_dev_t *dev,
                                  uint8_t screen_idx);
```

Sends the entire pixel buffer to the device. Use this for the first frame
and for periodic keyframes (see below). Uses a synchronous USB bulk transfer
internally, so the call blocks until the device has accepted the data
(typically 20-35 ms).

### Partial blit (zone)

```c
void ni_maschine_mk3_screen_blit_zone(struct ctlra_dev_t *dev,
                                       uint8_t screen_idx,
                                       uint16_t x, uint16_t y,
                                       uint16_t width, uint16_t height);
```

Sends only the rectangular sub-region of the pixel buffer defined by
`(x, y, width, height)`. Coordinates are automatically aligned to hardware
constraints (x to 4-pixel, y to 2-pixel boundaries) so callers do not need
to round themselves.

**Parameters:**
| Name | Range | Description |
|------|-------|-------------|
| `screen_idx` | 0-1 | Left or right screen |
| `x` | 0-479 | Left edge of the dirty rectangle |
| `y` | 0-271 | Top edge of the dirty rectangle |
| `width` | 1-480 | Width of the dirty rectangle |
| `height` | 1-272 | Height of the dirty rectangle |

## Recommended usage pattern: frame-diffing

The most effective way to use partial updates is to diff the current frame
against the previous one, compute the bounding box of all changed pixels, and
send only that region. The example in
`examples/maschine_mk3/maschine_mk3_custom_graphics.c` demonstrates the full
pattern. Here is a condensed version:

```c
static uint16_t prev_frame[2][HEIGHT][WIDTH];

void push_screen(struct ctlra_dev_t *dev, uint8_t scr)
{
    uint16_t *pix = (uint16_t *)ni_maschine_mk3_screen_get_pixels(dev, scr);

    int min_y = HEIGHT, max_y = -1;
    int min_x = WIDTH,  max_x = -1;

    for (int j = 0; j < HEIGHT; j++) {
        for (int i = 0; i < WIDTH; i++) {
            uint16_t val = render_pixel(j, i);   /* your rendering */
            pix[j * WIDTH + i] = val;

            if (val != prev_frame[scr][j][i]) {
                if (j < min_y) min_y = j;
                if (j > max_y) max_y = j;
                if (i < min_x) min_x = i;
                if (i > max_x) max_x = i;
            }
            prev_frame[scr][j][i] = val;
        }
    }

    if (frame_count == 0 || (frame_count % 60) == 0) {
        /* Periodic keyframe to recover from any missed packets */
        ni_maschine_mk3_screen_blit(dev, scr);
    } else if (max_y >= min_y && max_x >= min_x) {
        int w = max_x - min_x + 1;
        int h = max_y - min_y + 1;
        ni_maschine_mk3_screen_blit_zone(dev, scr, min_x, min_y, w, h);
    }
    /* No changed pixels: skip the transfer entirely */
}
```

### Keyframes

Send a full blit periodically (the example uses every 60 frames) to
recover from any USB packet loss or desync. This is cheap at roughly twice
per second and ensures long-running displays never drift.

## USB transfer details

The Mk3 driver uses **synchronous** `libusb_bulk_transfer` calls for all screen
writes. This means each blit blocks the calling thread until the device has
accepted the data. The device handles are opened on the default (NULL) libusb
context; the synchronous transfer pumps that context internally, so it does not
conflict with ctlra's own event loop.

Using synchronous transfers was a deliberate choice for dual-screen devices.
The earlier async approach queued transfers via `ctlra_dev_impl_usb_bulk_write`,
but the shared inflight-write counter (capped at 10) caused whichever screen was
submitted second to be silently dropped once the pipeline filled. Synchronous
writes naturally pace both screens to the device's actual throughput.

## Pixel format

Both Mk3 screens use 16-bit BGR565 in big-endian byte order, identical to the
Kontrol D2. To convert from 8-bit RGB:

```c
static inline uint16_t rgb_to_bgr565_be(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t ri = (r >> 3) & 0x1f;
    uint16_t gi = (g >> 2) & 0x3f;
    uint16_t bi = (b >> 3) & 0x1f;
    uint16_t px = bi | (gi << 5) | (ri << 11);
    return (px >> 8) | (px << 8);   /* swap to big-endian */
}
```

If using Cairo (ARGB32 surface), the byte order at each pixel is B, G, R, A
on little-endian hosts, so pass `p[2], p[1], p[0]` as the r, g, b arguments.
