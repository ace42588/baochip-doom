/*
 * UART + optional baosec-style keypad → doomgeneric key queue.
 */

#include <stdint.h>
#include <stdbool.h>

#include "bao.h"
#include "board.h"
#include "bao_input.h"
#include "doomkeys.h"

#define KEYQUEUE_SIZE 32

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex;
static unsigned int s_KeyQueueReadIndex;

#if BOARD_HAS_KEYPAD
static uint8_t s_prev_keys;
#endif

static void queue_key(int pressed, unsigned char key)
{
    unsigned int next = (s_KeyQueueWriteIndex + 1) % KEYQUEUE_SIZE;
    if (next == s_KeyQueueReadIndex) {
        return;
    }
    s_KeyQueue[s_KeyQueueWriteIndex] = (unsigned short)((pressed ? 1u : 0u) << 8) | key;
    s_KeyQueueWriteIndex = next;
}

static unsigned char map_uart_char(char c)
{
    switch (c) {
    case '\r':
    case '\n':
        return KEY_ENTER;
    case 27:
        return KEY_ESCAPE;
    case 'w':
    case 'W':
        return KEY_UPARROW;
    case 's':
    case 'S':
        return KEY_DOWNARROW;
    case 'a':
    case 'A':
        return KEY_LEFTARROW;
    case 'd':
    case 'D':
        return KEY_RIGHTARROW;
    case ' ':
        return KEY_USE;
    case 'j':
    case 'J':
    case 'z':
    case 'Z':
        return KEY_FIRE;
    case 'q':
    case 'Q':
        return KEY_ESCAPE;
    default:
        return (unsigned char)c;
    }
}

#if BOARD_HAS_KEYPAD
static void keypad_init(void)
{
    gpio_init(BOARD_KB_PORT, BOARD_KB_ROW0);
    gpio_init(BOARD_KB_PORT, BOARD_KB_ROW1);
    gpio_set_dir(BOARD_KB_PORT, BOARD_KB_ROW0, true);
    gpio_set_dir(BOARD_KB_PORT, BOARD_KB_ROW1, true);
    gpio_put(BOARD_KB_PORT, BOARD_KB_ROW0, true);
    gpio_put(BOARD_KB_PORT, BOARD_KB_ROW1, true);

    gpio_init(BOARD_KB_PORT, BOARD_KB_COL0);
    gpio_init(BOARD_KB_PORT, BOARD_KB_COL1);
    gpio_init(BOARD_KB_PORT, BOARD_KB_COL2);
    gpio_set_dir(BOARD_KB_PORT, BOARD_KB_COL0, false);
    gpio_set_dir(BOARD_KB_PORT, BOARD_KB_COL1, false);
    gpio_set_dir(BOARD_KB_PORT, BOARD_KB_COL2, false);
}

static unsigned char keypad_code(int row, int col)
{
    /* baosec-style 2x3 matrix → Doom keys */
    if (row == 1 && col == 2) {
        return KEY_LEFTARROW;
    }
    if (row == 1 && col == 1) {
        return KEY_ENTER;
    }
    if (row == 1 && col == 0) {
        return KEY_RIGHTARROW;
    }
    if (row == 0 && col == 0) {
        return KEY_DOWNARROW;
    }
    if (row == 0 && col == 2) {
        return KEY_UPARROW;
    }
    if (row == 0 && col == 1) {
        return KEY_FIRE;
    }
    return 0;
}

static void keypad_poll(void)
{
    const uint8_t rows[2] = { BOARD_KB_ROW0, BOARD_KB_ROW1 };
    const uint8_t cols[3] = { BOARD_KB_COL0, BOARD_KB_COL1, BOARD_KB_COL2 };
    uint8_t now = 0;
    int bit = 0;

    for (int r = 0; r < 2; r++) {
        gpio_put(BOARD_KB_PORT, rows[r], false);
        for (int c = 0; c < 3; c++) {
            int down = (gpio_get(BOARD_KB_PORT, cols[c]) == false);
            if (down) {
                now |= (uint8_t)(1u << bit);
            }
            int was = (s_prev_keys >> bit) & 1;
            if (down && !was) {
                unsigned char k = keypad_code(r, c);
                if (k) {
                    queue_key(1, k);
                }
            } else if (!down && was) {
                unsigned char k = keypad_code(r, c);
                if (k) {
                    queue_key(0, k);
                }
            }
            bit++;
        }
        gpio_put(BOARD_KB_PORT, rows[r], true);
    }
    s_prev_keys = now;
}
#endif

void bao_input_init(void)
{
    s_KeyQueueWriteIndex = 0;
    s_KeyQueueReadIndex = 0;
#if BOARD_HAS_KEYPAD
    keypad_init();
    s_prev_keys = 0;
#endif
}

void bao_input_poll(void)
{
    while (uart_is_readable(BOARD_UART)) {
        char c = uart_getc(BOARD_UART);
        if (c) {
            queue_key(1, map_uart_char(c));
            queue_key(0, map_uart_char(c));
        }
    }
#if BOARD_HAS_KEYPAD
    keypad_poll();
#endif
}

int bao_input_get_key(int *pressed, unsigned char *key)
{
    unsigned short e;
    if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex) {
        return 0;
    }
    e = s_KeyQueue[s_KeyQueueReadIndex];
    s_KeyQueueReadIndex = (s_KeyQueueReadIndex + 1) % KEYQUEUE_SIZE;
    *pressed = (e >> 8) & 1;
    *key = (unsigned char)(e & 0xff);
    return 1;
}
