/*
 * OLED presentation path for Baochip / DC34 badge.
 *
 * DC34 badge (BOARD_OLED_SPI_ONLY): CH112OL001A module, SH1107-class,
 *   128x128 1-bit, 4-wire SPI on SPIM2 (PC0 SCK / PC1 MOSI / PC2 C/D /
 *   PC3 CSN0), VOLED boost enabled by PC4. Init sequence and data layout
 *   mirror the stock firmware driver (xous-core bao1x-hal sh1107.rs).
 *
 * Dabao dev board: I2C breakout probing (SSD1327 4-bit grayscale preferred,
 *   SH1107/SSD1306 1-bit fallback), or the SPI path when built with
 *   BAO_OLED_SPI.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "bao.h"
#include "board.h"
#include "bao_display.h"
#include "bao_accel.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/regs/addressmap.h"
#include "hardware/regs/udma.h"

#ifndef BOARD_OLED_I2C_ADDR
#define BOARD_OLED_I2C_ADDR 0x3C
#endif

#ifndef BOARD_OLED_I2C_ADDR_ALT
#define BOARD_OLED_I2C_ADDR_ALT 0x3D
#endif

#ifndef BOARD_OLED_PREFER_SSD1327
#define BOARD_OLED_PREFER_SSD1327 1
#endif

#ifndef BOARD_OLED_SPI_CLKDIV
#define BOARD_OLED_SPI_CLKDIV 24
#endif

#define FB_W BOARD_OLED_WIDTH
#define FB_H BOARD_OLED_HEIGHT

/* SSD1327: 4 bpp, 2 pixels/byte. Max 128×128 → 8 KiB. */
#define GRAY4_BYTES ((FB_W * FB_H) / 2)
/* 1-bit buffer for SH1107/SSD1306 */
#define MONO_BYTES  ((FB_W * FB_H) / 8)

static bao_display_backend_t g_backend = BAO_DISPLAY_NONE;
static uint8_t g_i2c_addr = BOARD_OLED_I2C_ADDR;
static uint8_t g_fb[GRAY4_BYTES > MONO_BYTES ? GRAY4_BYTES : MONO_BYTES];
static uint8_t g_i2c_buf[129] __attribute__((section(".dma_buffers"), aligned(4)));

static const uint8_t bayer4[4][4] = {
    { 0,  8,  2, 10 },
    { 12, 4, 14,  6 },
    { 3, 11,  1,  9 },
    { 15, 7, 13,  5 },
};

/*
 * Portrait scheme: hold the badge 90° clockwise (long edge vertical).
 * Rotate logical pixels 90° clockwise onto the panel: (x, y) → (H-1-y, x).
 * Selected via bao_controls_is_portrait(): compile-time constant for
 * CONTROLS= builds, runtime (gravity) with ACCEL=orient.
 */
static void logical_to_panel(int *x, int *y)
{
    int lx = *x;
    int ly = *y;
    *x = FB_H - 1 - ly;
    *y = lx;
}

static void panel_to_logical(int px, int py, int *lx, int *ly)
{
    *lx = py;
    *ly = FB_H - 1 - px;
}

/* ------------------------------------------------------------------ */
/* I2C panels (Dabao dev-board breakouts)                              */
/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */
/* SPI SH1107 (DC34 badge panel, baosec wiring)                        */
/* ------------------------------------------------------------------ */

/*
 * Boot1's SH1107 driver uses full-duplex UDMA SPI (dummy RX) so the
 * controller actually finishes shifting before CS rises. TX-only
 * transfers report done too early and the panel never updates — the
 * bao splash then stays on screen after we "boot".
 */
static uint8_t spi_dummy_rx[256];

static void spim_reset(uint instance)
{
    uintptr_t base = UDMA_SPIM0_BASE + (uintptr_t)instance * 0x1000u;
    *(volatile uint32_t *)(base + UDMA_RX_CFG_OFFSET) = UDMA_CFG_CLR;
    *(volatile uint32_t *)(base + UDMA_TX_CFG_OFFSET) = UDMA_CFG_CLR;
    *(volatile uint32_t *)(base + UDMA_CMD_CFG_OFFSET) = UDMA_CFG_CLR;
}

