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

/* Cairo for drawing graphics */
#include <cairo/cairo.h>

#define WIDTH 480
#define HEIGHT 272
#define MAX_PARTICLES 100
#define NUM_WAVEFORM_POINTS 240

static cairo_surface_t *surface = NULL;
static cairo_t *cr = NULL;
static int frame_count = 0;
static volatile uint32_t done = 0;

/* Particle system */
typedef struct {
  double x, y;
  double vx, vy;
  double life;
  double size;
  double hue;
} Particle;

static Particle particles[MAX_PARTICLES];

/* Simulated waveform data */
static double waveform[NUM_WAVEFORM_POINTS];
static double spectrum[64];

/* DJ controls state */
static double deck_speed = 1.0;
static double crossfader = 0.5;
static int is_playing = 1;
static double volume = 0.75;

/* Initialize particle system */
void init_particles() {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    particles[i].x = WIDTH / 2;
    particles[i].y = HEIGHT - 50;
    particles[i].vx = ((rand() % 200) - 100) / 30.0;
    particles[i].vy = -((rand() % 100) + 50) / 30.0;
    particles[i].life = 1.0;
    particles[i].size = (rand() % 5) + 2;
    particles[i].hue = rand() % 360;
  }
}

/* Update particles */
void update_particles() {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    particles[i].x += particles[i].vx;
    particles[i].y += particles[i].vy;
    particles[i].vy += 0.15; // gravity
    particles[i].life -= 0.01;

    if (particles[i].life <= 0) {
      // Respawn particle
      particles[i].x = WIDTH / 2 + ((rand() % 100) - 50);
      particles[i].y = HEIGHT - 50;
      particles[i].vx = ((rand() % 200) - 100) / 30.0;
      particles[i].vy = -((rand() % 100) + 50) / 30.0;
      particles[i].life = 1.0;
      particles[i].hue = rand() % 360;
    }
  }
}

/* Generate simulated waveform */
void generate_waveform() {
  for (int i = 0; i < NUM_WAVEFORM_POINTS; i++) {
    double t = i / 10.0 + frame_count * 0.05;
    waveform[i] = sin(t * 0.5) * 0.3 + sin(t * 1.3) * 0.2 + sin(t * 2.7) * 0.15;
  }
}

/* Generate simulated spectrum */
void generate_spectrum() {
  for (int i = 0; i < 64; i++) {
    double t = frame_count * 0.05;
    spectrum[i] = fabs(sin((t + i * 0.1) * 0.5) * cos((t + i * 0.05) * 0.3)) *
                  (1.0 - i / 80.0);
  }
}

/* HSV to RGB conversion */
void hsv_to_rgb(double h, double s, double v, double *r, double *g, double *b) {
  double c = v * s;
  double x = c * (1 - fabs(fmod(h / 60.0, 2) - 1));
  double m = v - c;

  double r1, g1, b1;
  if (h < 60) {
    r1 = c;
    g1 = x;
    b1 = 0;
  } else if (h < 120) {
    r1 = x;
    g1 = c;
    b1 = 0;
  } else if (h < 180) {
    r1 = 0;
    g1 = c;
    b1 = x;
  } else if (h < 240) {
    r1 = 0;
    g1 = x;
    b1 = c;
  } else if (h < 300) {
    r1 = x;
    g1 = 0;
    b1 = c;
  } else {
    r1 = c;
    g1 = 0;
    b1 = x;
  }

  *r = r1 + m;
  *g = g1 + m;
  *b = b1 + m;
}

/* Convert ARGB (Cairo) to BGR565 (D2 screen format) */
static inline void pixel_convert_from_argb(int r, int g, int b, uint8_t *data) {
  r = ((int)((r / 255.0) * 31)) & ((1 << 5) - 1);
  g = ((int)((g / 255.0) * 63)) & ((1 << 6) - 1);
  b = ((int)((b / 255.0) * 31)) & ((1 << 5) - 1);

  uint16_t combined = (b | g << 5 | r << 11);
  data[0] = combined >> 8;
  data[1] = combined & 0xff;
}

