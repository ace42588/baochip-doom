/*
 * UART + optional baosec-style keypad → doomgeneric key queue.
 */

#include <stdint.h>
#include <stdbool.h>

#include "bao.h"
#include "board.h"
#include "bao_input.h"
#include "bao_accel.h"
#include "doomkeys.h"

#define KEYQUEUE_SIZE 32

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex;
static unsigned int s_KeyQueueReadIndex;

#if BOARD_HAS_KEYPAD
static uint8_t s_prev_keys;
static bool s_map_portrait;

/* Pitch-control toggle chord: hold for this long to toggle. */
#define KEYPAD_CHORD_MS 600
static bool s_chord_held;
static bool s_chord_fired;
static uint32_t s_chord_ms;
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
    const uint8_t rows[2] = { BOARD_KB_ROW0, BOARD_KB_ROW1 };
    const uint8_t cols[3] = { BOARD_KB_COL0, BOARD_KB_COL1, BOARD_KB_COL2 };

    /* Idle rows stay high-Z. Driving them high shorts any two keys that
     * share a column (portrait: forward + turn-right are both on col2). */
    for (int r = 0; r < 2; r++) {
        gpio_init(BOARD_KB_PORT, rows[r]);
        gpio_put(BOARD_KB_PORT, rows[r], false);
    }

    /* Columns idle high via pull-up; a pressed switch shorts them to the
     * row being scanned low. */
    for (int c = 0; c < 3; c++) {
        gpio_init(BOARD_KB_PORT, cols[c]);
        gpio_set_dir(BOARD_KB_PORT, cols[c], false);
        gpio_pull_up(BOARD_KB_PORT, cols[c]);
        gpio_set_schmitt(BOARD_KB_PORT, cols[c], true);
    }
}

/*
 * DC34 badge matrix (from dc34-core-hw / xous-core board-baosec):
 *   row0 = 3-way jog: col0=Down, col1=press(Select), col2=Up
 *   row1 = face buttons: col0=Left, col1=Right, col2=Center (middle)
 *
 * Jog press queues both FIRE (gameplay) and ENTER (menus); the unused key
 * in the respective context is ignored by the engine.
 *
 * Landscape (default): jog up/down = move, left/right face = turn, jog press
 * = fire. middle face = use.
 *
 * Portrait (CONTROLS=portrait, or at runtime with ACCEL=orient): badge held
 * 90° clockwise (long edge vertical). Jog up/down become turn right/left,
 * left/middle face = back/forward, right face = use.
 */
static void keypad_event(int row, int col, int pressed, bool portrait)
{
    if (portrait) {
        if (row == 0 && col == 0) {
            queue_key(pressed, KEY_LEFTARROW);   /* jog down → turn left */
        } else if (row == 0 && col == 1) {
            queue_key(pressed, KEY_FIRE);
            queue_key(pressed, KEY_ENTER);
        } else if (row == 0 && col == 2) {
            queue_key(pressed, KEY_RIGHTARROW);  /* jog up → turn right */
        } else if (row == 1 && col == 0) {
            queue_key(pressed, KEY_DOWNARROW);   /* left face → back */
        } else if (row == 1 && col == 1) {
            queue_key(pressed, KEY_USE);         /* right face → use */
        } else if (row == 1 && col == 2) {
            queue_key(pressed, KEY_UPARROW);     /* middle face → forward */
        }
        return;
    }
    if (row == 0 && col == 0) {
        queue_key(pressed, KEY_DOWNARROW);
    } else if (row == 0 && col == 1) {
        queue_key(pressed, KEY_FIRE);
        queue_key(pressed, KEY_ENTER);
    } else if (row == 0 && col == 2) {
        queue_key(pressed, KEY_UPARROW);
    } else if (row == 1 && col == 0) {
        queue_key(pressed, KEY_LEFTARROW);
    } else if (row == 1 && col == 1) {
        queue_key(pressed, KEY_RIGHTARROW);
    } else if (row == 1 && col == 2) {
        queue_key(pressed, KEY_USE); 
    }
}

static void keypad_poll(void)
{
    const uint8_t rows[2] = { BOARD_KB_ROW0, BOARD_KB_ROW1 };
    const uint8_t cols[3] = { BOARD_KB_COL0, BOARD_KB_COL1, BOARD_KB_COL2 };
    uint8_t now = 0;
    int bit = 0;
    bool portrait = bao_controls_is_portrait();

    if (portrait != s_map_portrait) {
        /* Orientation flipped mid-hold: release held keys under the old
         * map, then forget them so the next scan re-presses them under
         * the new map. */
        for (int b = 0; b < 6; b++) {
            if ((s_prev_keys >> b) & 1) {
                keypad_event(b / 3, b % 3, 0, s_map_portrait);
            }
        }
        s_prev_keys = 0;
        s_map_portrait = portrait;
    }

    for (int r = 0; r < 2; r++) {
        gpio_set_dir(BOARD_KB_PORT, rows[r], true);
        delay_us(1);
        for (int c = 0; c < 3; c++) {
            int down = (gpio_get(BOARD_KB_PORT, cols[c]) == false);
            if (down) {
                now |= (uint8_t)(1u << bit);
            }
            int was = (s_prev_keys >> bit) & 1;
            if (down != was) {
                keypad_event(r, c, down, s_map_portrait);
            }
            bit++;
        }
        gpio_set_dir(BOARD_KB_PORT, rows[r], false);
        delay_us(1);
    }
    s_prev_keys = now;

    /* Pitch-control toggle: hold the opposing-pair chord for a moment.
     * Landscape: turn left + turn right (left + right face buttons).
     * Portrait: forward + back (middle + left face buttons).
     * Bit index is row*3 + col. */
    {
        uint8_t chord = s_map_portrait
                            ? (uint8_t)((1u << 5) | (1u << 3))
                            : (uint8_t)((1u << 4) | (1u << 3));
        if ((now & chord) == chord) {
            uint32_t t = (uint32_t)millis();
            if (!s_chord_held) {
                s_chord_held = true;
                s_chord_ms = t;
            } else if (!s_chord_fired && t - s_chord_ms >= KEYPAD_CHORD_MS) {
                s_chord_fired = true;
                bao_accel_pitch_toggle();
            }
        } else {
            s_chord_held = false;
            s_chord_fired = false;
        }
    }
}
#endif

void bao_input_init(void)
{
    s_KeyQueueWriteIndex = 0;
    s_KeyQueueReadIndex = 0;
    bao_accel_init();
#if BOARD_HAS_KEYPAD
    keypad_init();
    s_prev_keys = 0;
    s_map_portrait = bao_controls_is_portrait();
#endif
#ifdef BAO_ACCEL_ORIENT
    mini_printf("controls: accel orient (%s)\r\n",
                bao_controls_is_portrait() ? "portrait" : "landscape");
#else
    mini_printf("controls: %s\r\n",
                bao_controls_is_portrait() ? "portrait" : "landscape");
#endif
#ifdef BAO_ACCEL_STRAFE
    mini_printf("controls: accel strafe\r\n");
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
    bao_accel_poll();
}

void bao_input_queue_key(int pressed, unsigned char key)
{
    queue_key(pressed, key);
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
