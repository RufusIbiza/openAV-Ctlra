#define _DEFAULT_SOURCE
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "ctlra.h"
#include "devices/ni_maschine_mk3.h"

#include <cairo/cairo.h>

#define WIDTH  480
#define HEIGHT 272
#define FPS    30

static cairo_surface_t *surface[2];
static cairo_t         *cr[2];

static int   frame_count = 0;
static volatile uint32_t done = 0;
static float browse_val  = 0.5f;
static int   active_pad  = -1;

/* Convert Cairo ARGB → Mk3 BGR565 (big-endian) */
static inline uint16_t argb_to_bgr565(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t ri = ((int)(r / 255.0f * 31)) & 0x1f;
    uint16_t gi = ((int)(g / 255.0f * 63)) & 0x3f;
    uint16_t bi = ((int)(b / 255.0f * 31)) & 0x1f;
    uint16_t pix = bi | (gi << 5) | (ri << 11);
    return (uint16_t)((pix >> 8) | (pix << 8));
}

static uint16_t prev_frame[2][HEIGHT][WIDTH];

static void push_screen(struct ctlra_dev_t *dev, uint8_t scr_idx)
{
    cairo_surface_flush(surface[scr_idx]);
    int stride = cairo_image_surface_get_stride(surface[scr_idx]);
    unsigned char *data = cairo_image_surface_get_data(surface[scr_idx]);
    uint16_t *pix = (uint16_t *)ni_maschine_mk3_screen_get_pixels(dev, scr_idx);

    int min_y = HEIGHT, max_y = -1;
    int min_x = WIDTH,  max_x = -1;

    for (int j = 0; j < HEIGHT; j++) {
        for (int i = 0; i < WIDTH; i++) {
            uint8_t *p = &data[j * stride + i * 4];
            uint16_t val = argb_to_bgr565(p[2], p[1], p[0]);
            pix[j * WIDTH + i] = val;

            if (val != prev_frame[scr_idx][j][i] ||
                frame_count == 0 || (frame_count % 60) == 0) {
                if (j < min_y) min_y = j;
                if (j > max_y) max_y = j;
                if (i < min_x) min_x = i;
                if (i > max_x) max_x = i;
            }
            prev_frame[scr_idx][j][i] = val;
        }
    }

    if (frame_count == 0 || (frame_count % 60) == 0) {
        /* Periodic keyframe: full blit to resync */
        ni_maschine_mk3_screen_blit(dev, scr_idx);
    } else if (max_y >= min_y && max_x >= min_x) {
        /* Partial update: only the dirty bounding box */
        int w = max_x - min_x + 1;
        int h = max_y - min_y + 1;
        ni_maschine_mk3_screen_blit_zone(dev, scr_idx, min_x, min_y, w, h);
    }
    /* else: no pixels changed, skip the USB transfer entirely */
}

