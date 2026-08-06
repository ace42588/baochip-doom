/*
 * Dabao evaluation board pin map for baochip-doom.
 * Optional I2C OLED breakout on I2C0 (SSD1327 / SH1107 / SSD1306),
 * or a baosec-style SPI SH1107 when built with BAO_OLED_SPI.
 */

#ifndef BOARD_DABAO_H
#define BOARD_DABAO_H

#include "boards/dabao.h"

#define BOARD_NAME              "dabao"

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
#ifndef BOARD_OLED_PREFER_SSD1327
#define BOARD_OLED_PREFER_SSD1327 1
#endif

#define BOARD_OLED_SPI          2
#define BOARD_OLED_SPI_CD_PORT  GPIO_PORT_C
#define BOARD_OLED_SPI_CD_PIN   2
#define BOARD_OLED_SPI_CS_PORT  GPIO_PORT_C
#define BOARD_OLED_SPI_CS_PIN   3
#define BOARD_OLED_SPI_CLKDIV   24

#define BOARD_W25Q_INSTANCE     BAO_DEFAULT_QSPI_INSTANCE
#define BOARD_W25Q_CS           0
#define BOARD_W25Q_CLKDIV       10
#define BOARD_WAD_SPI_OFFSET    0x000000u

#define BOARD_LED_PORT          GPIO_PORT_B
#define BOARD_LED_PIN           1

#define BOARD_HAS_KEYPAD        0

#endif
