#define _DEFAULT_SOURCE
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "ctlra.h"
#include "devices/ni_kontrol_d2.h"
#include <cairo/cairo.h>

#define WIDTH 480
#define HEIGHT 272
#define MAX_EVENTS 10

static cairo_surface_t *surface = NULL;
static cairo_t *cr = NULL;
static volatile uint32_t done = 0;

typedef struct {
  char type[32];
  char name[64];
  char value[64];
  int id;
  time_t timestamp;
} EventRecord;

static EventRecord event_history[MAX_EVENTS];
static int event_count = 0;
static float touchstrip_position = 0.5f;
static int touchstrip_active = 0;

/* CORRECT Button-to-LED mapping based on physical testing! */
int button_to_led(int button_id) {
  switch (button_id) {
  /* Pads - LEDs 0-7 */
  case NI_KONTROL_D2_BTN_PAD_1:
    return 0;
  case NI_KONTROL_D2_BTN_PAD_2:
    return 1;
  case NI_KONTROL_D2_BTN_PAD_3:
    return 2;
  case NI_KONTROL_D2_BTN_PAD_4:
    return 3;
  case NI_KONTROL_D2_BTN_PAD_5:
    return 4;
  case NI_KONTROL_D2_BTN_PAD_6:
    return 5;
  case NI_KONTROL_D2_BTN_PAD_7:
    return 6;
  case NI_KONTROL_D2_BTN_PAD_8:
    return 7;

  /* FX - LEDs 8-12 */
  case NI_KONTROL_D2_BTN_FX_SELECT:
    return 8;
  case NI_KONTROL_D2_BTN_FX_1:
    return 9;
  case NI_KONTROL_D2_BTN_FX_2:
    return 10;
  case NI_KONTROL_D2_BTN_FX_3:
    return 11;
  case NI_KONTROL_D2_BTN_FX_4:
    return 12;

  /* Screen buttons - LEDs 13-20 (linear mapping) */
  case NI_KONTROL_D2_BTN_SCREEN_LEFT_1:
    return 13; /* Left 1 (Brightness) → LED 13 */
  case NI_KONTROL_D2_BTN_SCREEN_LEFT_2:
    return 14; /* Left 2 → LED 14 */
  case NI_KONTROL_D2_BTN_SCREEN_LEFT_3:
    return 15; /* Left 3 → LED 15 */
  case NI_KONTROL_D2_BTN_SCREEN_LEFT_4:
    return 16; /* Left 4 → LED 16 */
  case NI_KONTROL_D2_BTN_SCREEN_RIGHT_1:
    return 17; /* Right 1 (Mode) → LED 17 */
  case NI_KONTROL_D2_BTN_SCREEN_RIGHT_2:
    return 18; /* Right 2 → LED 18 */
  case NI_KONTROL_D2_BTN_SCREEN_RIGHT_3:
    return 19; /* Right 3 → LED 19 */
  case NI_KONTROL_D2_BTN_SCREEN_RIGHT_4:
    return 20; /* Right 4 → LED 20 */
  case NI_KONTROL_D2_BTN_BACK:
    return 21;
  case NI_KONTROL_D2_BTN_CAPTURE:
    return 22;
  case NI_KONTROL_D2_BTN_EDIT:
    return 23;

  /* ON/FX2 buttons - LEDs 24-27 */
  case NI_KONTROL_D2_BTN_ON_1:
    return 24;
  case NI_KONTROL_D2_BTN_ON_2:
    return 25;
  case NI_KONTROL_D2_BTN_ON_3:
    return 26;
  case NI_KONTROL_D2_BTN_ON_4:
    return 27;

  /* Mode buttons - LEDs 28-34 */
  case NI_KONTROL_D2_BTN_HOTCUE:
    return 28;
  case NI_KONTROL_D2_BTN_LOOP:
    return 29;
  case NI_KONTROL_D2_BTN_FREEZE:
    return 30;
  case NI_KONTROL_D2_BTN_REMIX:
    return 31;
  case NI_KONTROL_D2_BTN_FLUX:
    return 32;
  case NI_KONTROL_D2_BTN_DECK:
    return 33;
  case NI_KONTROL_D2_BTN_SHIFT:
    return 34;

  /* Transport - LEDs 35-37 */
  case NI_KONTROL_D2_BTN_SYNC:
    return 35;
  case NI_KONTROL_D2_BTN_CUE:
    return 36;
  case NI_KONTROL_D2_BTN_PLAY:
    return 37;

  /* Deck select - LEDs 38-41 */
  case NI_KONTROL_D2_BTN_DECK_A:
    return 38;
  case NI_KONTROL_D2_BTN_DECK_B:
    return 39;
  case NI_KONTROL_D2_BTN_DECK_C:
    return 40;
  case NI_KONTROL_D2_BTN_DECK_D:
    return 41;

  default:
    return -1;
  }
}

