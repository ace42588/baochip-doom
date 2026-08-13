/*
 * LIS2DH12 accelerometer (DC34 badge, I2C0 @ 0x19).
 *
 * ACCEL=strafe: tilt left/right queues KEY_STRAFE_L / KEY_STRAFE_R.
 * ACCEL=orient: gravity selects landscape vs portrait controls + display.
 *
 * Any accel build also supports pitch-to-move: off by default, toggled at
 * runtime by a keypad chord (see bao_input.c). Enabling captures the
 * current pose as neutral; tilting forward/back from there queues
 * KEY_UPARROW / KEY_DOWNARROW.
 *
 * Axis frame: BOARD_ACCEL_LR_* points toward the badge's right edge in
 * landscape, BOARD_ACCEL_DOWN_* toward gravity when hanging in landscape.
 * BOARD_ACCEL_PORTRAIT_DIR gives the rotation that reaches portrait
 * (-1 = counter-clockwise, +1 = clockwise); in portrait gravity sits on
 * DIR·LR and the player's right is -DIR·DOWN.
 */

#include <stdint.h>
#include <stdbool.h>

#include "bao.h"
#include "board.h"
#include "bao_accel.h"
#include "bao_input.h"
#include "doomkeys.h"
#include "hardware/i2c.h"

#if defined(BAO_ACCEL_STRAFE) || defined(BAO_ACCEL_ORIENT)

#define LIS2DH12_WHO_AM_I    0x0F
#define LIS2DH12_WHO_VALUE   0x33
#define LIS2DH12_CTRL_REG1   0x20
#define LIS2DH12_CTRL_REG4   0x23
#define LIS2DH12_OUT_X_L     0x28
#define LIS2DH12_AUTO_INC    0x80

#define ACCEL_POLL_MS        20

/* Tilt thresholds in mg (1 g = 1000). Press/release gap gives hysteresis
 * so the strafe keys don't chatter around the deadzone edge. */
#define STRAFE_PRESS_MG      250
#define STRAFE_RELEASE_MG    150

/* Only one in-plane axis can exceed 750 mg at a time (0.75² · 2 > 1), so a
 * single switch threshold is naturally hysteretic: between the two poses
 * (and when flat on a table, Z dominant) the state just holds. */
#define ORIENT_SWITCH_MG     750

/* Ignore tilt briefly after an orientation flip so the rotation gesture
 * itself doesn't register as strafe or pitch movement. */
#define FLIP_INHIBIT_MS      500

/* Pitch thresholds on the cross product of the current (down, z) gravity
 * pair against the captured neutral pair: cross ≈ sin(Δpitch) · |g|² with
 * mg inputs, so 250000 ≈ 14.5° from neutral to press, 150000 ≈ 8.6° to
 * release. */
#define PITCH_PRESS_X        250000
#define PITCH_RELEASE_X      150000

/* Panel-normal axis: the one that is neither LR nor DOWN. */
#define ACCEL_Z_AXIS         (3 - BOARD_ACCEL_LR_AXIS - BOARD_ACCEL_DOWN_AXIS)

static bool s_ok;
static bool s_inited;
static uint32_t s_last_poll_ms;

static bool s_pitch_enabled;     /* runtime toggle, off at boot */
static bool s_pitch_recapture;   /* re-learn neutral on next sample */
static int32_t s_pitch_d0;       /* neutral in-plane down component (mg) */
static int32_t s_pitch_z0;       /* neutral panel-normal component (mg) */
static int s_pitch_move;         /* -1 = back held, 0 = none, +1 = forward */

#ifdef BAO_ACCEL_ORIENT
#ifdef BAO_CONTROLS_PORTRAIT
static bool s_portrait = true;   /* CONTROLS= fallback until gravity says */
#else
static bool s_portrait = false;
#endif

bool bao_controls_is_portrait(void)
{
    return s_portrait;
}
#endif

#ifdef BAO_ACCEL_STRAFE
static int s_strafe;             /* -1 = left held, 0 = none, +1 = right held */
#endif

#ifdef BAO_ACCEL_ORIENT
static bool s_flip_inhibit;
static uint32_t s_flip_ms;

static bool flip_inhibited(uint32_t now)
{
    if (!s_flip_inhibit) {
        return false;
    }
    if (now - s_flip_ms < FLIP_INHIBIT_MS) {
        return true;
    }
    s_flip_inhibit = false;
    return false;
}
#endif

static int accel_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_write_blocking(BOARD_ACCEL_I2C, BOARD_ACCEL_I2C_ADDR, buf, 2);
}

