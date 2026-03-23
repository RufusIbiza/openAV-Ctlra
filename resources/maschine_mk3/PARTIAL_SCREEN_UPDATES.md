# Partial Screen Updates API for NI Maschine Mk3

This document details the API for performing partial screen updates on the Native Instruments Maschine Mk3 screens.

## Why Partial Updates?

The dual screens on the Maschine Mk3 are 480x272 pixels at a 16-bit color depth (BGR565), identical to the Kontrol D2. Transmitting the full frame buffer frequently over USB can saturate bandwidth.
By updating only the rectangular “bounding box” zone where pixels have changed, you reduce the USB footprint heavily.

## API Documentation

### `ni_maschine_mk3_screen_blit_zone()`

Sends a specific rectangular portion of the pixel buffer to the physical screen.

Because the Mk3 has two identical screens, you must pass a `screen_idx`.
- `0`: Left Screen
- `1`: Right Screen

```c
void ni_maschine_mk3_screen_blit_zone(struct ctlra_dev_t *dev, uint8_t screen_idx, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
```

**Parameters:**
- `dev`: A pointer to the device context (`struct ctlra_dev_t`).
- `screen_idx`: The screen you wish to push to (`0` or `1`).
- `x`: The starting X-coordinate (0-479).
- `y`: The starting Y-coordinate (0-271).
- `w`: The width of the bounding box to update.
- `h`: The height of the bounding box to update.

### `ni_maschine_mk3_screen_get_pixels()`

Returns the byte array representing the framebuffer.

```c
uint8_t* ni_maschine_mk3_screen_get_pixels(struct ctlra_dev_t* dev, uint8_t screen_idx);
```

### Mixing with Full Refreshes
It is widely recommended to occasionally run a full screen refresh `ni_maschine_mk3_screen_blit_zone(dev, screen_idx, 0, 0, 480, 272)` (e.g., once every 60 frames) to clear out any artifacting or rendering discrepancies caused by missed USB packets.