/* Draw DJ-themed waveform display */
void draw_waveform(cairo_t *cr, int y_offset) {
  // Background for waveform
  cairo_pattern_t *bg =
      cairo_pattern_create_linear(0, y_offset, 0, y_offset + 80);
  cairo_pattern_add_color_stop_rgba(bg, 0.0, 0.0, 0.0, 0.0, 0.8);
  cairo_pattern_add_color_stop_rgba(bg, 1.0, 0.0, 0.1, 0.2, 0.8);
  cairo_set_source(cr, bg);
  cairo_rectangle(cr, 10, y_offset, WIDTH - 20, 80);
  cairo_fill(cr);
  cairo_pattern_destroy(bg);

  // Draw waveform
  cairo_set_line_width(cr, 1.5);

  // Gradient for waveform
  cairo_pattern_t *wave_grad =
      cairo_pattern_create_linear(0, y_offset, 0, y_offset + 80);
  cairo_pattern_add_color_stop_rgb(wave_grad, 0.0, 0.0, 1.0, 1.0);
  cairo_pattern_add_color_stop_rgb(wave_grad, 0.5, 0.0, 0.8, 1.0);
  cairo_pattern_add_color_stop_rgb(wave_grad, 1.0, 0.0, 0.5, 1.0);
  cairo_set_source(cr, wave_grad);

  cairo_move_to(cr, 10, y_offset + 40);
  for (int i = 0; i < NUM_WAVEFORM_POINTS; i++) {
    double x = 10 + (i * (WIDTH - 20) / (double)NUM_WAVEFORM_POINTS);
    double y = y_offset + 40 + waveform[i] * 35;
    cairo_line_to(cr, x, y);
  }
  cairo_stroke(cr);
  cairo_pattern_destroy(wave_grad);

  // Playhead
  double playhead_x = 10 + (frame_count % NUM_WAVEFORM_POINTS) * (WIDTH - 20) /
                               (double)NUM_WAVEFORM_POINTS;
  cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.8);
  cairo_set_line_width(cr, 2);
  cairo_move_to(cr, playhead_x, y_offset);
  cairo_line_to(cr, playhead_x, y_offset + 80);
  cairo_stroke(cr);
}

/* Draw spectrum analyzer */
void draw_spectrum(cairo_t *cr, int x_offset, int y_offset, int width,
                   int height) {
  double bar_width = width / 64.0;

  for (int i = 0; i < 64; i++) {
    double bar_height = spectrum[i] * height;
    double x = x_offset + i * bar_width;
    double y = y_offset + height - bar_height;

    // Color based on frequency
    double r, g, b;
    hsv_to_rgb(i * 5.6, 0.8, 1.0, &r, &g, &b);

    // Gradient for each bar
    cairo_pattern_t *bar_grad =
        cairo_pattern_create_linear(x, y + bar_height, x, y);
    cairo_pattern_add_color_stop_rgb(bar_grad, 0.0, r * 0.3, g * 0.3, b * 0.3);
    cairo_pattern_add_color_stop_rgb(bar_grad, 1.0, r, g, b);
    cairo_set_source(cr, bar_grad);

    cairo_rectangle(cr, x, y, bar_width - 1, bar_height);
    cairo_fill(cr);
    cairo_pattern_destroy(bar_grad);
  }
}

/* Draw VU meter */
void draw_vu_meter(cairo_t *cr, int x, int y, int width, int height,
                   double level) {
  // Background
  cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
  cairo_rectangle(cr, x, y, width, height);
  cairo_fill(cr);

  // Level indicator
  double fill_width = width * level;
  cairo_pattern_t *vu_grad = cairo_pattern_create_linear(x, y, x + width, y);
  cairo_pattern_add_color_stop_rgb(vu_grad, 0.0, 0.0, 1.0, 0.0);
  cairo_pattern_add_color_stop_rgb(vu_grad, 0.7, 1.0, 1.0, 0.0);
  cairo_pattern_add_color_stop_rgb(vu_grad, 1.0, 1.0, 0.0, 0.0);
  cairo_set_source(cr, vu_grad);
  cairo_rectangle(cr, x, y, fill_width, height);
  cairo_fill(cr);
  cairo_pattern_destroy(vu_grad);

  // Border
  cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
  cairo_set_line_width(cr, 1);
  cairo_rectangle(cr, x, y, width, height);
  cairo_stroke(cr);
}

