#define _DEFAULT_SOURCE
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ctlra.h"
#include "devices/ni_kontrol_d2.h"

/* Cairo for drawing graphics */
#include <cairo/cairo.h>

/* SDL2 for desktop visualization */
#include <SDL2/SDL.h>

#define WIDTH 480
#define HEIGHT 272

static cairo_surface_t *surface = NULL;
static cairo_t *cr = NULL;

SDL_Window *sdl_window = NULL;
SDL_Renderer *sdl_renderer = NULL;
SDL_Texture *sdl_texture = NULL;
static int frame_count = 0;
static volatile uint32_t done = 0;

/* Convert ARGB (Cairo) to BGR565 (D2 screen format) */
static inline void pixel_convert_from_argb(int r, int g, int b, uint8_t *data) {
  r = ((int)((r / 255.0) * 31)) & ((1 << 5) - 1);
  g = ((int)((g / 255.0) * 63)) & ((1 << 6) - 1);
  b = ((int)((b / 255.0) * 31)) & ((1 << 5) - 1);

  uint16_t combined = (b | g << 5 | r << 11);
  data[0] = combined >> 8;
  data[1] = combined & 0xff;
}

void draw_custom_graphics(struct ctlra_dev_t *dev) {
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
  }

  /* Clear background to dark grey */
  cairo_set_source_rgb(cr, 0.05, 0.05, 0.08);
  cairo_rectangle(cr, 0, 0, WIDTH, HEIGHT);
  cairo_fill(cr);

  /* --- Draw Top Header (Static Area) --- */
  /* Artwork placeholder box */
  cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
  cairo_rectangle(cr, 10, 10, 50, 50);
  cairo_fill(cr);
  
  /* Inner artwork graphic (static square) */
  cairo_set_source_rgb(cr, 0.8, 0.3, 0.1);
  cairo_rectangle(cr, 15, 15, 40, 40);
  cairo_fill(cr);

  /* Track Title */
  cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 22);
  cairo_move_to(cr, 75, 30);
  cairo_show_text(cr, "My Awesome Track (Original Mix)");

  /* Artist Name */
  cairo_set_source_rgb(cr, 0.6, 0.6, 0.6);
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 16);
  cairo_move_to(cr, 75, 52);
  cairo_show_text(cr, "DJ OpenAV & Ctlra");

  /* Time Remaining */
  int remaining_secs = 300 - (frame_count / 30); // Approx 30fps
  if(remaining_secs < 0) remaining_secs = 0;
  char time_text[16];
  snprintf(time_text, sizeof(time_text), "-%02d:%02d", remaining_secs / 60, remaining_secs % 60);
  
  cairo_set_source_rgb(cr, 0.1, 0.8, 0.2);
  cairo_set_font_size(cr, 24);
  cairo_text_extents_t extents;
  cairo_text_extents(cr, time_text, &extents);
  cairo_move_to(cr, WIDTH - extents.width - 15, 45);
  cairo_show_text(cr, time_text);
  
  /* Separator line */
  cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
  cairo_move_to(cr, 10, 70);
  cairo_line_to(cr, WIDTH - 10, 70);
  cairo_stroke(cr);

  /* --- Draw Scrolling Waveform (Dynamic Area) --- */
  /* The animated zone will be from Y=90 to Y=170 (Approx 30% of screen) */
  int wave_y_center = 130;
  int wave_height = 80;
  
  /* Waveform background */
  cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 1.0);
  cairo_rectangle(cr, 0, 90, WIDTH, wave_height);
  cairo_fill(cr);

  /* Playhead line */
  int playhead_x = 100;
  cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
  cairo_set_line_width(cr, 2);
  cairo_move_to(cr, playhead_x, 90);
  cairo_line_to(cr, playhead_x, 170);
  cairo_stroke(cr);

  /* Draw "audio" bars scrolling leftwards */
  cairo_set_line_width(cr, 3);
  double scroll_offset = (frame_count * 2) % 20; // Scroll speed
  
  for (int x = 0; x < WIDTH; x += 5) {
      double real_x = x - scroll_offset;
      if (real_x < 0) continue;
      
      /* Synthesize a pseudo-random looking audio wave based on position */
      double global_x = real_x + (frame_count * 2); 
      double amplitude = (sin(global_x * 0.05) * 0.5 + 0.5) * (wave_height / 2.0 * 0.8) + 5;
      
      /* Add some high frequency noise to make it look spiky */
      amplitude += fmod(global_x * 12.34, 10.0);
      
      if (real_x < playhead_x) {
          /* Played portion is darker blue */
          cairo_set_source_rgb(cr, 0.2, 0.5, 0.9);
      } else {
          /* Upcoming portion is lighter cyan */
          cairo_set_source_rgb(cr, 0.4, 0.8, 1.0);
      }
      
      cairo_move_to(cr, real_x, wave_y_center - amplitude);
      cairo_line_to(cr, real_x, wave_y_center + amplitude);
      cairo_stroke(cr);
  }

  /* --- Draw Bottom Deck Information (Static Area) --- */
  /* Separator line */
  cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
  cairo_move_to(cr, 10, 190);
  cairo_line_to(cr, WIDTH - 10, 190);
  cairo_stroke(cr);
  
  cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 14);

  cairo_move_to(cr, 15, 220);
  cairo_show_text(cr, "BPM: 124.00");

  cairo_move_to(cr, 15, 240);
  cairo_show_text(cr, "Key: 8A (Am)");
  
  /* Draw a static loop size indicator */
  cairo_set_source_rgb(cr, 0.2, 0.8, 0.2);
  cairo_rectangle(cr, WIDTH - 60, 210, 40, 30);
  cairo_fill(cr);
  
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  cairo_set_font_size(cr, 18);
  cairo_move_to(cr, WIDTH - 52, 232);
  cairo_show_text(cr, "16");

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
  
  static uint16_t prev_frame[HEIGHT][WIDTH] = {0};
  
  int min_y = HEIGHT, max_y = -1;
  int min_x = WIDTH, max_x = -1;

  for (int j = 0; j < HEIGHT; j++) {
    for (int i = 0; i < WIDTH; i++) {
      uint8_t *p = &data[(j * stride) + (i * 4)];
      int idx = (j * WIDTH) + (i);
      pixel_convert_from_argb(p[2], p[1], p[0], (uint8_t *)&write_head[idx]);
      
      if (write_head[idx] != prev_frame[j][i] || frame_count == 0 || (frame_count % 60) == 0) {
        if (j < min_y) min_y = j;
        if (j > max_y) max_y = j;
        if (i < min_x) min_x = i;
        if (i > max_x) max_x = i;
      }
      prev_frame[j][i] = write_head[idx];
    }
  }

  /* Update the SDL texture with the ARGB frame data */
  extern SDL_Texture *sdl_texture;
  extern SDL_Renderer *sdl_renderer;
  if(sdl_texture && sdl_renderer) {
      SDL_UpdateTexture(sdl_texture, NULL, data, stride);
      SDL_RenderClear(sdl_renderer);
      SDL_RenderCopy(sdl_renderer, sdl_texture, NULL, NULL);

      /* Draw a transparent red rectangle showing the bounding-box zone update */
      if(max_y >= min_y && max_x >= min_x && frame_count != 0 && (frame_count % 60) != 0) {
          SDL_Rect zone_rect = { min_x, min_y, max_x - min_x + 1, max_y - min_y + 1 };
          SDL_SetRenderDrawColor(sdl_renderer, 255, 0, 0, 255);
          SDL_RenderDrawRect(sdl_renderer, &zone_rect);
          
          /* Semi-transparent fill */
          SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);
          SDL_SetRenderDrawColor(sdl_renderer, 255, 0, 0, 64);
          SDL_RenderFillRect(sdl_renderer, &zone_rect);
      }
      
      SDL_RenderPresent(sdl_renderer);
  }

  /* Send to D2 screen */
  if (frame_count == 0 || (frame_count % 60) == 0) {
    ni_kontrol_d2_screen_blit(dev);
  } else if (max_y >= min_y && max_x >= min_x) {
    /* Send exactly the bouncing box of updated pixels */
    int w = max_x - min_x + 1;
    int h = max_y - min_y + 1;
    ni_kontrol_d2_screen_blit_zone(dev, min_x, min_y, w, h);
  }

  frame_count++;
}