int is_pad_button(int button_id) {
  return (button_id >= NI_KONTROL_D2_BTN_PAD_1 &&
          button_id <= NI_KONTROL_D2_BTN_PAD_8);
}

static inline void pixel_convert_from_argb(int r, int g, int b, uint8_t *data) {
  r = ((int)((r / 255.0) * 31)) & ((1 << 5) - 1);
  g = ((int)((g / 255.0) * 63)) & ((1 << 6) - 1);
  b = ((int)((b / 255.0) * 31)) & ((1 << 5) - 1);
  uint16_t combined = (b | g << 5 | r << 11);
  data[0] = combined >> 8;
  data[1] = combined & 0xff;
}

void add_event(const char *type, const char *name, const char *value, int id) {
  if (event_count >= MAX_EVENTS) {
    for (int i = MAX_EVENTS - 1; i > 0; i--) {
      event_history[i] = event_history[i - 1];
    }
  } else {
    event_count++;
  }

  strncpy(event_history[0].type, type, sizeof(event_history[0].type) - 1);
  strncpy(event_history[0].name, name, sizeof(event_history[0].name) - 1);
  strncpy(event_history[0].value, value, sizeof(event_history[0].value) - 1);
  event_history[0].id = id;
  event_history[0].timestamp = time(NULL);
}

const char *get_button_name(int id) {
  switch (id) {
  case NI_KONTROL_D2_BTN_PLAY:
    return "PLAY";
  case NI_KONTROL_D2_BTN_CUE:
    return "CUE";
  case NI_KONTROL_D2_BTN_SYNC:
    return "SYNC";
  case NI_KONTROL_D2_BTN_SHIFT:
    return "SHIFT";
  case NI_KONTROL_D2_BTN_HOTCUE:
    return "HOTCUE";
  case NI_KONTROL_D2_BTN_LOOP:
    return "LOOP";
  case NI_KONTROL_D2_BTN_FREEZE:
    return "FREEZE";
  case NI_KONTROL_D2_BTN_REMIX:
    return "REMIX";
  case NI_KONTROL_D2_BTN_FLUX:
    return "FLUX";
  case NI_KONTROL_D2_BTN_DECK:
    return "DECK";
  case NI_KONTROL_D2_BTN_DECK_A:
    return "DECK A";
  case NI_KONTROL_D2_BTN_DECK_B:
    return "DECK B";
  case NI_KONTROL_D2_BTN_DECK_C:
    return "DECK C";
  case NI_KONTROL_D2_BTN_DECK_D:
    return "DECK D";
  case NI_KONTROL_D2_BTN_PAD_1:
    return "PAD 1";
  case NI_KONTROL_D2_BTN_PAD_2:
    return "PAD 2";
  case NI_KONTROL_D2_BTN_PAD_3:
    return "PAD 3";
  case NI_KONTROL_D2_BTN_PAD_4:
    return "PAD 4";
  case NI_KONTROL_D2_BTN_PAD_5:
    return "PAD 5";
  case NI_KONTROL_D2_BTN_PAD_6:
    return "PAD 6";
  case NI_KONTROL_D2_BTN_PAD_7:
    return "PAD 7";
  case NI_KONTROL_D2_BTN_PAD_8:
    return "PAD 8";
  case NI_KONTROL_D2_BTN_FX_1:
    return "FX 1";
  case NI_KONTROL_D2_BTN_FX_2:
    return "FX 2";
  case NI_KONTROL_D2_BTN_FX_3:
    return "FX 3";
  case NI_KONTROL_D2_BTN_FX_4:
    return "FX 4";
  case NI_KONTROL_D2_BTN_FX_SELECT:
    return "FX SELECT";
  case NI_KONTROL_D2_BTN_BACK:
    return "BACK";
  case NI_KONTROL_D2_BTN_CAPTURE:
    return "CAPTURE";
  case NI_KONTROL_D2_BTN_EDIT:
    return "EDIT";
  case NI_KONTROL_D2_BTN_ON_1:
    return "ON 1";
  case NI_KONTROL_D2_BTN_ON_2:
    return "ON 2";
  case NI_KONTROL_D2_BTN_ON_3:
    return "ON 3";
  case NI_KONTROL_D2_BTN_ON_4:
    return "ON 4";
  default: {
    static char buf[32];
    snprintf(buf, sizeof(buf), "BTN %d", id);
    return buf;
  }
  }
}