static bool accel_read_xyz(int16_t mg[3])
{
    uint8_t reg = LIS2DH12_OUT_X_L | LIS2DH12_AUTO_INC;
    uint8_t buf[6];

    if (i2c_write_read_blocking(BOARD_ACCEL_I2C, BOARD_ACCEL_I2C_ADDR,
                                &reg, 1, buf, 6) != 0) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        int16_t raw = (int16_t)((buf[2 * i + 1] << 8) | buf[2 * i]);
        mg[i] = (int16_t)(raw >> 4);  /* HR ±2g: 1 mg/LSB after shift */
    }
    return true;
}

#ifdef BAO_ACCEL_ORIENT
static void orient_update(const int16_t mg[3], uint32_t now)
{
    int lr   = BOARD_ACCEL_LR_SIGN   * mg[BOARD_ACCEL_LR_AXIS];
    int down = BOARD_ACCEL_DOWN_SIGN * mg[BOARD_ACCEL_DOWN_AXIS];
    bool portrait = s_portrait;

    if (BOARD_ACCEL_PORTRAIT_DIR * lr > ORIENT_SWITCH_MG) {
        portrait = true;             /* rotated into the portrait pose */
    } else if (down > ORIENT_SWITCH_MG) {
        portrait = false;            /* hanging landscape */
    }

    if (portrait != s_portrait) {
        s_portrait = portrait;
        s_flip_inhibit = true;
        s_flip_ms = now;
    }
}
#endif

#ifdef BAO_ACCEL_STRAFE
static void strafe_update(const int16_t mg[3], uint32_t now)
{
    int tilt;
    int want = s_strafe;

#ifdef BAO_ACCEL_ORIENT
    /* Guard: the roll gesture that flips orientation would otherwise read
     * as a hard strafe on the way in and out. Hold strafe released until
     * the badge has settled in its new pose. */
    if (flip_inhibited(now)) {
        if (s_strafe > 0) {
            bao_input_queue_key(0, KEY_STRAFE_R);
        } else if (s_strafe < 0) {
            bao_input_queue_key(0, KEY_STRAFE_L);
        }
        s_strafe = 0;
        return;
    }
#endif
    (void)now;

    /* Gravity component along the player's right in the current control
     * scheme; positive = the player's right side dipping. */
    if (bao_controls_is_portrait()) {
        tilt = -BOARD_ACCEL_PORTRAIT_DIR *
               (BOARD_ACCEL_DOWN_SIGN * mg[BOARD_ACCEL_DOWN_AXIS]);
    } else {
        tilt = BOARD_ACCEL_LR_SIGN * mg[BOARD_ACCEL_LR_AXIS];
    }

    if (s_strafe == 0) {
        if (tilt > STRAFE_PRESS_MG) {
            want = 1;
        } else if (tilt < -STRAFE_PRESS_MG) {
            want = -1;
        }
    } else if (s_strafe > 0) {
        if (tilt < STRAFE_RELEASE_MG) {
            want = 0;
        }
    } else {
        if (tilt > -STRAFE_RELEASE_MG) {
            want = 0;
        }
    }

    if (want != s_strafe) {
        if (s_strafe > 0) {
            bao_input_queue_key(0, KEY_STRAFE_R);
        } else if (s_strafe < 0) {
            bao_input_queue_key(0, KEY_STRAFE_L);
        }
        if (want > 0) {
            bao_input_queue_key(1, KEY_STRAFE_R);
        } else if (want < 0) {
            bao_input_queue_key(1, KEY_STRAFE_L);
        }
        s_strafe = want;
    }
}
#endif

/* Gravity component along the current orientation's screen-down (mg). */
static int32_t pitch_down_mg(const int16_t mg[3])
{
    if (bao_controls_is_portrait()) {
        return BOARD_ACCEL_PORTRAIT_DIR *
               (BOARD_ACCEL_LR_SIGN * mg[BOARD_ACCEL_LR_AXIS]);
    }
    return BOARD_ACCEL_DOWN_SIGN * mg[BOARD_ACCEL_DOWN_AXIS];
}

static void pitch_release(void)
{
    if (s_pitch_move > 0) {
        bao_input_queue_key(0, KEY_UPARROW);
    } else if (s_pitch_move < 0) {
        bao_input_queue_key(0, KEY_DOWNARROW);
    }
    s_pitch_move = 0;
}

