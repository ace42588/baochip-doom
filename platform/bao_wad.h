#ifndef BAO_WAD_H
#define BAO_WAD_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Backend selected at compile time: embedded RRAM blob or SPI NOR. */
typedef enum {
    BAO_WAD_EMBEDDED = 0,
    BAO_WAD_SPI_FLASH,
} bao_wad_backend_t;

bool bao_wad_init(void);
bao_wad_backend_t bao_wad_backend(void);

const uint8_t *bao_wad_data(void);
size_t bao_wad_size(void);

/* True if path looks like our IWAD (used by fopen/M_FileExists stubs). */
bool bao_wad_path_match(const char *path);

#endif