void d2_feedback_func(struct ctlra_dev_t *dev, void *userdata) {
  /* This is called to update LEDs and screen */
  draw_custom_graphics(dev);
}

void d2_event_func(struct ctlra_dev_t *dev, uint32_t num_events,
                   struct ctlra_event_t **events, void *userdata) {
  /* Handle button presses, encoders, etc. */
  for (uint32_t i = 0; i < num_events; i++) {
    struct ctlra_event_t *e = events[i];

    switch (e->type) {
    case CTLRA_EVENT_BUTTON:
      if (e->button.pressed) {
        printf("Button %d pressed\n", e->button.id);
        /* Light up the button */
        ctlra_dev_light_set(dev, e->button.id, UINT32_MAX);
      } else {
        ctlra_dev_light_set(dev, e->button.id, 0);
      }
      ctlra_dev_light_flush(dev, 1);
      break;

    case CTLRA_EVENT_ENCODER:
      if (e->encoder.flags & CTLRA_EVENT_ENCODER_FLAG_INT) {
        printf("Encoder %d delta: %d\n", e->encoder.id, e->encoder.delta);
      }
      break;

    case CTLRA_EVENT_SLIDER:
      printf("Slider %d value: %f\n", e->slider.id, e->slider.value);
      break;

    default:
      break;
    }
  }
}

