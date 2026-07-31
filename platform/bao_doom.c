/*
 * doomgeneric platform entry for Baochip-1x / Dabao / DC34 badge.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bao.h"
#include "board.h"
#include "bao_display.h"
#include "bao_input.h"
#include "bao_wad.h"
#include "doomgeneric.h"
#include "doomkeys.h"

static uint32_t g_frames;

void DG_Init(void)
{
    bao_input_init();
    bao_display_init();
    mini_printf("DG_Init done\r\n");
}

void DG_DrawFrame(void)
{
    bao_input_poll();
    if (DG_ScreenBuffer) {
        bao_display_present_rgba((const uint32_t *)DG_ScreenBuffer,
                                 DOOMGENERIC_RESX, DOOMGENERIC_RESY);
    }
    g_frames++;
    if ((g_frames & 0x1f) == 0) {
        gpio_toggle(BOARD_LED_PORT, BOARD_LED_PIN);
    }
}

void DG_SleepMs(uint32_t ms)
{
    delay_ms(ms);
}

uint32_t DG_GetTicksMs(void)
{
    return (uint32_t)millis();
}

int DG_GetKey(int *pressed, unsigned char *key)
{
    bao_input_poll();
    return bao_input_get_key(pressed, key);
}

void DG_SetWindowTitle(const char *title)
{
    mini_printf("title: %s\r\n", title ? title : "");
}

/* Extra libc stubs used by some translation units */
int isatty(int fd)
{
    (void)fd;
    return 1;
}

unsigned int sleep(unsigned int seconds)
{
    delay_ms(seconds * 1000u);
    return 0;
}

int usleep(unsigned int usec)
{
    delay_ms((usec + 999u) / 1000u);
    return 0;
}

int main(void)
{
    static char *argv[] = {
        "doom",
        "-iwad", "doom1.wad",
        "-mb", "1",
        "-nogui",
        NULL
    };
    const int argc = 5;

    bao_init();

    gpio_init(BOARD_LED_PORT, BOARD_LED_PIN);
    gpio_set_dir(BOARD_LED_PORT, BOARD_LED_PIN, true);
    gpio_put(BOARD_LED_PORT, BOARD_LED_PIN, true);

    mini_printf("\r\n=== baochip-doom (%s) ===\r\n", BOARD_NAME);

    if (!bao_wad_init()) {
        mini_printf("FATAL: no IWAD\r\n");
        for (;;) {
            gpio_toggle(BOARD_LED_PORT, BOARD_LED_PIN);
            delay_ms(200);
        }
    }

    doomgeneric_Create(argc, argv);

    for (;;) {
        doomgeneric_Tick();
    }
}