static void draw_left(void)
{
    cairo_t *c = cr[0];
    cairo_set_source_rgb(c, 0.05, 0.05, 0.12); cairo_paint(c);

    cairo_set_source_rgb(c, 0.8, 0.8, 0.8);
    cairo_select_font_face(c, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(c, 20);
    cairo_move_to(c, 12, 30); cairo_show_text(c, "Maschine Mk3 – Left Screen");

    cairo_set_source_rgb(c, 0.3, 0.3, 0.3);
    cairo_move_to(c, 0, 40); cairo_line_to(c, WIDTH, 40);
    cairo_set_line_width(c, 1); cairo_stroke(c);

    double t  = frame_count * 0.05;
    double cx = WIDTH  / 2.0 + cos(t)        * 160;
    double cy = HEIGHT / 2.0 + sin(t * 1.17) * 80;

    cairo_pattern_t *glow = cairo_pattern_create_radial(cx, cy, 0, cx, cy, 48);
    cairo_pattern_add_color_stop_rgba(glow, 0.0, 0.1, 0.9, 0.3, 0.6);
    cairo_pattern_add_color_stop_rgba(glow, 1.0, 0.0, 0.0, 0.0, 0.0);
    cairo_set_source(c, glow);
    cairo_arc(c, cx, cy, 48, 0, 2 * M_PI); cairo_fill(c);
    cairo_pattern_destroy(glow);

    cairo_set_source_rgb(c, 0.1, 0.9, 0.35);
    cairo_arc(c, cx, cy, 24, 0, 2 * M_PI); cairo_fill(c);

    cairo_set_source_rgb(c, 0.2, 0.2, 0.2);
    cairo_rectangle(c, 12, HEIGHT - 36, WIDTH - 24, 14); cairo_fill(c);
    cairo_set_source_rgb(c, 0.1, 0.7, 1.0);
    cairo_rectangle(c, 12, HEIGHT - 36, (WIDTH - 24) * browse_val, 14); cairo_fill(c);

    char buf[32];
    cairo_set_source_rgb(c, 0.6, 0.6, 0.6); cairo_set_font_size(c, 13);
    snprintf(buf, sizeof(buf), "Encoder: %.2f", browse_val);
    cairo_move_to(c, 12, HEIGHT - 42); cairo_show_text(c, buf);

    if (active_pad >= 0) {
        snprintf(buf, sizeof(buf), "Pad %d", active_pad);
        cairo_set_source_rgb(c, 1.0, 0.5, 0.1); cairo_set_font_size(c, 16);
        cairo_move_to(c, 12, HEIGHT - 56); cairo_show_text(c, buf);
    }
}

static void draw_right(void)
{
    cairo_t *c = cr[1];
    cairo_set_source_rgb(c, 0.04, 0.04, 0.1); cairo_paint(c);

    cairo_set_source_rgb(c, 0.8, 0.8, 0.8);
    cairo_select_font_face(c, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(c, 20);
    cairo_move_to(c, 12, 30); cairo_show_text(c, "Maschine Mk3 – Right Screen");

    cairo_set_source_rgb(c, 0.3, 0.3, 0.3);
    cairo_move_to(c, 0, 40); cairo_line_to(c, WIDTH, 40);
    cairo_set_line_width(c, 1); cairo_stroke(c);

    int cy = (HEIGHT + 40) / 2;
    int wh = (HEIGHT - 60) / 2;
    cairo_set_line_width(c, 2);
    double offset = frame_count * 2.5;

    for (int x = 0; x < WIDTH; x += 4) {
        double g   = x + offset;
        double amp = (sin(g * 0.04) * 0.6 + sin(g * 0.13) * 0.4) * wh;
        double rel = (double)x / WIDTH;
        cairo_set_source_rgb(c, 0.2 + 0.8 * rel, 0.5 - 0.3 * rel, 1.0 - 0.7 * rel);
        cairo_move_to(c, x, cy - amp); cairo_line_to(c, x, cy + amp);
        cairo_stroke(c);
    }

    char buf[32];
    cairo_set_source_rgb(c, 0.7, 0.7, 0.7); cairo_set_font_size(c, 14);
    snprintf(buf, sizeof(buf), "Frame: %d", frame_count);
    cairo_move_to(c, 12, HEIGHT - 10); cairo_show_text(c, buf);
}

/* All rendering happens here - called once per ctlra_idle_iter */
void mk3_feedback_func(struct ctlra_dev_t *dev, void *userdata)
{
    draw_left();  push_screen(dev, 0);
    draw_right(); push_screen(dev, 1);
    frame_count++;

    /* Print frame count once per second to confirm callback is firing */
    if (frame_count % FPS == 0)
        printf("frame %d\n", frame_count);
}

void mk3_event_func(struct ctlra_dev_t *dev, uint32_t num_events,
                    struct ctlra_event_t **events, void *userdata)
{
    for (uint32_t i = 0; i < num_events; i++) {
        struct ctlra_event_t *e = events[i];
        switch (e->type) {
        case CTLRA_EVENT_BUTTON:
            if (e->button.pressed) {
                printf("Button %d pressed\n", e->button.id);
                ctlra_dev_light_set(dev, e->button.id,
                    e->button.id == NI_MASCHINE_MK3_BTN_PLAY ? 0xFF00FF00 : 0xFFFFFFFF);
            } else {
                ctlra_dev_light_set(dev, e->button.id, 0);
            }
            ctlra_dev_light_flush(dev, 1);
            break;
        case CTLRA_EVENT_ENCODER:
            if (e->encoder.flags & CTLRA_EVENT_ENCODER_FLAG_INT) {
                browse_val += e->encoder.delta * 0.05f;
                if (browse_val < 0.0f) browse_val = 0.0f;
                if (browse_val > 1.0f) browse_val = 1.0f;
            }
            break;
        case CTLRA_EVENT_GRID:
            if (e->grid.pressed) active_pad = e->grid.pos;
            break;
        case CTLRA_EVENT_SLIDER:
            browse_val = e->slider.value;
            break;
        default: break;
        }
    }
}

int accept_dev_func(struct ctlra_t *ctlra, const struct ctlra_dev_info_t *info,
                    struct ctlra_dev_t *dev, void *userdata)
{
    if (info->vendor_id == 0x17cc && info->device_id == 0x1600) {
        printf("Connected: %s %s\n", info->vendor, info->device);
        ctlra_dev_set_event_func(dev, mk3_event_func);
        ctlra_dev_set_feedback_func(dev, mk3_feedback_func);
        return 1;
    }
    return 0;
}

void sighndlr(int sig) { done = 1; }

int main(int argc, char **argv)
{
    signal(SIGINT, sighndlr);

    for (int i = 0; i < 2; i++) {
        surface[i] = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, WIDTH, HEIGHT);
        cr[i]      = cairo_create(surface[i]);
    }

    struct ctlra_create_opts_t opts = { .screen_redraw_target_fps = FPS };
    struct ctlra_t *ctlra = ctlra_create(&opts);
    if (!ctlra) { fprintf(stderr, "ctlra_create failed\n"); return 1; }

    fprintf(stderr, "[DEMO] ctlra_probe...\n");
    int num = ctlra_probe(ctlra, accept_dev_func, NULL);
    fprintf(stderr, "[DEMO] ctlra_probe returned %d\n", num);
    if (num == 0) {
        fprintf(stderr, "No Maschine Mk3 found\n");
        ctlra_exit(ctlra); return 1;
    }
    fprintf(stderr, "[DEMO] Running at %d fps – Ctrl+C to exit\n", FPS);

    /* Main loop paced at 30fps using nanosleep */
    const long frame_ns = 1000000000L / FPS;
    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);

    while (!done) {
        /* ctlra_idle_iter polls USB input AND calls feedback_func once */
        ctlra_idle_iter(ctlra);

        /* Sleep to the next frame boundary */
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long long diff = ((long long)(next.tv_sec  - now.tv_sec)  * 1000000000LL)
                       + ((long long)(next.tv_nsec - now.tv_nsec));
        if (diff > 0)
            nanosleep(&(struct timespec){ diff / 1000000000LL, diff % 1000000000LL }, NULL);

        next.tv_nsec += frame_ns;
        if (next.tv_nsec >= 1000000000L) { next.tv_sec++; next.tv_nsec -= 1000000000L; }
    }

    ctlra_exit(ctlra);
    for (int i = 0; i < 2; i++) { cairo_destroy(cr[i]); cairo_surface_destroy(surface[i]); }
    return 0;
}