const char *get_slider_name(int id) {
  switch (id) {
  case NI_KONTROL_D2_SLIDER_FADER_1:
    return "FADER 1";
  case NI_KONTROL_D2_SLIDER_FADER_2:
    return "FADER 2";
  case NI_KONTROL_D2_SLIDER_FADER_3:
    return "FADER 3";
  case NI_KONTROL_D2_SLIDER_FADER_4:
    return "FADER 4";
  case NI_KONTROL_D2_SLIDER_FX_DIAL_1:
    return "FX DIAL 1";
  case NI_KONTROL_D2_SLIDER_FX_DIAL_2:
    return "FX DIAL 2";
  case NI_KONTROL_D2_SLIDER_FX_DIAL_3:
    return "FX DIAL 3";
  case NI_KONTROL_D2_SLIDER_FX_DIAL_4:
    return "FX DIAL 4";
  case NI_KONTROL_D2_SLIDER_TOUCHSTRIP:
    return "TOUCHSTRIP";
  default: {
    static char buf[32];
    snprintf(buf, sizeof(buf), "SLIDER %d", id);
    return buf;
  }
  }
}

const char *get_encoder_name(int id) {
  switch (id) {
  case NI_KONTROL_D2_ENCODER_SCREEN_1:
    return "SCREEN ENC 1";
  case NI_KONTROL_D2_ENCODER_SCREEN_2:
    return "SCREEN ENC 2";
  case NI_KONTROL_D2_ENCODER_SCREEN_3:
    return "SCREEN ENC 3";
  case NI_KONTROL_D2_ENCODER_SCREEN_4:
    return "SCREEN ENC 4";
  case NI_KONTROL_D2_ENCODER_BROWSE:
    return "BROWSE";
  case NI_KONTROL_D2_ENCODER_LOOP:
    return "LOOP SIZE";
  default: {
    static char buf[32];
    snprintf(buf, sizeof(buf), "ENC %d", id);
    return buf;
  }
  }
}

void update_touchstrip_leds(struct ctlra_dev_t *dev) {
  uint8_t orange[25] = {0};
  uint8_t blue[25] = {0};

  if (touchstrip_active) {
    /* Calculate which LED to light based on position (0.0 to 1.0) */
    int led_pos = (int)(touchstrip_position * 24.0f);
    if (led_pos < 0)
      led_pos = 0;
    if (led_pos > 24)
      led_pos = 24;

    /* Light up the current position in blue at full brightness */
    blue[led_pos] = 0x7F; /* Max brightness */

    /* Light up neighbors for better visibility */
    if (led_pos > 0)
      blue[led_pos - 1] = 0x30;
    if (led_pos < 24)
      blue[led_pos + 1] = 0x30;
  }

  /* Send to touchstrip - this is the correct API call */
  ni_kontrol_d2_light_touchstrip(dev, orange, blue);
  ctlra_dev_light_flush(dev, 1);
}

