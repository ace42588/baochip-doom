#ifndef BAO_DISPLAY_H
#define BAO_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    BAO_DISPLAY_NONE = 0,
    BAO_DISPLAY_I2C_SSD1327,   /* 4-bit grayscale (Dabao dev-board breakout) */
    BAO_DISPLAY_I2C_SH1107,    /* 1-bit mono square OLED breakout */
    BAO_DISPLAY_I2C_SSD1306,   /* 1-bit mono (taller panels / Dabao breakouts) */
    BAO_DISPLAY_SPI_SH1107,    /* DC34 badge panel (CH112OL001A, 128x128) */
} bao_display_backend_t;

/* Probe and init OLED. Returns false if none found (headless OK). */
bool bao_display_init(void);

bao_display_backend_t bao_display_backend(void);
int bao_display_width(void);
int bao_display_height(void);

/*
 * Present a 320x200 RGBA8888 frame (doomgeneric DG_ScreenBuffer layout)
 * scaled onto the OLED. SSD1327 uses native 4-bit grayscale; 1-bit panels
 * use Bayer dither.
 */
void bao_display_present_rgba(const uint32_t *fb, int width, int height);

/* Present luminance 8-bit source */
void bao_display_present_gray(const uint8_t *gray, int width, int height);

#endif
