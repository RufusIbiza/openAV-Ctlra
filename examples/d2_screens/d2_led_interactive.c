#define _DEFAULT_SOURCE
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

void flush_stdin() {
  int c;
  while ((c = getchar()) != '\n' && c != EOF)
    ;
}

int main(int argc, char **argv) {
  signal(SIGINT, sighndlr);

  printf("==============================================\n");
  printf("  D2 LED MAPPER\n");
  printf("  Recording LED-to-Button mappings\n");
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

  FILE *mapping_file = fopen("led_mapping.txt", "w");
  if (!mapping_file) {
    printf("Failed to open mapping file!\n");
    ctlra_exit(ctlra);
    return -1;
  }

  printf("Instructions:\n");
  printf("  1. An LED will light up\n");
  printf("  2. Type which button lit up (e.g., 'PAD 1' or 'NONE')\n");
  printf("  3. Press ENTER to move to next LED\n");
  printf("  (Type 'quit' to stop early)\n\n");

  printf("Ready? Press ENTER to start...");
  flush_stdin();

  char input[100];

  /* Cycle through all LED IDs */
  for (int led_id = 0; led_id < 45 && !done; led_id++) {
    /* Turn on this LED */
    if (led_id < 8) {
      /* RGB pads - use red */
      printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
      printf("LED ID %d: RED (RGB pad) is now ON\n", led_id);
      printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
      ctlra_dev_light_set(g_dev, led_id, 0xFF0000FF);
    } else {
      /* Regular LEDs - full brightness */
      printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
      printf("LED ID %d: WHITE (regular LED) is now ON\n", led_id);
      printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
      ctlra_dev_light_set(g_dev, led_id, UINT32_MAX);
    }
    ctlra_dev_light_flush(g_dev, 1);

    printf("Which button is lit? (or 'NONE'): ");
    fflush(stdout);

    /* Get input */
    if (fgets(input, sizeof(input), stdin) == NULL) {
      break;
    }

    /* Remove newline */
    input[strcspn(input, "\n")] = 0;

    /* Check for quit */
    if (strcmp(input, "quit") == 0) {
      break;
    }

    /* Save to file */
    fprintf(mapping_file, "LED %d: %s\n", led_id, input);
    fflush(mapping_file);

    printf("  ✓ Recorded: LED %d → %s\n", led_id, input);

    /* Turn off */
    ctlra_dev_light_set(g_dev, led_id, 0);
    ctlra_dev_light_flush(g_dev, 1);

    /* Process events */
    ctlra_idle_iter(ctlra);
  }

  fclose(mapping_file);

  printf("\n\n==============================================\n");
  printf("  Mapping saved to: led_mapping.txt\n");
  printf("==============================================\n\n");

  ctlra_exit(ctlra);
  printf("Cleanup complete.\n");
  return 0;
}