int accept_dev_func(struct ctlra_t *ctlra, const struct ctlra_dev_info_t *info,
                    struct ctlra_dev_t *dev, void *userdata) {
  /* Only accept Kontrol D2 */
  if (info->vendor_id == 0x17cc && info->device_id == 0x1400) {
    printf("Accepting Kontrol D2: %s %s\n", info->vendor, info->device);

    /* Set callback functions */
    ctlra_dev_set_event_func(dev, d2_event_func);
    ctlra_dev_set_feedback_func(dev, d2_feedback_func);
    ctlra_dev_set_callback_userdata(dev, userdata);

    return 1; /* Accept */
  }

  return 0; /* Reject */
}

void sighndlr(int signal) {
  done = 1;
  printf("\nExiting...\n");
}

int main(int argc, char **argv) {
  signal(SIGINT, sighndlr);

  printf("==============================================\n");
  printf("  D2 Custom Graphics Demo\n");
  printf("  Using openAV-Ctlra library\n");
  printf("==============================================\n\n");

  /* Create ctlra instance with screen refresh rate */
  struct ctlra_create_opts_t opts = {
      .screen_redraw_target_fps = 30,
  };

  struct ctlra_t *ctlra = ctlra_create(&opts);
  if (!ctlra) {
    printf("Failed to create ctlra instance\n");
    return -1;
  }

  /* Probe for devices */
  int num_devs = ctlra_probe(ctlra, accept_dev_func, NULL);
  printf("Connected devices: %d\n\n", num_devs);

  if (num_devs == 0) {
    printf("No Kontrol D2 found!\n");
    printf("Make sure the D2 is connected via USB.\n");
    ctlra_exit(ctlra);
    return -1;
  }

  printf("Displaying custom graphics on D2 screen!\n");
  printf("Press Ctrl+C to exit...\n\n");

  /* Initialize SDL2 for Desktop previewing */
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
      printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
  } else {
      sdl_window = SDL_CreateWindow("D2 Screen Partial Updates Visualizer",
                                    SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                    WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
      if (sdl_window) {
          sdl_renderer = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_ACCELERATED);
          if (sdl_renderer) {
              sdl_texture = SDL_CreateTexture(sdl_renderer,
                                              SDL_PIXELFORMAT_ARGB8888,
                                              SDL_TEXTUREACCESS_STREAMING,
                                              WIDTH, HEIGHT);
          }
      }
  }

  /* Main loop */
  while (!done) {
    ctlra_idle_iter(ctlra);
    usleep(10 * 1000); /* 10ms sleep */
    
    /* Process SDL events to keep window responsive */
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
            done = 1;
        }
    }
  }

  /* Cleanup */
  if (sdl_texture) SDL_DestroyTexture(sdl_texture);
  if (sdl_renderer) SDL_DestroyRenderer(sdl_renderer);
  if (sdl_window) SDL_DestroyWindow(sdl_window);
  SDL_Quit();

  ctlra_exit(ctlra);
  if (cr)
    cairo_destroy(cr);
  if (surface)
    cairo_surface_destroy(surface);

  printf("Cleanup complete. Goodbye!\n");
  return 0;
}
