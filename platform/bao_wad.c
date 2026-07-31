/*
 * WAD backend: embedded RRAM blob (default) or SPI NOR via W25Q.
 * Provides stdc_wad_file so doomgeneric's w_file.c keeps working.
 */

#include <string.h>
#include <stddef.h>
#include <stdint.h>

#include "bao.h"
#include "board.h"
#include "bao_wad.h"
#include "w_file.h"
#include "z_zone.h"

#if defined(WAD_BACKEND_SPI)
#include "hardware/w25q.h"
#endif

/* Produced by: riscv-none-elf-objcopy -I binary -O elf32-littleriscv -B riscv doom1.wad */
#if !defined(WAD_BACKEND_SPI)
extern const uint8_t _binary_doom1_wad_start[];
extern const uint8_t _binary_doom1_wad_end[];
#endif

#if defined(WAD_BACKEND_SPI)
static uint8_t g_spi_cache[4096] __attribute__((section(".dma_buffers"), aligned(4)));
static uint32_t g_spi_cache_base = 0xffffffffu;
static size_t g_spi_wad_size;
static int g_spi_ok;
#endif

static const uint8_t *g_wad;
static size_t g_wad_len;
static bao_wad_backend_t g_backend;

bool bao_wad_path_match(const char *path)
{
    const char *base;
    if (!path) {
        return false;
    }
    base = strrchr(path, '/');
    base = base ? base + 1 : path;
    return strcasecmp(base, "doom1.wad") == 0
        || strcasecmp(base, "doom.wad") == 0
        || strcasecmp(base, "doom2.wad") == 0
        || strcasecmp(path, ".") == 0;
}

bool bao_wad_init(void)
{
#if defined(WAD_BACKEND_SPI)
    g_backend = BAO_WAD_SPI_FLASH;
    g_spi_ok = (w25q_init(BOARD_W25Q_INSTANCE, BOARD_W25Q_CS, BOARD_W25Q_CLKDIV) == 0);
    if (!g_spi_ok) {
        mini_printf("WAD: W25Q init failed, falling back to empty\r\n");
        g_wad = NULL;
        g_wad_len = 0;
        return false;
    }
    /* Expect IWAD size stored at offset 0 as raw wad; use declared size or probe.
     * For full doom1.wad, set BAO_SPI_WAD_SIZE at build time (default 4196020). */
#ifndef BAO_SPI_WAD_SIZE
#define BAO_SPI_WAD_SIZE 4196020u
#endif
    g_spi_wad_size = BAO_SPI_WAD_SIZE;
    g_wad = NULL; /* unmapped — Read() via SPI */
    g_wad_len = g_spi_wad_size;
    mini_printf("WAD: SPI NOR size=%u @0x%x\r\n",
                (unsigned)g_wad_len, (unsigned)BOARD_WAD_SPI_OFFSET);
    return true;
#else
    g_backend = BAO_WAD_EMBEDDED;
    g_wad = _binary_doom1_wad_start;
    g_wad_len = (size_t)(_binary_doom1_wad_end - _binary_doom1_wad_start);
    mini_printf("WAD: embedded size=%u @%p\r\n", (unsigned)g_wad_len, (void *)g_wad);
    return g_wad_len > 0;
#endif
}

bao_wad_backend_t bao_wad_backend(void)
{
    return g_backend;
}

const uint8_t *bao_wad_data(void)
{
    return g_wad;
}

size_t bao_wad_size(void)
{
    return g_wad_len;
}

/* ---- stdc_wad_file replacement ---- */

typedef struct {
    wad_file_t wad;
} bao_wad_file_t;

extern wad_file_class_t stdc_wad_file;

static wad_file_t *W_Bao_OpenFile(char *path)
{
    bao_wad_file_t *result;

    if (!bao_wad_path_match(path) && path && path[0] != '\0') {
        /* Still accept any open attempt and serve our IWAD (IWAD search). */
    }

    if (g_wad_len == 0) {
        return NULL;
    }

    result = Z_Malloc(sizeof(*result), PU_STATIC, 0);
    result->wad.file_class = &stdc_wad_file;
#if defined(WAD_BACKEND_SPI)
    result->wad.mapped = NULL;
#else
    result->wad.mapped = (byte *)g_wad;
#endif
    result->wad.length = (unsigned int)g_wad_len;
    return &result->wad;
}

static void W_Bao_CloseFile(wad_file_t *wad)
{
    Z_Free(wad);
}

static size_t W_Bao_Read(wad_file_t *wad, unsigned int offset,
                         void *buffer, size_t buffer_len)
{
#if defined(WAD_BACKEND_SPI)
    size_t done = 0;
    uint8_t *out = (uint8_t *)buffer;
    (void)wad;

    if (offset >= g_wad_len) {
        return 0;
    }
    if (offset + buffer_len > g_wad_len) {
        buffer_len = g_wad_len - offset;
    }

    while (done < buffer_len) {
        uint32_t abs = BOARD_WAD_SPI_OFFSET + offset + (uint32_t)done;
        uint32_t page = abs & ~0xfffu;
        uint32_t off = abs & 0xfffu;
        size_t chunk = 4096u - off;
        if (chunk > buffer_len - done) {
            chunk = buffer_len - done;
        }
        if (g_spi_cache_base != page) {
            w25q_read(page, g_spi_cache, 4096);
            g_spi_cache_base = page;
        }
        memcpy(out + done, g_spi_cache + off, chunk);
        done += chunk;
    }
    return done;
#else
    if (offset >= wad->length) {
        return 0;
    }
    if (offset + buffer_len > wad->length) {
        buffer_len = wad->length - offset;
    }
    memcpy(buffer, wad->mapped + offset, buffer_len);
    return buffer_len;
#endif
}

wad_file_class_t stdc_wad_file = {
    W_Bao_OpenFile,
    W_Bao_CloseFile,
    W_Bao_Read,
};
