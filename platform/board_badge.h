/*
 * DEF CON 34 badge board pack.
 *
 * OLED (strongest candidate): SSD1327-compatible square grayscale
 *   - 1.5" 128×128 or 1.12" 96×96
 *   - I²C 0x3C or 0x3D, 4-bit grayscale (badge firmware may use B/W)
 * Alternate: SH1107 128×128 mono @ 0x3C
 * Accel: LIS2DH12 @ 0x19 (DC34 SAO note)
 */

#ifndef BOARD_BADGE_H
#define BOARD_BADGE_H

#include "boards/dabao.h"

#define BOARD_NAME              "dc34-badge"

#define BOARD_UART              BAO_DEFAULT_UART

#define BOARD_OLED_I2C          0
#define BOARD_OLED_I2C_ADDR     0x3C
#define BOARD_OLED_I2C_ADDR_ALT 0x3D
#ifndef BOARD_OLED_WIDTH
#define BOARD_OLED_WIDTH        128
#endif
#ifndef BOARD_OLED_HEIGHT
#define BOARD_OLED_HEIGHT       128
#endif
/* Prefer SSD1327 4-bit gray; override with OLED_DRIVER=sh1107 */
#ifndef BOARD_OLED_PREFER_SSD1327
#define BOARD_OLED_PREFER_SSD1327 1
#endif

/* Optional SPI SH1107 (baosec-like) if BAO_OLED_SPI is defined at build */
#define BOARD_OLED_SPI          2
#define BOARD_OLED_SPI_CD_PORT  GPIO_PORT_C
#define BOARD_OLED_SPI_CD_PIN   2
#define BOARD_OLED_SPI_CS_PORT  GPIO_PORT_C
#define BOARD_OLED_SPI_CS_PIN   3

#define BOARD_W25Q_INSTANCE     1
#define BOARD_W25Q_CS           0
#define BOARD_W25Q_CLKDIV       10
#define BOARD_WAD_SPI_OFFSET    0x000000u

#define BOARD_LED_PORT          GPIO_PORT_B
#define BOARD_LED_PIN           1

#define BOARD_HAS_KEYPAD        1
#define BOARD_KB_PORT           GPIO_PORT_A
#define BOARD_KB_ROW0           0
#define BOARD_KB_ROW1           1
#define BOARD_KB_COL0           2
#define BOARD_KB_COL1           3
#define BOARD_KB_COL2           4

#define BOARD_ACCEL_I2C_ADDR    0x19

#endif
