#define _DEFAULT_SOURCE
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ctlra.h"
#include "devices/ni_maschine_mk3.h"

/* Cairo for drawing graphics */
#include <cairo/cairo.h>

#define WIDTH 480
#define HEIGHT 272

static cairo_surface_t *surface_left = NULL;
static cairo_t *cr_left = NULL;

static cairo_surface_t *surface_right = NULL;
static cairo_t *cr_right = NULL;

static int frame_count = 0;
static volatile uint32_t done = 0;
static uint8_t active_pad = 0;
static float browse_encoder_val = 0.5f;

/* Convert ARGB (Cairo) to BGR565 (Device screen format) */
static inline void pixel_convert_from_argb(int r, int g, int b, uint8_t *data) {
  r = ((int)((r / 255.0) * 31)) & ((1 << 5) - 1);
  g = ((int)((g / 255.0) * 63)) & ((1 << 6) - 1);
  b = ((int)((b / 255.0) * 31)) & ((1 << 5) - 1);

  uint16_t combined = (b | g << 5 | r << 11);
  data[0] = combined >> 8;
  data[1] = combined & 0xff;
}

void draw_screen(struct ctlra_dev_t *dev, uint8_t screen_idx, cairo_t *cr, cairo_surface_t *surface) {
  /* Clear background based on screen side */
  cairo_set_source_rgb(cr, screen_idx == 0 ? 0.1 : 0.05, 0.1, 0.15);
  cairo_rectangle(cr, 0, 0, WIDTH, HEIGHT);
  cairo_fill(cr);

  if (screen_idx == 0) {
    /* --- Left Screen: Information & Playback --- */
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 24);
    cairo_move_to(cr, 20, 40);
    cairo_show_text(cr, "MASCHINE MK3 - LEFT SCREEN");

    /* Draw bouncing ball */
    double x = 240 + sin(frame_count * 0.05) * 150;
    double y = 136 + cos(frame_count * 0.07) * 80;
    cairo_set_source_rgb(cr, 0.2, 0.8, 0.3);
    cairo_arc(cr, x, y, 30 + active_pad * 2, 0, 2 * M_PI);
    cairo_fill(cr);

    /* Display Encoder Value */
    char enc_text[32];
    snprintf(enc_text, sizeof(enc_text), "Browse Val: %.2f", browse_encoder_val);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_move_to(cr, 20, 240);
    cairo_show_text(cr, enc_text);

  } else {
    /* --- Right Screen: Waveform or visualization --- */
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 24);
    cairo_move_to(cr, 20, 40);
    cairo_show_text(cr, "MASCHINE MK3 - RIGHT SCREEN");

    /* Draw scrolling wave */
    cairo_set_line_width(cr, 3);
    double scroll_offset = (frame_count * 3) % 20;

    for (int px = 0; px < WIDTH; px += 8) {
        double real_x = px - scroll_offset;
        if (real_x < 0) continue;
        
        double amplitude = (sin((real_x + frame_count * 4) * 0.03) * 0.5 + 0.5) * 60 + 10;
        
        if (px % 16 == 0) cairo_set_source_rgb(cr, 0.3, 0.6, 1.0);
        else cairo_set_source_rgb(cr, 0.2, 0.4, 0.8);
        
        cairo_move_to(cr, real_x, 136 - amplitude);
        cairo_line_to(cr, real_x, 136 + amplitude);
        cairo_stroke(cr);
    }
  }

  cairo_surface_flush(surface);

  /* Convert Cairo ARGB to Mk3's BGR565 format */
  int stride = cairo_image_surface_get_stride(surface);
  unsigned char *data = cairo_image_surface_get_data(surface);
  if (!data) return;

  uint8_t *pixels = ni_maschine_mk3_screen_get_pixels(dev, screen_idx);
  uint16_t *write_head = (uint16_t *)pixels;
  
  /* We use static storage here based on screen_idx for prev buffer calculation */
  static uint16_t prev_frame[2][HEIGHT][WIDTH] = {0};
  
  int min_y = HEIGHT, max_y = -1;
  int min_x = WIDTH, max_x = -1;

  for (int j = 0; j < HEIGHT; j++) {
    for (int i = 0; i < WIDTH; i++) {
      uint8_t *p = &data[(j * stride) + (i * 4)];
      int idx = (j * WIDTH) + (i);
      
      uint16_t current_val;
      pixel_convert_from_argb(p[2], p[1], p[0], (uint8_t *)&current_val);
      write_head[idx] = current_val;
      
      /* Only update subregions that changed */
      if (current_val != prev_frame[screen_idx][j][i] || frame_count == 0 || (frame_count % 60) == 0) {
        if (j < min_y) min_y = j;
        if (j > max_y) max_y = j;
        if (i < min_x) min_x = i;
        if (i > max_x) max_x = i;
      }
      prev_frame[screen_idx][j][i] = current_val;
    }
  }

  /* Send exactly the bounded box of updated pixels */
  if (frame_count == 0 || (frame_count % 60) == 0) {
      /* Once a second we push a full frame refresh to avoid protocol artifacts */
      ni_maschine_mk3_screen_blit_zone(dev, screen_idx, 0, 0, WIDTH, HEIGHT);
  } else if (max_y >= min_y && max_x >= min_x) {
      int w = max_x - min_x + 1;
      int h = max_y - min_y + 1;
      ni_maschine_mk3_screen_blit_zone(dev, screen_idx, min_x, min_y, w, h);
  }
}

