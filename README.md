# baochip-doom

Bare-metal [DOOM](https://github.com/ozkl/doomgeneric) for the **Baochip-1x** (DEF CON 34 badge / Dabao), built on [dabao-sdk](https://github.com/bunnie/dabao-sdk).

## Hardware

| Item | Notes |
|------|--------|
| SoC | Baochip-1x — 350 MHz Vexriscv, 2 MiB SRAM, 4 MiB RRAM |
| Dev board | [Dabao](https://www.crowdsupply.com/baochip/dabao) |
| Target badge | DC34 Baochip module (SSD1327 square OLED @ I²C `0x3C`/`0x3D`, keypad, SPI NOR) |

## Quick start

```sh
git submodule update --init --recursive

# Toolchain (macOS arm64 example) into third_party/dabao-sdk/
# https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases

# Place shareware / trimmed IWAD
cp /path/to/doom1.wad assets/doom1.wad

make BOARD=dabao WAD_BACKEND=embedded
# → build/doom.uf2
```

Flash (board in boot1 REPL):

```sh
make flash PORT=/dev/ttyACM0
# or persistent demo:
make flash-persistent PORT=/dev/ttyACM0
```

Serial console: UART2 **115200 8N1** (Dabao PB14=TX, PB13=RX).

### Controls (UART)

| Key | Action |
|-----|--------|
| WASD | Move / turn |
| J / Z | Fire |
| Space | Use |
| Enter | Menu select |
| Esc / Q | Escape |

With `BOARD=badge`, a baosec-style keypad is also polled (see `platform/board_badge.h`).

## Display

`DG_DrawFrame` scales the 320×200 RGBA buffer to the OLED:

| Controller | Mode | Notes |
|------------|------|--------|
| **SSD1327** (preferred) | 4-bit grayscale | 1.5″ 128×128 or 1.12″ 96×96; I²C `0x3C` or `0x3D` |
| SH1107 | 1-bit + Bayer dither | Square mono fallback @ `0x3C` |
| SPI SH1107 | 1-bit | baosec-like wiring (`BAO_OLED_SPI`) |

Probe order: `0x3C` then `0x3D`. Override size with `OLED_W=96 OLED_H=96`. Headless (no OLED) still runs.

## WAD storage

| Mode | Build | Notes |
|------|-------|--------|
| Embedded RRAM | `WAD_BACKEND=embedded` (default) | Link `assets/doom1.wad` into UF2. Use a trimmed/silent IWAD (~1.4 MiB). Full 4 MiB IWAD will not fit with the engine. |
| SPI NOR | `WAD_BACKEND=spi` | Full `doom1.wad` on W25Q; see `scripts/flash_wad.py` |

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

Loading unsigned/developer firmware on Baochip clears device secrets (see Xous/Baochip security model). Expected for hobby badge demos.