static void spi_xfer(const uint8_t *data, uint32_t len)
{
    while (len) {
        uint32_t n = len > 256 ? 256 : len;
        spi_write_read_blocking(BOARD_OLED_SPI, data, spi_dummy_rx, n,
                                BOARD_OLED_SPI_CLKDIV);
        data += n;
        len -= n;
    }
}

static void spi_cmds(const uint8_t *cmds, uint32_t len)
{
    gpio_put(BOARD_OLED_SPI_CD_PORT, BOARD_OLED_SPI_CD_PIN, false);
    spi_xfer(cmds, len);
}

static void spi_data(const uint8_t *data, uint32_t len)
{
    gpio_put(BOARD_OLED_SPI_CD_PORT, BOARD_OLED_SPI_CD_PIN, true);
    spi_xfer(data, len);
}

/* Address one panel column: page 0, column n (data then auto-increments
 * through the 16 pages of the column). */
static void spi_set_column(uint8_t n)
{
    uint8_t cmds[3] = {
        0xB0,                                /* page address 0 */
        (uint8_t)(n & 0x0F),                 /* column address low nibble */
        (uint8_t)(0x10 | ((n >> 4) & 0x07)), /* column address high nibble */
    };
    spi_cmds(cmds, 3);
}

static bool init_spi_sh1107(void)
{
    /* Init sequence from the stock DC34/baosec driver
     * (xous-core bao1x-hal sh1107.rs), with normal instead of inverted
     * display mode: we write 1 = lit. */
    static const uint8_t seq[] = {
        0xAE,       /* display off */
        0xAD, 0x80, /* DC-DC control: external VOLED (MT3608 boost) */
        0xDC, 0x00, /* start line 0 */
        0xD3, 0x00, /* display offset 0 */
        0x81, 0x3F, /* contrast */
        0x21,       /* vertical (column) addressing mode */
        0xA0,       /* segment remap off */
        0xC8,       /* COM scan direction inverted */
        0xA8, 0x7F, /* multiplex ratio 128 */
        0xD5, 0x60, /* clock divider 1, osc freq +5% */
        0xD9, 0x22, /* pre-charge/discharge periods */
        0xDB, 0x35, /* VCOMH deselect level */
        0xB0,       /* page address 0 */
        0xA4,       /* follow RAM */
        0xA6,       /* normal (non-inverted) display */
    };

    /* boot1 may have left SPIM2 enabled on its own IFRAM buffers. */
    spim_reset(BOARD_OLED_SPI);
    spi_init(BOARD_OLED_SPI);

    /* spi_init() muxes PC2 as MISO with a pull-up; on this wiring PC2 is
     * the panel's C/D line, and CS is the hardware CSN0 on PC3. */
    gpio_init(BOARD_OLED_SPI_CD_PORT, BOARD_OLED_SPI_CD_PIN);
    gpio_disable_pulls(BOARD_OLED_SPI_CD_PORT, BOARD_OLED_SPI_CD_PIN);
    gpio_set_dir(BOARD_OLED_SPI_CD_PORT, BOARD_OLED_SPI_CD_PIN, true);
    gpio_put(BOARD_OLED_SPI_CD_PORT, BOARD_OLED_SPI_CD_PIN, false);
    gpio_set_function(BOARD_OLED_SPI_CS_PORT, BOARD_OLED_SPI_CS_PIN, GPIO_FUNC_AF2);
    gpio_set_dir(BOARD_OLED_SPI_CS_PORT, BOARD_OLED_SPI_CS_PIN, true);
    gpio_pull_up(BOARD_OLED_SPI_CS_PORT, BOARD_OLED_SPI_CS_PIN);
    gpio_set_schmitt(BOARD_OLED_SPI_CS_PORT, BOARD_OLED_SPI_CS_PIN, true);

#if defined(BOARD_OLED_PON_PORT)
    /* Enable the VOLED boost converter and let it stabilize. The panel's
     * reset pin rides the TRST_PRST net (RC pull), no GPIO involved. */
    gpio_init(BOARD_OLED_PON_PORT, BOARD_OLED_PON_PIN);
    gpio_set_dir(BOARD_OLED_PON_PORT, BOARD_OLED_PON_PIN, true);
    gpio_put(BOARD_OLED_PON_PORT, BOARD_OLED_PON_PIN, true);
    delay_ms(20);
#endif

    spi_cmds(seq, sizeof(seq));

    /* Black fill so we overwrite boot1's splash immediately. */
    memset(g_fb, 0, MONO_BYTES);
    for (int col = 0; col < FB_H; col++) {
        spi_set_column((uint8_t)col);
        spi_data(&g_fb[col * (FB_W / 8)], FB_W / 8);
    }

    uint8_t on = 0xAF;
    spi_cmds(&on, 1);

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
    if (g_backend != BAO_DISPLAY_NONE) {
        return true;
    }
#ifdef BAO_ACCEL_ORIENT
    /* Sample gravity before the first status screen so it is already
     * drawn in the right orientation. */
    bao_accel_init();
#endif
    memset(g_fb, 0, sizeof(g_fb));

#if defined(BAO_OLED_SPI) || BOARD_OLED_SPI_ONLY
    if (init_spi_sh1107()) {
        mini_printf("OLED: %s %dx%d\r\n", backend_name(g_backend), FB_W, FB_H);
        bao_display_status("loading");
        return true;
    }
#endif

#if !BOARD_OLED_SPI_ONLY
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
        bao_display_status("loading");
        return true;
    }
