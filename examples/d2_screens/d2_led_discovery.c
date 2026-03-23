#define _DEFAULT_SOURCE
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ctlra.h"
#include "devices/ni_kontrol_d2.h"

static volatile uint32_t done = 0;
static struct ctlra_dev_t *g_dev = NULL;
static int current_led = 0;
static int led_count = 0;
static int user_confirmed = 0;

void sighndlr(int signal) {
  done = 1;
  printf("\nExiting...\n");
}

void d2_feedback_func(struct ctlra_dev_t *dev, void *userdata) {
  /* Nothing needed here */
}

void d2_event_func(struct ctlra_dev_t *dev, uint32_t num_events,
                   struct ctlra_event_t **events, void *userdata) {
  for (uint32_t i = 0; i < num_events; i++) {
    struct ctlra_event_t *e = events[i];

    /* Check if PLAY button was pressed */
    if (e->type == CTLRA_EVENT_BUTTON &&
        e->button.id == NI_KONTROL_D2_BTN_PLAY && e->button.pressed) {

      user_confirmed = 1;
      printf("✓ LED %d WORKS!\n\n", current_led);
      led_count++;
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

    g_dev = dev;
    return 1;
  }

  return 0;
}

void test_led(int led_id) {
  if (!g_dev)
    return;

  current_led = led_id;
  user_confirmed = 0;

  printf("Testing LED ID %d...\n", led_id);
  printf("  If you see an LED light up, press PLAY.\n");
  printf("  If no LED lights, wait 3 seconds.\n");

  /* Turn on this LED at full brightness */
  ctlra_dev_light_set(g_dev, led_id, UINT32_MAX);
  ctlra_dev_light_flush(g_dev, 1);

  /* Wait 3 seconds for user response */
  for (int i = 0; i < 30; i++) {
    usleep(100000); /* 100ms */
    if (user_confirmed) {
      break;
    }
  }

  /* Turn off the LED */
  ctlra_dev_light_set(g_dev, led_id, 0);
  ctlra_dev_light_flush(g_dev, 1);

  if (!user_confirmed) {
    printf("  No LED detected (ID %d doesn't work or no LED)\n\n", led_id);
  }

  usleep(500000); /* 500ms pause between tests */
}

void test_rgb_led(int led_id, const char *color_name, uint32_t color_value) {
  if (!g_dev)
    return;

  current_led = led_id;
  user_confirmed = 0;

  printf("Testing LED ID %d (%s)...\n", led_id, color_name);
  printf("  If you see a %s LED light up, press PLAY.\n", color_name);
  printf("  If no LED lights, wait 3 seconds.\n");

  /* Turn on this LED with specific color */
  ctlra_dev_light_set(g_dev, led_id, color_value);
  ctlra_dev_light_flush(g_dev, 1);

  /* Wait 3 seconds for user response */
  for (int i = 0; i < 30; i++) {
    usleep(100000); /* 100ms */
    if (user_confirmed) {
      break;
    }
  }

  /* Turn off the LED */
  ctlra_dev_light_set(g_dev, led_id, 0);
  ctlra_dev_light_flush(g_dev, 1);

  if (!user_confirmed) {
    printf("  No %s LED detected\n\n", color_name);
  }

  usleep(500000); /* 500ms pause between tests */
}

int main(int argc, char **argv) {
  signal(SIGINT, sighndlr);

  printf("==============================================\n");
  printf("  D2 LED DISCOVERY TOOL\n");
  printf("  Finding which LED IDs actually work\n");
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

  printf("INSTRUCTIONS:\n");
  printf("  - Each LED ID will be tested\n");
  printf("  - If you see an LED light up, press the PLAY button\n");
  printf("  - If no LED lights within 3 seconds, test moves on\n");
  printf("  - Press Ctrl+C to abort\n\n");
  printf("Starting in 3 seconds...\n");
  sleep(3);
  printf("\n");

  /* Test RGB Pads (IDs 0-7) - these might need RGB values */
  printf("=== Testing RGB PADS (0-7) ===\n\n");
  for (int i = 0; i < 8 && !done; i++) {
    /* Test each color */
    test_rgb_led(i, "RED", 0xFF0000FF);
    if (user_confirmed) {
      test_rgb_led(i, "GREEN", 0xFF00FF00);
      test_rgb_led(i, "BLUE", 0xFFFF0000);
      test_rgb_led(i, "WHITE", 0xFFFFFFFF);
    }

    /* Process events */
    ctlra_idle_iter(ctlra);
  }

  /* Test all other LEDs in sequence */
  printf("=== Testing Other LEDs (8-84) ===\n\n");
  for (int i = 8; i <= NI_KONTROL_D2_LED_COUNT && !done; i++) {
    test_led(i);

    /* Process events */
    ctlra_idle_iter(ctlra);
  }

  printf("\n==============================================\n");
  printf("  LED DISCOVERY COMPLETE\n");
  printf("==============================================\n");
  printf("  Working LEDs found: %d\n", led_count);
  printf("  Check the output above for LED IDs that work\n");
  printf("==============================================\n\n");

  ctlra_exit(ctlra);
  return 0;
}