void mk3_feedback_func(struct ctlra_dev_t *dev, void *userdata) {
  /* Update Left and Right screens */
  draw_screen(dev, 0, cr_left, surface_left);
  draw_screen(dev, 1, cr_right, surface_right);
  frame_count++;
}

void mk3_event_func(struct ctlra_dev_t *dev, uint32_t num_events,
                   struct ctlra_event_t **events, void *userdata) {

  for (uint32_t i = 0; i < num_events; i++) {
    struct ctlra_event_t *e = events[i];

    switch (e->type) {
    case CTLRA_EVENT_BUTTON:
      if (e->button.pressed) {
        printf("Button %d pressed\n", e->button.id);
        
        if (e->button.id == NI_MASCHINE_MK3_BTN_PLAY) {
            ctlra_dev_light_set(dev, e->button.id, 0xFF00FF00); // Green
        } else {
            ctlra_dev_light_set(dev, e->button.id, UINT32_MAX); // White
        }
      } else {
        ctlra_dev_light_set(dev, e->button.id, 0);
      }
      ctlra_dev_light_flush(dev, 1);
      break;

    case CTLRA_EVENT_ENCODER:
      if (e->encoder.flags & CTLRA_EVENT_ENCODER_FLAG_INT) {
        printf("Encoder %d delta: %d\n", e->encoder.id, e->encoder.delta);
        browse_encoder_val += (e->encoder.delta * 0.05f);
      } else if (e->encoder.flags & CTLRA_EVENT_ENCODER_FLAG_FLOAT) {
        printf("Float Encoder %d delta: %f\n", e->encoder.id, e->encoder.delta_float);
      }
      break;

    case CTLRA_EVENT_GRID:
      if (e->grid.flags & CTLRA_EVENT_GRID_FLAG_BUTTON) {
          if (e->grid.pressed) {
              active_pad = e->grid.pos;
              printf("Pad %d hit with pressure %f\n", e->grid.pos, e->grid.pressure);
          }
      }
      break;

    case CTLRA_EVENT_SLIDER:
      printf("Touchstrip: %f\n", e->slider.value);
      break;

    default:
      break;
    }
  }
}

int accept_dev_func(struct ctlra_t *ctlra, const struct ctlra_dev_info_t *info,
                    struct ctlra_dev_t *dev, void *userdata) {
  /* Accept Maschine Mk3 */
  if (info->vendor_id == 0x17cc && info->device_id == 0x1600) {
    printf("Accepting Maschine Mk3 (Vendor: %s, Device: %s)\n", info->vendor, info->device);

    ctlra_dev_set_event_func(dev, mk3_event_func);
    ctlra_dev_set_feedback_func(dev, mk3_feedback_func);

    return 1;
  }
  return 0;
}

void sighndlr(int signal) {
  done = 1;
}

int main(int argc, char **argv) {
  signal(SIGINT, sighndlr);

  surface_left = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, WIDTH, HEIGHT);
  cr_left = cairo_create(surface_left);
  surface_right = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, WIDTH, HEIGHT);
  cr_right = cairo_create(surface_right);

  struct ctlra_create_opts_t opts = {
      .screen_redraw_target_fps = 30,
  };

  struct ctlra_t *ctlra = ctlra_create(&opts);
  if (!ctlra) {
    printf("Failed to create ctlra instance\n");
    return -1;
  }

  int num_devs = ctlra_probe(ctlra, accept_dev_func, NULL);
  if (num_devs == 0) {
    printf("No Maschine Mk3 found. Are you sure it's plugged in via USB?\n");
    ctlra_exit(ctlra);
    return -1;
  }

  printf("Displaying custom graphics on Maschine Mk3 screens! Press Ctrl+C to exit...\n");

  /* Main loop */
  while (!done) {
    ctlra_idle_iter(ctlra);
    usleep(10 * 1000); /* 10ms frame padding limit */
  }

  ctlra_exit(ctlra);
  cairo_destroy(cr_left);
  cairo_surface_destroy(surface_left);
  cairo_destroy(cr_right);
  cairo_surface_destroy(surface_right);
  return 0;
}