void draw_control_display(struct ctlra_dev_t *dev) {
  if (surface == NULL) {
    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, WIDTH, HEIGHT);
    if (!surface)
      return;
    cr = cairo_create(surface);
    if (!cr)
      return;
  }

  cairo_set_source_rgb(cr, 0.05, 0.05, 0.1);
  cairo_rectangle(cr, 0, 0, WIDTH, HEIGHT);
  cairo_fill(cr);

  cairo_pattern_t *header_grad = cairo_pattern_create_linear(0, 0, 0, 40);
  cairo_pattern_add_color_stop_rgb(header_grad, 0.0, 0.1, 0.3, 0.5);
  cairo_pattern_add_color_stop_rgb(header_grad, 1.0, 0.05, 0.15, 0.25);
  cairo_set_source(cr, header_grad);
  cairo_rectangle(cr, 0, 0, WIDTH, 40);
  cairo_fill(cr);
  cairo_pattern_destroy(header_grad);

  cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 24);
  cairo_move_to(cr, 15, 28);
  cairo_show_text(cr, "D2 CONTROL MONITOR");

  cairo_set_font_size(cr, 12);
  cairo_set_source_rgb(cr, 0.4, 1.0, 0.4);
  cairo_move_to(cr, WIDTH - 180, 28);
  cairo_show_text(cr, "✓ LEDs + Touchstrip!");

  if (event_count > 0) {
    cairo_set_source_rgba(cr, 0.2, 0.4, 0.6, 0.4);
    cairo_rectangle(cr, 10, 50, WIDTH - 20, 60);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.3, 0.6, 1.0);
    cairo_set_line_width(cr, 2);
    cairo_rectangle(cr, 10, 50, WIDTH - 20, 60);
    cairo_stroke(cr);

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_set_font_size(cr, 20);
    cairo_move_to(cr, 20, 75);

    char current_text[128];
    snprintf(current_text, sizeof(current_text), "%s: %s",
             event_history[0].type, event_history[0].name);
    cairo_show_text(cr, current_text);

    cairo_set_source_rgb(cr, 0.4, 1.0, 0.4);
    cairo_set_font_size(cr, 18);
    cairo_move_to(cr, 20, 100);
    cairo_show_text(cr, event_history[0].value);
  }

  cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
  cairo_set_font_size(cr, 14);
  cairo_move_to(cr, 15, 130);
  cairo_show_text(cr, "Recent Events:");

  int y_offset = 150;
  for (int i = 0; i < event_count && i < 8; i++) {
    double alpha = 1.0 - (i * 0.1);
    cairo_set_source_rgba(cr, 0.7, 0.7, 0.7, alpha);

    cairo_set_font_size(cr, 11);
    char event_line[128];

    snprintf(event_line, sizeof(event_line), "%s: %s", event_history[i].type,
             event_history[i].name);
    cairo_move_to(cr, 20, y_offset);
    cairo_show_text(cr, event_line);

    cairo_set_source_rgba(cr, 0.4, 1.0, 0.4, alpha);
    cairo_move_to(cr, WIDTH - 120, y_offset);
    cairo_show_text(cr, event_history[i].value);

    y_offset += 15;
  }

  cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.3);
  cairo_set_font_size(cr, 10);
  cairo_move_to(cr, 15, HEIGHT - 10);
  cairo_show_text(cr, "Buttons light up | Touchstrip shows blue LED position");

  cairo_surface_flush(surface);

  int stride = cairo_image_surface_get_stride(surface);
  unsigned char *data = cairo_image_surface_get_data(surface);
  if (!data)
    return;

  uint8_t *pixels = ni_kontrol_d2_screen_get_pixels(dev);
  uint16_t *write_head = (uint16_t *)pixels;

  for (int j = 0; j < HEIGHT; j++) {
    for (int i = 0; i < WIDTH; i++) {
      uint8_t *p = &data[(j * stride) + (i * 4)];
      int idx = (j * WIDTH) + (i);
      pixel_convert_from_argb(p[2], p[1], p[0], (uint8_t *)&write_head[idx]);
    }
  }

  ni_kontrol_d2_screen_blit(dev);
}

void d2_feedback_func(struct ctlra_dev_t *dev, void *userdata) {
  static int frame_counter = 0;
  frame_counter++;

  /* Only update screen every 3rd frame to reduce interference with LED commands
   */
  if (frame_counter % 3 == 0) {
    draw_control_display(dev);
  }

  /* Always update touchstrip LEDs */
  update_touchstrip_leds(dev);
}