#endif

    if (init_sh1107_i2c()) {
        mini_printf("OLED: %s 1bpp %dx%d @0x%02x\r\n",
                    backend_name(g_backend), FB_W, FB_H, g_i2c_addr);
        bao_display_status("loading");
        return true;
    }
#endif /* !BOARD_OLED_SPI_ONLY */

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
    if (g_backend == BAO_DISPLAY_SPI_SH1107) {
        /* Column-addressed like the stock driver: panel column n carries
         * framebuffer row n (16 bytes). */
        for (int col = 0; col < FB_H; col++) {
            spi_set_column((uint8_t)col);
            spi_data(&g_fb[col * (FB_W / 8)], FB_W / 8);
        }
        return;
    }

    int pages = FB_H / 8;
    for (int p = 0; p < pages; p++) {
        const uint8_t *row = &g_fb[p * FB_W];
        i2c_cmd((uint8_t)(0xB0 | p));
        i2c_cmd(0x00);
        i2c_cmd(0x10);
        i2c_data(row, FB_W);
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
            uint8_t *dst = &g_fb[y * (FB_W / 2)];
            for (int x = 0; x < FB_W; x += 2) {
                int lx0, ly0, lx1, ly1;
                if (bao_controls_is_portrait()) {
                    panel_to_logical(x, y, &lx0, &ly0);
                    panel_to_logical(x + 1, y, &lx1, &ly1);
                } else {
                    lx0 = x;
                    ly0 = y;
                    lx1 = x + 1;
                    ly1 = y;
                }
                int sx0 = (lx0 * width) / FB_W;
                int sy0 = (ly0 * height) / FB_H;
                int sx1 = (lx1 * width) / FB_W;
                int sy1 = (ly1 * height) / FB_H;
                uint8_t l0 = gray[sy0 * width + sx0] >> 4;
                uint8_t l1 = gray[sy1 * width + sx1] >> 4;
                dst[x / 2] = (uint8_t)((l0 << 4) | l1);
            }
        }
        flush_ssd1327();
        return;
    }

    /* 1-bit panels: Bayer dither */
    memset(g_fb, 0, MONO_BYTES);
    for (int oy = 0; oy < FB_H; oy++) {
        for (int ox = 0; ox < FB_W; ox++) {
            int lx = ox;
            int ly = oy;
            if (bao_controls_is_portrait()) {
                panel_to_logical(ox, oy, &lx, &ly);
            }
            int sy = (ly * height) / FB_H;
            int sx = (lx * width) / FB_W;
            uint8_t lum = gray[sy * width + sx];
            uint8_t thr = (uint8_t)(bayer4[oy & 3][ox & 3] * 16);
            if (lum > thr) {
                if (g_backend == BAO_DISPLAY_SPI_SH1107) {
                    /* Row-major, LSB = lowest x (stock driver layout) */
                    g_fb[oy * (FB_W / 8) + (ox >> 3)] |= (uint8_t)(1u << (ox & 7));
                } else {
                    /* Page layout for I2C SSD1306/SH1107 */
                    g_fb[(oy >> 3) * FB_W + ox] |= (uint8_t)(1u << (oy & 7));
                }
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

/* ------------------------------------------------------------------ */
/* Tiny 5x7 font (Adafruit GLCD subset, 0x20-0x5F) + status text       */
/* Column-major, bit0 = top pixel. Lowercase is folded to uppercase.   */
/* ------------------------------------------------------------------ */

static const uint8_t font5x7[64][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5f,0x00,0x00},
    {0x00,0x07,0x00,0x07,0x00}, {0x14,0x7f,0x14,0x7f,0x14},
    {0x24,0x2a,0x7f,0x2a,0x12}, {0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x55,0x22,0x50}, {0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1c,0x22,0x41,0x00}, {0x00,0x41,0x22,0x1c,0x00},
    {0x14,0x08,0x3e,0x08,0x14}, {0x08,0x08,0x3e,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08},
    {0x00,0x60,0x60,0x00,0x00}, {0x20,0x10,0x08,0x04,0x02},
    {0x3e,0x51,0x49,0x45,0x3e}, {0x00,0x42,0x7f,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4b,0x31},
    {0x18,0x14,0x12,0x7f,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3c,0x4a,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1e},
    {0x00,0x36,0x36,0x00,0x00}, {0x00,0x56,0x36,0x00,0x00},
    {0x08,0x14,0x22,0x41,0x00}, {0x14,0x14,0x14,0x14,0x14},
    {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3e}, {0x7e,0x11,0x11,0x11,0x7e},
    {0x7f,0x49,0x49,0x49,0x36}, {0x3e,0x41,0x41,0x41,0x22},
    {0x7f,0x41,0x41,0x22,0x1c}, {0x7f,0x49,0x49,0x49,0x41},
    {0x7f,0x09,0x09,0x09,0x01}, {0x3e,0x41,0x49,0x49,0x7a},
    {0x7f,0x08,0x08,0x08,0x7f}, {0x00,0x41,0x7f,0x41,0x00},
    {0x20,0x40,0x41,0x3f,0x01}, {0x7f,0x08,0x14,0x22,0x41},
    {0x7f,0x40,0x40,0x40,0x40}, {0x7f,0x02,0x0c,0x02,0x7f},
    {0x7f,0x04,0x08,0x10,0x7f}, {0x3e,0x41,0x41,0x41,0x3e},
    {0x7f,0x09,0x09,0x09,0x06}, {0x3e,0x41,0x51,0x21,0x5e},
    {0x7f,0x09,0x19,0x29,0x46}, {0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7f,0x01,0x01}, {0x3f,0x40,0x40,0x40,0x3f},
    {0x1f,0x20,0x40,0x20,0x1f}, {0x3f,0x40,0x38,0x40,0x3f},
    {0x63,0x14,0x08,0x14,0x63}, {0x07,0x08,0x70,0x08,0x07},
    {0x61,0x51,0x49,0x45,0x43}, {0x00,0x7f,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20}, {0x00,0x41,0x41,0x7f,0x00},
    {0x04,0x02,0x01,0x02,0x04}, {0x40,0x40,0x40,0x40,0x40},
};

static void fb_plot(int x, int y, int on)
{
    if (bao_controls_is_portrait()) {
        logical_to_panel(&x, &y);
    }
    if (x < 0 || y < 0 || x >= FB_W || y >= FB_H) {
        return;
    }
    if (g_backend == BAO_DISPLAY_I2C_SSD1327) {
        uint8_t *dst = &g_fb[y * (FB_W / 2) + (x / 2)];
        uint8_t nibble = on ? 0x0F : 0x00;
        if (x & 1) {
            *dst = (uint8_t)((*dst & 0xF0) | nibble);
        } else {
            *dst = (uint8_t)((*dst & 0x0F) | (nibble << 4));
        }
        return;
    }
    if (g_backend == BAO_DISPLAY_SPI_SH1107) {
        uint8_t *dst = &g_fb[y * (FB_W / 8) + (x >> 3)];
        uint8_t bit = (uint8_t)(1u << (x & 7));
        if (on) {
            *dst |= bit;
        } else {
            *dst &= (uint8_t)~bit;
        }
        return;
    }
    {
        uint8_t *dst = &g_fb[(y >> 3) * FB_W + x];
        uint8_t bit = (uint8_t)(1u << (y & 7));
        if (on) {
            *dst |= bit;
        } else {
            *dst &= (uint8_t)~bit;
        }
    }
}

static void fb_draw_char(int x, int y, char c, int scale, int fg)
{
    unsigned idx;
    int bg = fg ? 0 : 1;
    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }
    if (c < 0x20 || c > 0x5F) {
        c = '?';
    }
    if (scale < 1) {
        scale = 1;
    }
    idx = (unsigned)(c - 0x20);
    for (int col = 0; col < 5; col++) {
        uint8_t bits = font5x7[idx][col];
        for (int row = 0; row < 7; row++) {
            int lit = (bits >> row) & 1;
            int color = lit ? fg : bg;
            for (int sy = 0; sy < scale; sy++) {
                for (int sx = 0; sx < scale; sx++) {
                    fb_plot(x + col * scale + sx, y + row * scale + sy, color);
                }
            }
        }
    }
}

static int fb_draw_str(int x0, int y, const char *s, int scale, int fg)
{
    int x = x0;
    int cell = 6 * scale;
    int row_h = 8 * scale;
    while (s && *s) {
        if (*s == '\n') {
            y += row_h;
            x = x0;
            s++;
            continue;
        }
        if (*s == '\r') {
            s++;
            continue;
        }
        if (x + cell > FB_W) {
            x = x0;
            y += row_h;
        }
        if (y + 7 * scale > FB_H) {
            break;
        }
        fb_draw_char(x, y, *s++, scale, fg);
        x += cell;
    }
    return y + row_h;
}

static void display_flush(void)
{
    if (g_backend == BAO_DISPLAY_I2C_SSD1327) {
        flush_ssd1327();
    } else {
        flush_mono();
    }
}

static void display_clear_black(void)
{
    size_t fill = (g_backend == BAO_DISPLAY_I2C_SSD1327) ? (size_t)GRAY4_BYTES
                                                         : (size_t)MONO_BYTES;
    memset(g_fb, 0x00, fill);
}

void bao_display_status(const char *msg)
{
    if (g_backend == BAO_DISPLAY_NONE) {
        return;
    }
    display_clear_black();
    fb_draw_str(0, 0, "DC34 DOOM", 2, 1);
    fb_draw_str(0, 20, msg ? msg : "", 1, 1);
    display_flush();
}

void bao_display_fatal(const char *msg)
{
    if (g_backend == BAO_DISPLAY_NONE) {
        return;
    }
    /* Black panel, 2x white glyphs. The old invert path filled white then
     * plotted the strokes as white, so FATAL vanished into the background. */
    display_clear_black();
    fb_draw_str(0, 2, "FATAL", 2, 1);
    fb_draw_str(0, 22, msg ? msg : "", 1, 1);
    display_flush();
}

void bao_display_log_line(const char *msg)
{
    static uint32_t last_ms;
    static char last[96];
    uint32_t now;
    const char *p;
    int useful = 0;
    size_t n = 0;

    if (!msg || g_backend == BAO_DISPLAY_NONE) {
        return;
    }
    for (p = msg; *p && n + 1 < sizeof(last); p++) {
        if (*p == '\r' || *p == '\n') {
            continue;
        }
        last[n++] = *p;
        if (*p != '.' && *p != ' ' && *p != '-') {
            useful = 1;
        }
    }
    last[n] = 0;
    if (!useful) {
        return;
    }
    now = (uint32_t)millis();
    if (last_ms != 0 && (now - last_ms) < 200u) {
        return;
    }
    last_ms = now;
    bao_display_status(last);
}