static void pitch_update(const int16_t mg[3], uint32_t now)
{
    int32_t d, z, cross;
    int want = s_pitch_move;

    if (!s_pitch_enabled) {
        return;
    }

#ifdef BAO_ACCEL_ORIENT
    /* An orientation flip moves the down axis out from under the captured
     * neutral: hold movement released through the settle window, then
     * re-learn neutral in the new pose. */
    if (flip_inhibited(now)) {
        pitch_release();
        s_pitch_recapture = true;
        return;
    }
#endif
    (void)now;

    d = pitch_down_mg(mg);
    z = BOARD_ACCEL_PITCH_SIGN * mg[ACCEL_Z_AXIS];

    if (s_pitch_recapture) {
        s_pitch_recapture = false;
        s_pitch_d0 = d;
        s_pitch_z0 = z;
        pitch_release();
        return;
    }

    /* 2-D cross product of (d, z) against the neutral pair: proportional
     * to sin(pitch deviation), independent of how reclined the neutral
     * pose is. Positive = tilted back relative to neutral. */
    cross = z * s_pitch_d0 - d * s_pitch_z0;

    if (s_pitch_move == 0) {
        if (cross < -PITCH_PRESS_X) {
            want = 1;                /* tilted forward → move forward */
        } else if (cross > PITCH_PRESS_X) {
            want = -1;               /* tilted back → move back */
        }
    } else if (s_pitch_move > 0) {
        if (cross > -PITCH_RELEASE_X) {
            want = 0;
        }
    } else {
        if (cross < PITCH_RELEASE_X) {
            want = 0;
        }
    }

    if (want != s_pitch_move) {
        pitch_release();
        if (want > 0) {
            bao_input_queue_key(1, KEY_UPARROW);
        } else if (want < 0) {
            bao_input_queue_key(1, KEY_DOWNARROW);
        }
        s_pitch_move = want;
    }
}

void bao_accel_pitch_toggle(void)
{
    if (!s_ok) {
        mini_printf("accel: pitch toggle ignored (no accel)\r\n");
        return;
    }
    if (s_pitch_enabled) {
        s_pitch_enabled = false;
        s_pitch_recapture = false;
        pitch_release();
        mini_printf("accel: pitch control off\r\n");
    } else {
        /* Neutral is re-learned from the next sample every time pitch
         * control is enabled. */
        s_pitch_enabled = true;
        s_pitch_recapture = true;
        mini_printf("accel: pitch control on\r\n");
    }
}

void bao_accel_init(void)
{
    uint8_t reg = LIS2DH12_WHO_AM_I;
    uint8_t who = 0;
    int16_t mg[3];

    if (s_inited) {
        return;
    }
    s_inited = true;

    i2c_init(BOARD_ACCEL_I2C, 400000);
    if (i2c_write_read_blocking(BOARD_ACCEL_I2C, BOARD_ACCEL_I2C_ADDR,
                                &reg, 1, &who, 1) != 0 ||
        who != LIS2DH12_WHO_VALUE) {
        mini_printf("accel: WHO_AM_I=0x%02x (want 0x33), disabled\r\n", who);
        return;
    }
    accel_write_reg(LIS2DH12_CTRL_REG1, 0x47);  /* 50 Hz, X/Y/Z enable */
    accel_write_reg(LIS2DH12_CTRL_REG4, 0x88);  /* BDU + HR, ±2g */
    delay_ms(25);                               /* first 50 Hz sample */
    s_ok = true;

    if (accel_read_xyz(mg)) {
        /* Log the resting vector so a wrong BOARD_ACCEL_*_AXIS/SIGN guess
         * in board_badge.h is obvious from the console. */
        mini_printf("accel: WHO=0x33 x=%d y=%d z=%d mg\r\n",
                    mg[0], mg[1], mg[2]);
#ifdef BAO_ACCEL_ORIENT
        orient_update(mg, (uint32_t)millis());
#endif
    }
}

void bao_accel_poll(void)
{
    int16_t mg[3];
    uint32_t now;

    if (!s_ok) {
        return;
    }
    now = (uint32_t)millis();
    if (now - s_last_poll_ms < ACCEL_POLL_MS) {
        return;
    }
    s_last_poll_ms = now;

    if (!accel_read_xyz(mg)) {
        return;
    }
#ifdef BAO_ACCEL_ORIENT
    orient_update(mg, now);
#endif
#ifdef BAO_ACCEL_STRAFE
    strafe_update(mg, now);
#endif
    pitch_update(mg, now);
}

#endif /* BAO_ACCEL_STRAFE || BAO_ACCEL_ORIENT */