void draw_intensive_graphics(struct ctlra_dev_t *dev) {
  if (surface == NULL) {
    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, WIDTH, HEIGHT);
    if (!surface) {
      printf("Failed to create Cairo surface!\n");
      return;
    }
    cr = cairo_create(surface);
    if (!cr) {
      printf("Failed to create Cairo context!\n");
      return;
    }
    init_particles();
    srand(time(NULL));
  }

  /* Generate audio data */
  generate_waveform();
  generate_spectrum();
  update_particles();

  /* Animated gradient background */
  double bg_hue = fmod(frame_count * 0.5, 360.0);
  double r1, g1, b1, r2, g2, b2;
  hsv_to_rgb(bg_hue, 0.6, 0.15, &r1, &g1, &b1);
  hsv_to_rgb(fmod(bg_hue + 60, 360.0), 0.6, 0.1, &r2, &g2, &b2);

  cairo_pattern_t *gradient = cairo_pattern_create_radial(
      WIDTH / 2, HEIGHT / 2, 0, WIDTH / 2, HEIGHT / 2, WIDTH);
  cairo_pattern_add_color_stop_rgb(gradient, 0.0, r1, g1, b1);
  cairo_pattern_add_color_stop_rgb(gradient, 1.0, r2, g2, b2);
  cairo_set_source(cr, gradient);
  cairo_rectangle(cr, 0, 0, WIDTH, HEIGHT);
  cairo_fill(cr);
  cairo_pattern_destroy(gradient);

  /* Draw top info bar */
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.7);
  cairo_rectangle(cr, 0, 0, WIDTH, 35);
  cairo_fill(cr);

  /* Title */
  cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 20);
  cairo_move_to(cr, 10, 24);
  cairo_show_text(cr, "DJ MODE - INTENSIVE DEMO");

  /* FPS counter */
  char fps_text[32];
  snprintf(fps_text, sizeof(fps_text), "30 FPS");
  cairo_set_font_size(cr, 14);
  cairo_move_to(cr, WIDTH - 70, 22);
  cairo_show_text(cr, fps_text);

  /* Draw waveform display */
  draw_waveform(cr, 45);

  /* Draw dual spectrum analyzers */
  draw_spectrum(cr, 10, 135, (WIDTH - 30) / 2, 60);
  draw_spectrum(cr, WIDTH / 2 + 5, 135, (WIDTH - 30) / 2, 60);

  /* Draw VU meters */
  double vu_level = 0.5 + 0.4 * sin(frame_count * 0.1);
  draw_vu_meter(cr, 10, 205, (WIDTH - 30) / 2, 15, vu_level * volume);
  draw_vu_meter(cr, WIDTH / 2 + 5, 205, (WIDTH - 30) / 2, 15,
                vu_level * volume * 0.8);

  /* Draw particle effects */
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (particles[i].life > 0) {
      double r, g, b;
      hsv_to_rgb(particles[i].hue, 1.0, 1.0, &r, &g, &b);

      cairo_pattern_t *particle_grad = cairo_pattern_create_radial(
          particles[i].x, particles[i].y, 0, particles[i].x, particles[i].y,
          particles[i].size);
      cairo_pattern_add_color_stop_rgba(particle_grad, 0.0, r, g, b,
                                        particles[i].life);
      cairo_pattern_add_color_stop_rgba(particle_grad, 1.0, r, g, b, 0);
      cairo_set_source(cr, particle_grad);
      cairo_arc(cr, particles[i].x, particles[i].y, particles[i].size, 0,
                2 * M_PI);
      cairo_fill(cr);
      cairo_pattern_destroy(particle_grad);
    }
  }

  /* Status indicators */
  cairo_set_font_size(cr, 12);
  cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
  char status[64];
  snprintf(status, sizeof(status), "PLAYING - %.1fx - VOL: %.0f%%", deck_speed,
           volume * 100);
  cairo_move_to(cr, 10, 240);
  cairo_show_text(cr, status);

  /* Playing indicator */
  if (is_playing && (frame_count % 30) < 15) {
    cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
    cairo_arc(cr, 180, 235, 4, 0, 2 * M_PI);
    cairo_fill(cr);
  }

  /* Crossfader visualization */
  cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.3);
  cairo_rectangle(cr, WIDTH - 100, 230, 90, 6);
  cairo_fill(cr);

  cairo_set_source_rgb(cr, 0.0, 1.0, 1.0);
  double cf_x = WIDTH - 100 + crossfader * 90 - 3;
  cairo_rectangle(cr, cf_x, 228, 6, 10);
  cairo_fill(cr);

  cairo_surface_flush(surface);

  /* Convert Cairo ARGB to D2's BGR565 format */
  int stride = cairo_image_surface_get_stride(surface);
  unsigned char *data = cairo_image_surface_get_data(surface);

  if (!data) {
    printf("Error: cairo data == NULL\n");
    return;
  }

  uint8_t *pixels = ni_kontrol_d2_screen_get_pixels(dev);
  uint16_t *write_head = (uint16_t *)pixels;

  for (int j = 0; j < HEIGHT; j++) {
    for (int i = 0; i < WIDTH; i++) {
      uint8_t *p = &data[(j * stride) + (i * 4)];
      int idx = (j * WIDTH) + (i);
      pixel_convert_from_argb(p[2], p[1], p[0], (uint8_t *)&write_head[idx]);
    }
  }

  /* Send to D2 screen */
  ni_kontrol_d2_screen_blit(dev);

  frame_count++;
}

