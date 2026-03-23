# Native Instruments Kontrol D2 Custom Graphics

This project demonstrates how to display custom graphical content on the Native Instruments Kontrol D2 screen using the openAV-Ctlra library and Cairo graphics.

## What's Working

✅ **D2 Hardware Detected**: Your Kontrol D2 is successfully detected via USB  
✅ **Library Compiled**: openAV-Ctlra library built and functional  
✅ **Custom Graphics**: Animated graphics displaying on the D2's 480x272 screen  
✅ **Button Input**: Button presses are detected and LED feedback works  

## Quick Start

Run the demo program:
```bash
./run_d2_graphics.sh
```

Or manually:
```bash
export LD_LIBRARY_PATH=./openAV-Ctlra/build/ctlra:$LD_LIBRARY_PATH
./d2_custom_graphics
```

Press `Ctrl+C` to exit.

## What You're Seeing

The current demo displays:
- Animated gradient background (dark blue/purple)
- Pulsing circle that moves in a figure-8 pattern
- Main title text: "CUSTOM D2 GRAPHICS"
- Subtitle with library credits
- Frame counter (top left)
- 16 colorful animated bars at the bottom
- Pulsing ring effects in the center

Everything updates at **30 FPS** for smooth animation.

## D2 Hardware Specifications

- **Screen Resolution**: 480x272 pixels
- **Color Format**: BGR565 (16-bit, 2 bytes per pixel)
  - 5 bits for blue
  - 6 bits for green  
  - 5 bits for red
- **Total Screen Buffer**: 261,120 bytes (480 × 272 × 2)

## Customizing the Graphics

The graphics are drawn using the Cairo library in the `draw_custom_graphics()` function in `d2_custom_graphics.c`.

### Basic Drawing Examples

#### Draw a Rectangle
```c
cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);  // Red color
cairo_rectangle(cr, x, y, width, height);
cairo_fill(cr);
```

#### Draw a Circle
```c
cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);  // Green color
cairo_arc(cr, center_x, center_y, radius, 0, 2 * M_PI);
cairo_fill(cr);
```

#### Draw Text
```c
cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);  // White color
cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
cairo_set_font_size(cr, 24);
cairo_move_to(cr, x, y);
cairo_show_text(cr, "Hello D2!");
```

#### Draw with Gradients
```c
cairo_pattern_t *gradient = cairo_pattern_create_linear(0, 0, WIDTH, 0);
cairo_pattern_add_color_stop_rgb(gradient, 0.0, 1.0, 0.0, 0.0);  // Red
cairo_pattern_add_color_stop_rgb(gradient, 1.0, 0.0, 0.0, 1.0);  // Blue
cairo_set_source(cr, gradient);
cairo_rectangle(cr, 0, 0, WIDTH, HEIGHT);
cairo_fill(cr);
cairo_pattern_destroy(gradient);
```

## Creating Animations

Use the `frame_count` global variable to create animations:

```c
// Rotating element
double angle = (frame_count * 2) % 360;
double x = center_x + cos(angle * M_PI / 180.0) * radius;
double y = center_y + sin(angle * M_PI / 180.0) * radius;

// Pulsing size
double scale = 1.0 + 0.3 * sin(frame_count * 0.1);

// Color cycling
double hue = (frame_count * 2) % 360;
```

## Handling User Input

Button presses and other controls are handled in the `d2_event_func()` function:

```c
case CTLRA_EVENT_BUTTON:
    if (e->button.pressed) {
        // Button was pressed
        int button_id = e->button.id;
        
        // Light up the button LED
        ctlra_dev_light_set(dev, button_id, UINT32_MAX);
        ctlra_dev_light_flush(dev, 1);
    }
    break;
```

### Available Event Types
- `CTLRA_EVENT_BUTTON`: Button presses
- `CTLRA_EVENT_ENCODER`: Rotary encoder turns
- `CTLRA_EVENT_SLIDER`: Fader/slider movements
- `CTLRA_EVENT_GRID`: Pad presses (if applicable)

