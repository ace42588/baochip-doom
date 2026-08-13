#ifndef BAO_ACCEL_H
#define BAO_ACCEL_H

#include <stdbool.h>

#if defined(BAO_ACCEL_STRAFE) || defined(BAO_ACCEL_ORIENT)
/* Probe + configure the LIS2DH12. Idempotent; safe to call from both
 * display and input init. */
void bao_accel_init(void);

/* Sample gravity and update strafe keys / orientation. Rate-limited
 * internally; cheap to call from every input poll. */
void bao_accel_poll(void);

/* Toggle pitch-to-move (forward/back from tilt). Off by default; each
 * enable re-captures the current pose as the neutral position. Wired to
 * a keypad chord in bao_input.c. */
void bao_accel_pitch_toggle(void);
#else
static inline void bao_accel_init(void) {}
static inline void bao_accel_poll(void) {}
static inline void bao_accel_pitch_toggle(void) {}
#endif

/*
 * Current control/display orientation. Runtime (gravity-driven) when built
 * with ACCEL=orient, otherwise the compile-time CONTROLS= scheme, which
 * lets the compiler fold the orientation branches away.
 */
#ifdef BAO_ACCEL_ORIENT
bool bao_controls_is_portrait(void);
#else
static inline bool bao_controls_is_portrait(void)
{
#ifdef BAO_CONTROLS_PORTRAIT
    return true;
#else
    return false;
#endif
}
#endif

#endif