void d2_feedback_func(struct ctlra_dev_t *dev, void *userdata) {
  draw_intensive_graphics(dev);
}

void d2_event_func(struct ctlra_dev_t *dev, uint32_t num_events,
                   struct ctlra_event_t **events, void *userdata) {
  for (uint32_t i = 0; i < num_events; i++) {
    struct ctlra_event_t *e = events[i];

    switch (e->type) {
    case CTLRA_EVENT_BUTTON:
      printf("Button %d: %s\n", e->button.id,
             e->button.pressed ? "pressed" : "released");

      if (e->button.pressed) {
        // Play/Pause on PLAY button
        if (e->button.id == NI_KONTROL_D2_BTN_PLAY) {
          is_playing = !is_playing;
        }

        ctlra_dev_light_set(dev, e->button.id, UINT32_MAX);
      } else {
        ctlra_dev_light_set(dev, e->button.id, 0);
      }
      ctlra_dev_light_flush(dev, 1);
      break;

    case CTLRA_EVENT_SLIDER:
      printf("Slider %d: %f\n", e->slider.id, e->slider.value);
      if (e->slider.id == NI_KONTROL_D2_SLIDER_FX_DIAL_1) {
        volume = e->slider.value;
      }
      break;

    case CTLRA_EVENT_ENCODER:
      if (e->encoder.flags & CTLRA_EVENT_ENCODER_FLAG_INT) {
        printf("Encoder %d delta: %d\n", e->encoder.id, e->encoder.delta);
        if (e->encoder.id == NI_KONTROL_D2_ENCODER_LOOP) {
          deck_speed += e->encoder.delta * 0.05;
          if (deck_speed < 0.5)
            deck_speed = 0.5;
          if (deck_speed > 2.0)
            deck_speed = 2.0;
        }
      }
      break;

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
  printf("  D2 INTENSIVE GRAPHICS DEMO\n");
  printf("  DJ Mode with Particles & Visualizers\n");
  printf("==============================================\n\n");

  struct ctlra_create_opts_t opts = {
      .screen_redraw_target_fps = 30,
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

  printf("Controls:\n");
  printf("  - PLAY button: Toggle play/pause\n");
  printf("  - LOOP encoder: Change deck speed\n");
  printf("  - FX DIAL 1: Control volume\n");
  printf("  - Any button: Lights up the LED\n\n");
  printf("Running at 30 FPS with:\n");
  printf("  - Dual waveform display\n");
  printf("  - Dual spectrum analyzers\n");
  printf("  - Particle effects (100 particles)\n");
  printf("  - VU meters\n");
  printf("  - Animated gradients\n\n");
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
