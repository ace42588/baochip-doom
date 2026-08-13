# baochip-doom

Bare-metal [DOOM](https://github.com/ozkl/doomgeneric) for the **DEF CON 34 badge**
(Baochip-1x / baosec-lite), built on [dabao-sdk](https://github.com/bunnie/dabao-sdk).

This replaces the stock Xous badge firmware. Loading unsigned/developer firmware
clears device secrets (light-gene keys, PDDB) See [dc34-vault](https://github.com/bunnie/dc34-vault).

The [Dabao](https://www.crowdsupply.com/baochip/dabao) eval board is a secondary
target (`BOARD=dabao`).

## Quick start

```sh
git submodule update --init --recursive

# Toolchain into third_party/dabao-sdk/ (macOS arm64 example):
# https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases

# Trimmed IWAD required for the default RRAM build (~1.4 MiB).
# Full shareware (~4 MiB) will not fit with the engine.
# https://github.com/fragglet/squashware is one that fits.
cp /path/to/doom1.wad assets/doom1.wad

make                         # DC34 badge: tilt strafe + auto orientation (default)
# make ACCEL=off             # buttons only, CONTROLS= picks the orientation
# make CONTROLS=portrait     # starting orientation (fixed when ACCEL=off|strafe)
# make BOARD=dabao           # Dabao + I²C OLED breakout (no accelerometer)
# → build/doom.uf2
```

### Flash (badge)

Hold any face button while plugging in USB (or while resetting) to enter mass-storage mode (`BAOCHIP` volume):

1. Copy `build/doom.uf2` onto `BAOCHIP`. The OLED shows write progress
   (`loader - NNk`, then `kernel - NNk`) — that is boot1 programming RRAM, not
   DOOM.
2. Eject the volume (or `sync`).
3. Press any face button to leave mass-storage mode and run the image.

You should see **`DC34 DOOM` / `loading`**, then engine init (`Z_Init`,
`W_Init`, `R_Init`, …). `R_Init` can take a while. The title screen appears once
the first frame is drawn.

If you still see the bao splash / `kernel - NNk`, boot1 never jumped — eject and
press any face button again. Unplugging alone may not boot while bootwait is on.

Serial (board already in boot1 REPL): `make flash PORT=/dev/ttyACM0`.
Console: UART2 **115200 8N1**.

## Controls

On the badge the LIS2DH12 accelerometer is enabled by default (`ACCEL=both`):

- Tilt the badge left/right (~15°) to strafe.
- Rotate the badge between landscape and portrait and the keypad map and
  OLED rotation follow. Strafe is paused for half a second after a flip so
  the rotation gesture itself doesn't strafe.
- Pitch-to-move, off by default: hold both turn buttons (landscape) or the
  forward + back buttons (portrait) for ~0.6 s to toggle it. The pose held
  at that moment becomes neutral; tilt ~15° forward/back from there to
  move. Every enable re-captures neutral, and an orientation flip re-learns
  it automatically. Available in any accelerometer build.

`CONTROLS=landscape` (default) or `CONTROLS=portrait` sets the starting
orientation — the fixed orientation when auto-rotation is off, otherwise
just the fallback until gravity says (badge flat, chip missing).

Other accelerometer builds: `ACCEL=strafe` (tilt strafe only, fixed
orientation), `ACCEL=orient` (auto-orientation only), `ACCEL=off` (buttons
only; always the case on Dabao, which has no accelerometer). At boot the
console prints the resting gravity vector (`accel: WHO=0x33 x=.. y=.. z=..`);
if tilt, pitch, or auto-rotation is backwards, flip the `BOARD_ACCEL_*`
axis/sign constants in `platform/board_badge.h`.

### Landscape (default)

| Key | Action |
|-----|--------|
| Jog up / down | Move forward / back |
| Left / Right face buttons | Turn |
| Jog press | Fire (also Enter in menus) |

### Portrait (rotate the badge, or `make CONTROLS=portrait`)

Badge rotated 90° counter-clockwise (long edge vertical). The OLED is rotated
to match.

| Key | Action |
|-----|--------|
| Jog wheel | Turn left / right |
| Left face button | Move back |
| Middle face button | Move forward |
| Right face button | Use (open doors) |
| Jog press | Fire (also Enter in menus) |

### UART (either board)

| Key | Action |
|-----|--------|
| WASD | Move / turn |
| J / Z | Fire |
| Space | Use (open doors) |
| Enter | Menu select |
| Esc / Q | Escape |

## WAD backends

| Mode | Build | Notes |
|------|-------|--------|
| Embedded RRAM | `WAD_BACKEND=embedded` (default) | Links `assets/doom1.wad` into the UF2. Needs a trimmed IWAD ([squashware](https://github.com/fragglet/squashware) works). |
| SPI NOR | `WAD_BACKEND=spi` | Full `doom1.wad` on QSPI2 NOR; see `scripts/flash_wad.py`. On the badge this erases the stock PDDB area. |

## Build options (Dabao)

For the eval board: `make BOARD=dabao`. Optional OLED overrides:
`OLED_W` / `OLED_H`, `OLED_DRIVER=spi`. Headless (no OLED) still runs.

## References

| Resource | What it is |
|----------|------------|
| [dc34-core-hw](https://github.com/bunnie/dc34-core-hw) | Badge schematics |
| [Baochip-1x book](https://baochip.github.io/baochip-1x/) | SoC docs (memory map, boot, RRAM) |
| [dc34-console](https://github.com/bunnie/dc34-console) | Stock Xous firmware |
| [xous-core](https://github.com/betrusted-io/xous-core/) | boot1, SH1107 driver, UF2 / loader slots |
| [dc34-vault](https://github.com/bunnie/dc34-vault) | Vault / secrets model |
| [dc34-api](https://github.com/bunnie/dc34-api) | Shared IPC/API crate |
| [dc34-bio](https://github.com/bunnie/dc34-bio) | BIO uploader for stock firmware |

## License

- Engine / doomgeneric: **GPL-2.0**
- dabao-sdk: **Apache-2.0**
- Do not redistribute commercial IWADs
