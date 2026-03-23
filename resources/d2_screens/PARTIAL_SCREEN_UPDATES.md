# Partial Screen Updates API for NI Kontrol D2

This document details the newly added API for performing partial screen updates on the Native Instruments Kontrol D2 screen.

## Why Partial Updates?

The screen on the D2 is 480x272 pixels at a 16-bit color depth (BGR565). This amounts to 261,120 bytes per frame. Transmitting the full frame buffer frequently over USB can saturate the USB bus capacity and result in reduced frame rates or sluggish animations.

By updating only the rectangular “bounding box” zone where pixels have changed, you can drastically reduce the USB transfer bandwidth required for animations, such as a scrolling waveform, blinking playback indicators, or track time text.

## API Documentation

### `ni_kontrol_d2_screen_blit_zone()`

Sends a specific rectangular portion of the pixel buffer to the physical screen.

```c
int ni_kontrol_d2_screen_blit_zone(struct ctlra_dev_t *dev, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
```

**Parameters:**
- `dev`: A pointer to the device context (`struct ctlra_dev_t`).
- `x`: The starting X-coordinate (0-479).
- `y`: The starting Y-coordinate (0-271).
- `w`: The width of the bounding box to update.
- `h`: The height of the bounding box to update.

**Return value:**
- Returns `0` on success.
- Returns `< 0` in case of failure (such as an invalid coordinate range or USB write error).

## Usage Example

Below is a typical flow using the partial screen update API with a tracking system for pixel differences.

```c
void d2_feedback_func(struct ctlra_dev_t *dev, void *userdata) {
    // 1. Get the screen buffer allocated by openAV-Ctlra
    uint8_t *pixels = ni_kontrol_d2_screen_get_pixels(dev);
    uint16_t *write_head = (uint16_t *)pixels;
    
    // 2. Track bounding box coordinates
    int min_y = 272, max_y = -1;
    int min_x = 480, max_x = -1;

    // 3. Draw your updated frame onto `pixels`
    // (Assume standard Cairo render setup here)
    // As you copy the frame, track which pixels actually changed:

    for (int j = 0; j < 272; j++) {
        for (int i = 0; i < 480; i++) {
            int idx = (j * 480) + i;
            uint16_t new_pixel = your_rendered_argb_to_bgr565_func(...);
            
            if (write_head[idx] != new_pixel) {
                // Expand bounding box if a pixel differs
                if (j < min_y) min_y = j;
                if (j > max_y) max_y = j;
                if (i < min_x) min_x = i;
                if (i > max_x) max_x = i;
            }
            write_head[idx] = new_pixel;
        }
    }

    // 4. Update the device bounding box natively
    if (min_x <= max_x && min_y <= max_y) {
        int rect_w = max_x - min_x + 1;
        int rect_h = max_y - min_y + 1;
        
        ni_kontrol_d2_screen_blit_zone(dev, min_x, min_y, rect_w, rect_h);
    }
}
```

## Mixing with Full Refreshes
It is widely recommended to occasionally run a full screen refresh `ni_kontrol_d2_screen_blit(dev)` (e.g., once every 60 frames) to clear out any artifacting or rendering discrepancies caused by missed USB packets.
