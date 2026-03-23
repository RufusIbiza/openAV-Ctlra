#define _DEFAULT_SOURCE
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ctlra.h"
#include "devices/ni_kontrol_d2.h"

static volatile uint32_t done = 0;
static struct ctlra_dev_t *g_dev = NULL;

void sighndlr(int signal) {
  done = 1;
  printf("\nExiting...\n");
}

void d2_feedback_func(struct ctlra_dev_t *dev, void *userdata) {}

void d2_event_func(struct ctlra_dev_t *dev, uint32_t num_events,
                   struct ctlra_event_t **events, void *userdata) {}

int accept_dev_func(struct ctlra_t *ctlra, const struct ctlra_dev_info_t *info,
                    struct ctlra_dev_t *dev, void *userdata) {
  if (info->vendor_id == 0x17cc && info->device_id == 0x1400) {
    printf("Accepting Kontrol D2: %s %s\n", info->vendor, info->device);

    ctlra_dev_set_event_func(dev, d2_event_func);
    ctlra_dev_set_feedback_func(dev, d2_feedback_func);
    ctlra_dev_set_callback_userdata(dev, userdata);

    g_dev = dev;
    return 1;
  }

  return 0;
}

int main(int argc, char **argv) {
  signal(SIGINT, sighndlr);

  printf("==============================================\n");
  printf("  D2 LED CYCLER\n");
  printf("  Lighting each LED for 2 seconds\n");
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

  if (num_devs == 0 || !g_dev) {
    printf("No Kontrol D2 found!\n");
    ctlra_exit(ctlra);
    return -1;
  }

  printf("Starting LED cycle...\n");
  printf("Watch which button lights up and note the LED ID!\n\n");
  sleep(2);

  /* Cycle through all LED IDs */
  for (int led_id = 0; led_id < 45 && !done; led_id++) {
    printf("LED ID %d: ", led_id);
    fflush(stdout);

    /* Turn on this LED */
    if (led_id < 8) {
      /* RGB pads - use red */
      ctlra_dev_light_set(g_dev, led_id, 0xFF0000FF);
    } else {
      /* Regular LEDs - full brightness */
      ctlra_dev_light_set(g_dev, led_id, UINT32_MAX);
    }
    ctlra_dev_light_flush(g_dev, 1);

    /* Wait 2 seconds */
    for (int i = 0; i < 20 && !done; i++) {
      ctlra_idle_iter(ctlra);
      usleep(100000); /* 100ms */
    }

    /* Turn off */
    ctlra_dev_light_set(g_dev, led_id, 0);
    ctlra_dev_light_flush(g_dev, 1);

    printf("(done)\n");

    /* Short pause between LEDs */
    usleep(300000); /* 300ms */
  }

  printf("\nCycle complete! Press Ctrl+C to exit.\n");

  while (!done) {
    ctlra_idle_iter(ctlra);
    usleep(100000);
  }

  ctlra_exit(ctlra);
  printf("Cleanup complete.\n");
  return 0;
}
