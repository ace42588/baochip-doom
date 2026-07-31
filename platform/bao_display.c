/*
 * OLED present path for Baochip / DC34 badge.
 *
 * Preferred: SSD1327 I²C square grayscale (128×128 or 96×96), addr 0x3C/0x3D.
 * Fallback:  SH1107 / SSD1306 1-bit with Bayer dither.
 * Optional:  SPI SH1107 (baosec-like) when BAO_OLED_SPI is defined.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "bao.h"
#include "board.h"
#include "bao_display.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

#ifndef BOARD_OLED_I2C_ADDR_ALT
#define BOARD_OLED_I2C_ADDR_ALT 0x3D
#endif

#ifndef BOARD_OLED_PREFER_SSD1327
#define BOARD_OLED_PREFER_SSD1327 1
#endif

#define FB_W BOARD_OLED_WIDTH
#define FB_H BOARD_OLED_HEIGHT

/* SSD1327: 4 bpp, 2 pixels/byte. Max 128×128 → 8 KiB. */
#define GRAY4_BYTES ((FB_W * FB_H) / 2)
/* 1-bit page buffer for SH1107/SSD1306 */
#define MONO_BYTES  ((FB_W * FB_H) / 8)

static bao_display_backend_t g_backend = BAO_DISPLAY_NONE;
static uint8_t g_i2c_addr = BOARD_OLED_I2C_ADDR;
static uint8_t g_fb[GRAY4_BYTES > MONO_BYTES ? GRAY4_BYTES : MONO_BYTES];
static uint8_t g_i2c_buf[129] __attribute__((section(".dma_buffers"), aligned(4)));
static uint8_t g_spi_buf[FB_W] __attribute__((section(".dma_buffers"), aligned(4)));

static const uint8_t bayer4[4][4] = {
    { 0,  8,  2, 10 },
    { 12, 4, 14,  6 },
    { 3, 11,  1,  9 },
    { 15, 7, 13,  5 },
};

static int i2c_cmd(uint8_t cmd)
{
    uint8_t buf[2] = { 0x00, cmd };
    return i2c_write_blocking(BOARD_OLED_I2C, g_i2c_addr, buf, 2);
}

static int i2c_cmd2(uint8_t cmd, uint8_t arg)
{
    uint8_t buf[3] = { 0x00, cmd, arg };
    return i2c_write_blocking(BOARD_OLED_I2C, g_i2c_addr, buf, 3);
}

static int i2c_cmd3(uint8_t cmd, uint8_t a, uint8_t b)
{
    uint8_t buf[4] = { 0x00, cmd, a, b };
    return i2c_write_blocking(BOARD_OLED_I2C, g_i2c_addr, buf, 4);
}

static int i2c_data(const uint8_t *data, uint32_t len)
{
    while (len) {
        uint32_t n = len > 128 ? 128 : len;
        g_i2c_buf[0] = 0x40;
        memcpy(&g_i2c_buf[1], data, n);
        if (i2c_write_blocking(BOARD_OLED_I2C, g_i2c_addr, g_i2c_buf, n + 1) != 0) {
            return -1;
        }
        data += n;
        len -= n;
    }
    return 0;
}

static bool i2c_probe_addr(uint8_t addr)
{
    uint8_t buf[2] = { 0x00, 0xAE }; /* display off */
    g_i2c_addr = addr;
    return i2c_write_blocking(BOARD_OLED_I2C, addr, buf, 2) == 0;
}

static bool init_ssd1327(void)
{
    uint8_t col_end = (uint8_t)(FB_W / 2 - 1); /* SSD1327 columns are segment pairs */
    uint8_t row_end = (uint8_t)(FB_H - 1);

    if (i2c_cmd(0xAE) != 0) {
        return false;
    }

    i2c_cmd3(0x15, 0x00, col_end);           /* column address */
    i2c_cmd3(0x75, 0x00, row_end);           /* row address */
    i2c_cmd2(0x81, 0x80);                    /* contrast */
    i2c_cmd2(0xA0, 0x51);                    /* remap: horizontal increment, nibble remap */
    i2c_cmd2(0xA1, 0x00);                    /* start line */
    i2c_cmd2(0xA2, 0x00);                    /* display offset */
    i2c_cmd(0xA4);                           /* normal display (resume RAM) */
    i2c_cmd2(0xA8, row_end);                 /* multiplex ratio */
    i2c_cmd2(0xB1, 0xF1);                    /* phase length */
    i2c_cmd2(0xB3, 0x00);                    /* clock div */
    i2c_cmd2(0xAB, 0x01);                    /* enable internal VDD regulator */
    i2c_cmd2(0xB6, 0x0F);                    /* second pre-charge period */
    i2c_cmd2(0xBE, 0x0F);                    /* VCOMH */
    i2c_cmd2(0xBC, 0x08);                    /* pre-charge voltage */
    i2c_cmd2(0xD5, 0x62);                    /* function selection B */
    i2c_cmd2(0xFD, 0x12);                    /* unlock commands */
    i2c_cmd(0xAF);                           /* display on */

    g_backend = BAO_DISPLAY_I2C_SSD1327;
    return true;
}