## Controlling LEDs

Light up specific LEDs (see `ni_kontrol_d2.h` for LED IDs):
```c
// Turn on LED at full brightness
ctlra_dev_light_set(dev, NI_KONTROL_D2_LED_PLAY, UINT32_MAX);

// Turn off LED
ctlra_dev_light_set(dev, NI_KONTROL_D2_LED_CUE, 0);

// Dim LED (50%)
ctlra_dev_light_set(dev, NI_KONTROL_D2_LED_SYNC, UINT32_MAX / 2);

// Flush changes to hardware
ctlra_dev_light_flush(dev, 1);
```

### Control the Touchstrip LEDs
```c
uint8_t orange[25] = {0};
uint8_t blue[25] = {0};

// Light up first 10 LEDs in blue
for (int i = 0; i < 10; i++) {
    blue[i] = 0xff;
}

ni_kontrol_d2_light_touchstrip(dev, orange, blue);
```

## File Structure

```
D2 Screens/
├── openAV-Ctlra/              # The library (cloned from GitHub)
│   ├── build/                 # Compiled library
│   ├── ctlra/                 # Library source
│   └── examples/              # Example programs
├── d2_custom_graphics.c       # Your custom graphics program
├── d2_custom_graphics         # Compiled executable
├── run_d2_graphics.sh         # Launcher script
└── README.md                  # This file
```

## Recompiling After Changes

When you modify `d2_custom_graphics.c`:

```bash
gcc -o d2_custom_graphics d2_custom_graphics.c \
    -I. \
    -L./openAV-Ctlra/build/ctlra \
    -lctlra \
    $(pkg-config --cflags --libs cairo) \
    -lm -lusb-1.0 -ludev -lpthread
```

Or create a simple `build.sh` script for convenience.

## Troubleshooting

### "No Kontrol D2 found!"
- Check USB connection: `lsusb | grep -i native`
- You should see: `17cc:1400 Native Instruments Traktor Kontrol D2`
- Try unplugging and replugging the D2

### Permission Issues
You may need udev rules for USB access. Create `/etc/udev/rules.d/50-ni-kontrol-d2.rules`:
```
SUBSYSTEM=="usb", ATTR{idVendor}=="17cc", ATTR{idProduct}=="1400", MODE="0666"
```

Then reload: `sudo udevadm control --reload-rules`

### Library Not Found
Make sure to set `LD_LIBRARY_PATH` before running:
```bash
export LD_LIBRARY_PATH=./openAV-Ctlra/build/ctlra:$LD_LIBRARY_PATH
```

## Next Steps & Ideas

Here are some ideas for what you can build:

1. **Audio Visualizer**: Use ALSA/JACK to capture audio and display waveforms or spectrum analyzer
2. **System Monitor**: Display CPU usage, RAM, network stats
3. **MIDI Controller Display**: Show MIDI notes, CC values, etc.
4. **DJ Waveform Display**: Show track waveforms and playhead position
5. **Custom Synthesizer UI**: Control parameters and see visual feedback
6. **Game**: Create a simple game using the pads and screen
7. **Clock/Weather Display**: Show time, date, weather info
8. **VU Meters**: Audio level meters with peak hold

## Cairo Resources

Learn more about Cairo graphics:
- Cairo tutorial: https://www.cairographics.org/tutorial/
- Cairo manual: https://www.cairographics.org/manual/
- API reference: https://www.cairographics.org/manual/cairo-drawing.html

## openAV-Ctlra Resources

- GitHub: https://github.com/openAVproductions/openAV-Ctlra
- Device info: See `openAV-Ctlra/ctlra/devices/ni_kontrol_d2.h`
- More examples: See `openAV-Ctlra/examples/vegas_mode/`

## License

This project uses the openAV-Ctlra library, which is licensed under the BSD license.

---

**Have fun creating custom graphics for your Kontrol D2!** 🎨🎛️
