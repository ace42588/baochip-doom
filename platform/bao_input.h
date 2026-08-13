#ifndef BAO_INPUT_H
#define BAO_INPUT_H

#include <stdint.h>
#include <stdbool.h>

void bao_input_init(void);

/* Poll UART + optional keypad into the DG key queue. */
void bao_input_poll(void);

/* doomgeneric queue API used by i_input.c */
int bao_input_get_key(int *pressed, unsigned char *key);

/* Inject a key event from another driver (accelerometer strafe). */
void bao_input_queue_key(int pressed, unsigned char key);

#endif