static bool init_sh1107_i2c(void)
{
    if (i2c_cmd(0xAE) != 0) {
        return false;
    }
    i2c_cmd2(0xD5, 0x50);
    i2c_cmd2(0xA8, (uint8_t)(FB_H - 1));
    i2c_cmd2(0xD3, 0x00);
    i2c_cmd(0x40);
    i2c_cmd(0xA1);
    i2c_cmd(0xC8);
    i2c_cmd2(0xDA, 0x12);
    i2c_cmd2(0x81, 0x80);
    i2c_cmd2(0xD9, 0x22);
    i2c_cmd2(0xDB, 0x35);
    i2c_cmd(0xA4);
    i2c_cmd(0xA6);
    i2c_cmd(0xAF);

    g_backend = (FB_H > 64) ? BAO_DISPLAY_I2C_SH1107 : BAO_DISPLAY_I2C_SSD1306;
    return true;
}

static void spi_cmd(uint8_t cmd)
{
    gpio_put(BOARD_OLED_SPI_CD_PORT, BOARD_OLED_SPI_CD_PIN, false);
    spi_write_blocking(BOARD_OLED_SPI, &cmd, 1, 9);
}

static void spi_data(const uint8_t *data, uint32_t len)
{
    gpio_put(BOARD_OLED_SPI_CD_PORT, BOARD_OLED_SPI_CD_PIN, true);
    while (len) {
        uint32_t n = len > FB_W ? FB_W : len;
        memcpy(g_spi_buf, data, n);
        spi_write_blocking(BOARD_OLED_SPI, g_spi_buf, n, 9);
        data += n;
        len -= n;
    }
}

static bool init_spi_sh1107(void)
{
    spi_init(BOARD_OLED_SPI);
    gpio_init(BOARD_OLED_SPI_CD_PORT, BOARD_OLED_SPI_CD_PIN);
    gpio_set_dir(BOARD_OLED_SPI_CD_PORT, BOARD_OLED_SPI_CD_PIN, true);

    spi_cmd(0xAE);
    spi_cmd(0xD5); spi_cmd(0x50);
    spi_cmd(0xA8); spi_cmd((uint8_t)(FB_H - 1));
    spi_cmd(0xD3); spi_cmd(0x00);
    spi_cmd(0x40);
    spi_cmd(0xA1);
    spi_cmd(0xC8);
    spi_cmd(0xDA); spi_cmd(0x12);
    spi_cmd(0x81); spi_cmd(0x80);
    spi_cmd(0xD9); spi_cmd(0x22);
    spi_cmd(0xDB); spi_cmd(0x35);
    spi_cmd(0xA4);
    spi_cmd(0xA6);
    spi_cmd(0xAF);

    g_backend = BAO_DISPLAY_SPI_SH1107;
    return true;
}

static const char *backend_name(bao_display_backend_t b)
{
    switch (b) {
    case BAO_DISPLAY_I2C_SSD1327: return "SSD1327";
    case BAO_DISPLAY_I2C_SH1107:  return "SH1107";
    case BAO_DISPLAY_I2C_SSD1306: return "SSD1306";
    case BAO_DISPLAY_SPI_SH1107:  return "SH1107-SPI";
    default: return "none";
    }
}

