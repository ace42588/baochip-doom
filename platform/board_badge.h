/*
 * DEF CON 34 badge board pack.
 *
 * Hardware and firmware references (this is baosec-lite, not the Dabao eval board):
 *   - https://github.com/bunnie/dc34-core-hw          KiCad: defcon-34-v3
 *   - https://baochip.github.io/baochip-1x/            Baochip-1x book / SoC docs
 *   - https://github.com/bunnie/dc34-console           stock Xous firmware (board-baosec)
 *   - https://github.com/bunnie/dc34-api
 *   - https://github.com/bunnie/dc34-vault
 *   - https://github.com/betrusted-io/xous-core/       boot1, SH1107 driver, memory map
 *
 * The badge is a "baosec-lite" style design:
 *   OLED     CH112OL001A module, SH1107-class, 128x128 1-bit, 4-wire SPI on SPIM2:
 *            PC0=SCK (AF2), PC1=MOSI (AF2), PC2=C/D (GPIO), PC3=CSN0 (AF2).
 *            VOLED boost (MT3608) enabled by OLED_PON = PC4 (active high).
 *            Panel reset rides the JTAG TRST_PRST net (RC pull) - no GPIO needed.
 *   Keypad   Matrix on port F: rows PF6 (KB_R0) / PF7 (KB_R1, shared with JTAG TDI),
 *            columns PF2/PF3/PF4 (KB_C0..C2), switch closes row to column.
 *            Populated keys: row0 = 3-way jog (Down / press-Select / Up),
 *            row1 = Left (col0), Right (col1), Center/middle (col2) face buttons.
 *   Accel    LIS2DH12 @ 0x19 on I2C0 (PB11 SCL / PB12 SDA).
 *   NOR      SPI flash on QSPI2 pins PC7..PC13 (SDK w25q instance 1).
 *   LEDs     WS2812 chain on PB15 (BIO-driven in stock firmware; unused here).
 *   UART2    console PB13=RX / PB14=TX (CON_FROM_HOST / CON_TO_HOST).
 */

#ifndef BOARD_BADGE_H
#define BOARD_BADGE_H

#include "boards/dabao.h"

#define BOARD_NAME              "dc34-badge"

#define BOARD_UART              BAO_DEFAULT_UART

/* Display: fixed SPI SH1107, no I2C probing on the badge */
#define BOARD_OLED_SPI_ONLY     1
#define BOARD_OLED_SPI          2
#define BOARD_OLED_SPI_CD_PORT  GPIO_PORT_C
#define BOARD_OLED_SPI_CD_PIN   2
#define BOARD_OLED_SPI_CS_PORT  GPIO_PORT_C
#define BOARD_OLED_SPI_CS_PIN   3
#define BOARD_OLED_PON_PORT     GPIO_PORT_C
#define BOARD_OLED_PON_PIN      4
/* SPI clock = perclk / (2*(clkdiv+1)); 24 -> ~2 MHz at 100 MHz perclk,
 * matching the stock xous sh1107 driver's 2 MHz. */
#define BOARD_OLED_SPI_CLKDIV   24
#ifndef BOARD_OLED_WIDTH
#define BOARD_OLED_WIDTH        128
#endif
#ifndef BOARD_OLED_HEIGHT
#define BOARD_OLED_HEIGHT       128
#endif

/* I2C0 carries the LIS2DH12 accelerometer (ACCEL=strafe|orient builds) */
#define BOARD_OLED_I2C          0
#define BOARD_ACCEL_I2C         0
#define BOARD_ACCEL_I2C_ADDR    0x19

/* LIS2DH12 axis mapping in the landscape badge frame: LR points toward the
 * badge's right edge, DOWN toward gravity when the badge hangs in landscape.
 * PORTRAIT_DIR is the rotation that reaches portrait: -1 = 90° counter-
 * clockwise (left edge down), +1 = 90° clockwise (right edge down).
 * Verified on hardware: X reads +1g held upright in landscape (so X = DOWN)
 * and pitch was bleeding into strafe when X was assumed to be LR. Y is the
 * left/right axis; its sign and PORTRAIT_DIR are still best guesses — if
 * tilt or auto-rotation is backwards, check the boot-time "accel:" log line
 * and flip the constants here. Axis index: 0 = X, 1 = Y, 2 = Z. */
#define BOARD_ACCEL_LR_AXIS      1
#define BOARD_ACCEL_LR_SIGN      (-1)
#define BOARD_ACCEL_DOWN_AXIS    0
#define BOARD_ACCEL_DOWN_SIGN    1
#define BOARD_ACCEL_PORTRAIT_DIR (-1)

#define BOARD_W25Q_INSTANCE     1
#define BOARD_W25Q_CS           0
#define BOARD_W25Q_CLKDIV       10
#define BOARD_WAD_SPI_OFFSET    0x000000u

/* No plain GPIO LED on the badge (LEDs are WS2812 on PB15); PB1 is unconnected
 * on this board, so the heartbeat toggle is harmless. */
#define BOARD_LED_PORT          GPIO_PORT_B
#define BOARD_LED_PIN           1

#define BOARD_HAS_KEYPAD        1
#define BOARD_KB_PORT           GPIO_PORT_F
#define BOARD_KB_ROW0           6
#define BOARD_KB_ROW1           7
#define BOARD_KB_COL0           2
#define BOARD_KB_COL1           3
#define BOARD_KB_COL2           4

#endif
