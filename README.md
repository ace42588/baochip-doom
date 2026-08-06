# baochip-doom

Bare-metal [DOOM](https://github.com/ozkl/doomgeneric) for the **Baochip-1x** (DEF CON 34 badge / Dabao), built on [dabao-sdk](https://github.com/bunnie/dabao-sdk).

## Hardware

| Item | Notes |
|------|--------|
| SoC | Baochip-1x — 350 MHz Vexriscv, 2 MiB SRAM, 4 MiB RRAM |
| Dev board | [Dabao](https://www.crowdsupply.com/baochip/dabao) |
| Target badge | DC34 badge — see [dc34-core-hw](https://github.com/bunnie/dc34-core-hw) schematics |

Badge hardware (confirmed from [dc34-core-hw](https://github.com/bunnie/dc34-core-hw) and the stock
[dc34-console](https://github.com/bunnie/dc34-console) firmware, a Xous build with `board-baosec`):

| Peripheral | Details |
|------------|---------|
| OLED | CH112OL001A (SH1107-class) 128×128 1-bit, SPI2: PC0 SCK / PC1 MOSI / PC2 C/D / PC3 CS, VOLED boost enable PC4 |
| Keypad | Matrix rows PF6/PF7 × cols PF2/PF3/PF4: 3-way jog (up/down/press) + Left/Right buttons |
| Accelerometer | LIS2DH12 @ I²C `0x19` (PB11/PB12) |
| SPI NOR | QSPI2 on PC7–PC13 |
| LEDs | WS2812 chain on PB15 (BIO-driven; unused by this port) |
| Console | UART2 PB13=RX / PB14=TX, 115200 8N1 |

Related badge repos: [dc34-api](https://github.com/bunnie/dc34-api) (shared IPC/API crate),
[dc34-bio](https://github.com/bunnie/dc34-bio) (BIO program uploader for the stock firmware).

## Quick start

```sh
git submodule update --init --recursive

# Toolchain (macOS arm64 example) into third_party/dabao-sdk/
# https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases

# Place shareware / trimmed IWAD
cp /path/to/doom1.wad assets/doom1.wad

make BOARD=badge WAD_BACKEND=embedded    # DC34 badge
# or: make BOARD=dabao                   # Dabao dev board + I2C OLED breakout
# → build/doom.uf2
```

Flash (board in boot1 REPL):

```sh
make flash PORT=/dev/ttyACM0
# or persistent demo:
make flash-persistent PORT=/dev/ttyACM0
```

Serial console: UART2 **115200 8N1** (PB14=TX, PB13=RX).

### Controls

Badge keypad (`BOARD=badge`):

| Key | Action |
|-----|--------|
| Jog up / down | Move forward / back |
| Left / Right buttons | Turn |
| Jog press | Fire (also Enter in menus) |

UART (both boards):

| Key | Action |
|-----|--------|
| WASD | Move / turn |
| J / Z | Fire |
| Space | Use (open doors) |
| Enter | Menu select |
| Esc / Q | Escape |

## Display

`DG_DrawFrame` scales the 320×200 RGBA buffer to the OLED:

| Board | Panel | Mode |
|-------|-------|------|
| `badge` | SPI SH1107 128×128 (fixed) | 1-bit + Bayer dither, init/data layout matches the stock xous `sh1107.rs` driver |
| `dabao` | I²C breakout, probed `0x3C` then `0x3D` | SSD1327 4-bit grayscale preferred, SH1107/SSD1306 1-bit fallback; `OLED_DRIVER=spi` for baosec-style SPI wiring |

Dev-board panel size override: `OLED_W=96 OLED_H=96`. Headless (no OLED) still runs.

## WAD storage

| Mode | Build | Notes |
|------|-------|--------|
| Embedded RRAM | `WAD_BACKEND=embedded` (default) | Link `assets/doom1.wad` into UF2. Use a trimmed/silent IWAD (~1.4 MiB). Full 4 MiB IWAD will not fit with the engine. |
| SPI NOR | `WAD_BACKEND=spi` | Full `doom1.wad` on the QSPI2 NOR; see `scripts/flash_wad.py`. On the badge this area also holds the stock firmware's PDDB — expect to erase it. |

## Layout

```
platform/          DG_* + libc stubs + OLED + input + WAD
third_party/dabao-sdk/
third_party/doomgeneric/
assets/doom1.wad   (not committed)
build/doom.uf2
```

## License

- Engine / doomgeneric: **GPL-2.0**
- dabao-sdk: **Apache-2.0**
- Do not redistribute commercial IWADs

## Security note

The stock DC34 badge firmware is a Xous build ([dc34-console](https://github.com/bunnie/dc34-console)); this port replaces it. Loading unsigned/developer firmware on Baochip clears device secrets (see Xous/Baochip security model) — your badge's light-gene keys and PDDB contents will be lost. Expected for hobby badge demos.