bool bao_display_init(void)
{
    memset(g_fb, 0, sizeof(g_fb));

#if defined(BAO_OLED_SPI)
    if (init_spi_sh1107()) {
        mini_printf("OLED: %s %dx%d\r\n", backend_name(g_backend), FB_W, FB_H);
        return true;
    }
#endif

    i2c_init(BOARD_OLED_I2C, 400000);

    if (!i2c_probe_addr(BOARD_OLED_I2C_ADDR) &&
        !i2c_probe_addr(BOARD_OLED_I2C_ADDR_ALT)) {
        g_backend = BAO_DISPLAY_NONE;
        mini_printf("OLED: none (headless)\r\n");
        return false;
    }

#if BOARD_OLED_PREFER_SSD1327
    if (init_ssd1327()) {
        mini_printf("OLED: %s 4bpp %dx%d @0x%02x\r\n",
                    backend_name(g_backend), FB_W, FB_H, g_i2c_addr);
        return true;
    }
#endif

    if (init_sh1107_i2c()) {
        mini_printf("OLED: %s 1bpp %dx%d @0x%02x\r\n",
                    backend_name(g_backend), FB_W, FB_H, g_i2c_addr);
        return true;
    }

    g_backend = BAO_DISPLAY_NONE;
    mini_printf("OLED: none (headless)\r\n");
    return false;
}

bao_display_backend_t bao_display_backend(void)
{
    return g_backend;
}

int bao_display_width(void)
{
    return FB_W;
}

int bao_display_height(void)
{
    return FB_H;
}

static void flush_ssd1327(void)
{
    uint8_t col_end = (uint8_t)(FB_W / 2 - 1);
    uint8_t row_end = (uint8_t)(FB_H - 1);

    i2c_cmd3(0x15, 0x00, col_end);
    i2c_cmd3(0x75, 0x00, row_end);
    i2c_data(g_fb, GRAY4_BYTES);
}

static void flush_mono(void)
{
    int pages = FB_H / 8;
    for (int p = 0; p < pages; p++) {
        const uint8_t *row = &g_fb[p * FB_W];
        if (g_backend == BAO_DISPLAY_SPI_SH1107) {
            spi_cmd((uint8_t)(0xB0 | p));
            spi_cmd(0x00);
            spi_cmd(0x10);
            spi_data(row, FB_W);
        } else {
            i2c_cmd((uint8_t)(0xB0 | p));
            i2c_cmd(0x00);
            i2c_cmd(0x10);
            i2c_data(row, FB_W);
        }
    }
}

void bao_display_present_gray(const uint8_t *gray, int width, int height)
{
    if (g_backend == BAO_DISPLAY_NONE) {
        return;
    }

    if (g_backend == BAO_DISPLAY_I2C_SSD1327) {
        /* Scale into 4-bit grayscale: two horizontal pixels per byte. */
        for (int y = 0; y < FB_H; y++) {
            int sy = (y * height) / FB_H;
            uint8_t *dst = &g_fb[y * (FB_W / 2)];
            for (int x = 0; x < FB_W; x += 2) {
                int sx0 = (x * width) / FB_W;
                int sx1 = ((x + 1) * width) / FB_W;
                uint8_t l0 = gray[sy * width + sx0] >> 4;
                uint8_t l1 = gray[sy * width + sx1] >> 4;
                dst[x / 2] = (uint8_t)((l0 << 4) | l1);
            }
        }
        flush_ssd1327();
        return;
    }

    /* 1-bit panels: Bayer dither */
    memset(g_fb, 0, MONO_BYTES);
    for (int oy = 0; oy < FB_H; oy++) {
        int sy = (oy * height) / FB_H;
        for (int ox = 0; ox < FB_W; ox++) {
            int sx = (ox * width) / FB_W;
            uint8_t lum = gray[sy * width + sx];
            uint8_t thr = (uint8_t)(bayer4[oy & 3][ox & 3] * 16);
            if (lum > thr) {
                g_fb[(oy >> 3) * FB_W + ox] |= (uint8_t)(1u << (oy & 7));
            }
        }
    }
    flush_mono();
}

void bao_display_present_rgba(const uint32_t *fb, int width, int height)
{
    static uint8_t gray[320 * 200];
    int w = width > 320 ? 320 : width;
    int h = height > 200 ? 200 : height;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t px = fb[y * width + x];
            uint8_t r = (uint8_t)(px & 0xff);
            uint8_t g = (uint8_t)((px >> 8) & 0xff);
            uint8_t b = (uint8_t)((px >> 16) & 0xff);
            gray[y * w + x] = (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
        }
    }
    bao_display_present_gray(gray, w, h);
}