void d2_event_func(struct ctlra_dev_t *dev, uint32_t num_events,
                   struct ctlra_event_t **events, void *userdata) {
  for (uint32_t i = 0; i < num_events; i++) {
    struct ctlra_event_t *e = events[i];

    switch (e->type) {
    case CTLRA_EVENT_BUTTON: {
      const char *btn_name = get_button_name(e->button.id);
      char value[64];
      snprintf(value, sizeof(value), "%s",
               e->button.pressed ? "PRESSED" : "RELEASED");
      add_event("BUTTON", btn_name, value, e->button.id);

      int led_id = button_to_led(e->button.id);

      printf("  → BTN ID %d mapped to LED ID %d\n", e->button.id, led_id);

      if (led_id >= 0) {
        if (e->button.pressed) {
          /* For pads, use cyan color */
          if (is_pad_button(e->button.id)) {
            /* D2 uses BBGGRR format: Cyan = 0xFFFFFF00 (B=FF, G=FF, R=00) */
            printf("  → Setting PAD LED %d to CYAN (0xFFFFFF00)\n", led_id);
            ctlra_dev_light_set(dev, led_id, 0xFFFFFF00); /* Cyan */
          } else {
            printf("  → Setting LED %d to FULL (0xFFFFFFFF)\n", led_id);
            ctlra_dev_light_set(dev, led_id, UINT32_MAX); /* Full brightness */
          }
        } else {
          printf("  → Turning OFF LED %d\n", led_id);
          ctlra_dev_light_set(dev, led_id, 0);
        }
        printf("  → Flushing LEDs\n");
        ctlra_dev_light_flush(dev, 1);
      } else {
        printf("  → No LED mapping\n");
      }

      if (e->button.id == NI_KONTROL_D2_BTN_TOUCHSTRIP_TOUCH) {
        touchstrip_active = e->button.pressed;
        printf("Touchstrip %s\n", e->button.pressed ? "TOUCHED" : "RELEASED");
      }

      printf("Button: %s - %s\n", btn_name, value);
      break;
    }

    case CTLRA_EVENT_SLIDER: {
      const char *slider_name = get_slider_name(e->slider.id);
      char value[64];
      snprintf(value, sizeof(value), "%.1f%%", e->slider.value * 100.0);
      add_event("SLIDER", slider_name, value, e->slider.id);

      if (e->slider.id == NI_KONTROL_D2_SLIDER_TOUCHSTRIP) {
        touchstrip_position = e->slider.value;
      }

      printf("Slider: %s - %s\n", slider_name, value);
      break;
    }

    case CTLRA_EVENT_ENCODER: {
      const char *enc_name = get_encoder_name(e->encoder.id);
      char value[64];

      if (e->encoder.flags & CTLRA_EVENT_ENCODER_FLAG_INT) {
        snprintf(value, sizeof(value), "Delta: %+d", e->encoder.delta);
      } else if (e->encoder.flags & CTLRA_EVENT_ENCODER_FLAG_FLOAT) {
        snprintf(value, sizeof(value), "Delta: %+.2f", e->encoder.delta_float);
      } else {
        snprintf(value, sizeof(value), "Turned");
      }

      add_event("ENCODER", enc_name, value, e->encoder.id);
      printf("Encoder: %s - %s\n", enc_name, value);
      break;
    }

    default:
      break;
    }
  }
}

int accept_dev_func(struct ctlra_t *ctlra, const struct ctlra_dev_info_t *info,
                    struct ctlra_dev_t *dev, void *userdata) {
  if (info->vendor_id == 0x17cc && info->device_id == 0x1400) {
    printf("Accepting Kontrol D2: %s %s\n", info->vendor, info->device);

    ctlra_dev_set_event_func(dev, d2_event_func);
    ctlra_dev_set_feedback_func(dev, d2_feedback_func);
    ctlra_dev_set_callback_userdata(dev, userdata);

    return 1;
  }

  return 0;
}

void sighndlr(int signal) {
  done = 1;
  printf("\nExiting...\n");
}

int main(int argc, char **argv) {
  signal(SIGINT, sighndlr);

  printf("==============================================\n");
  printf("  D2 CONTROL MONITOR - FINAL VERSION\n");
  printf("  Complete LED + Touchstrip Feedback\n");
  printf("==============================================\n\n");

  struct ctlra_create_opts_t opts = {
      .screen_redraw_target_fps = 15, /* Lower FPS to reduce LED interference */
  };

  struct ctlra_t *ctlra = ctlra_create(&opts);
  if (!ctlra) {
    printf("Failed to create ctlra instance\n");
    return -1;
  }

  int num_devs = ctlra_probe(ctlra, accept_dev_func, NULL);
  printf("Connected devices: %d\n\n", num_devs);

  if (num_devs == 0) {
    printf("No Kontrol D2 found!\n");
    ctlra_exit(ctlra);
    return -1;
  }

  printf("Features:\n");
  printf("  ✓ All button LEDs light when pressed (correct mapping!)\n");
  printf("  ✓ Pads glow cyan when pressed\n");
  printf("  ✓ Touchstrip shows blue LED at finger position\n");
  printf("  ✓ Screen displays all events\n\n");
  printf("Press Ctrl+C to exit...\n\n");

  while (!done) {
    ctlra_idle_iter(ctlra);
    usleep(10 * 1000);
  }

  ctlra_exit(ctlra);
  if (cr)
    cairo_destroy(cr);
  if (surface)
    cairo_surface_destroy(surface);

  printf("Cleanup complete.\n");
  return 0;
}
