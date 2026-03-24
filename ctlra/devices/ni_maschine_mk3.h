#ifndef CTLRA_NI_MASCHINE_MK3_H
#define CTLRA_NI_MASCHINE_MK3_H

#include <stdint.h>
#include <ctlra.h>

#ifdef __cplusplus
extern "C" {
#endif

// Full screen blits (send entire framebuffer to device)
uint8_t* ni_maschine_mk3_screen_get_pixels(struct ctlra_dev_t* dev, uint8_t screen_idx);
void ni_maschine_mk3_screen_blit(struct ctlra_dev_t* dev, uint8_t screen_idx);

// Partial screen update - sends only a bounding-box sub-region (experimental on Mk3)
void ni_maschine_mk3_screen_blit_zone(struct ctlra_dev_t* dev, uint8_t screen_idx, uint16_t x, uint16_t y, uint16_t width, uint16_t height);

// Mapped Control and LED IDs as analyzed in ni_maschine_mk3.c
// Note: Pad LEDs are typically accessed via CTLRA_EVENT_GRID instead of specific LED IDs natively in ctlra.
// Button events have ID equal to their positions in the control_names array.

// Top Encoders (Turn) => CTLRA_EVENT_ENCODER
#define NI_MASCHINE_MK3_ENC_MAIN    0
#define NI_MASCHINE_MK3_ENC_1       1
#define NI_MASCHINE_MK3_ENC_2       2
#define NI_MASCHINE_MK3_ENC_3       3
#define NI_MASCHINE_MK3_ENC_4       4
#define NI_MASCHINE_MK3_ENC_5       5
#define NI_MASCHINE_MK3_ENC_6       6
#define NI_MASCHINE_MK3_ENC_7       7
#define NI_MASCHINE_MK3_ENC_8       8

// Buttons => CTLRA_EVENT_BUTTON
#define NI_MASCHINE_MK3_BTN_TOP_1   55
#define NI_MASCHINE_MK3_BTN_TOP_2   56
#define NI_MASCHINE_MK3_BTN_TOP_3   57
#define NI_MASCHINE_MK3_BTN_TOP_4   58
#define NI_MASCHINE_MK3_BTN_TOP_5   59
#define NI_MASCHINE_MK3_BTN_TOP_6   60
#define NI_MASCHINE_MK3_BTN_TOP_7   61
#define NI_MASCHINE_MK3_BTN_TOP_8   62

#define NI_MASCHINE_MK3_BTN_PLAY    40
#define NI_MASCHINE_MK3_BTN_REC     41
#define NI_MASCHINE_MK3_BTN_STOP    42
#define NI_MASCHINE_MK3_BTN_RESTART 36
#define NI_MASCHINE_MK3_BTN_ERASE   37
#define NI_MASCHINE_MK3_BTN_TAP     38

#define NI_MASCHINE_MK3_BTN_MACRO   43
#define NI_MASCHINE_MK3_BTN_SETTINGS 44
#define NI_MASCHINE_MK3_BTN_SAMPLING 46
#define NI_MASCHINE_MK3_BTN_MIXER   47
#define NI_MASCHINE_MK3_BTN_PLUGIN  48
#define NI_MASCHINE_MK3_BTN_CHANNEL 49
#define NI_MASCHINE_MK3_BTN_ARRANGER 50
#define NI_MASCHINE_MK3_BTN_BROWSER 51

// Colors/Brightness macros for `ctlra_dev_light_set` or `dev->lights` manipulation
// Hardware takes an HSV mapping dynamically transformed inside the driver from full ARGB input.
// Normal operations use generic ctlra_dev_light_set(dev, id, color_value).

#ifdef __cplusplus
}
#endif

#endif /* CTLRA_NI_MASCHINE_MK3_H */
