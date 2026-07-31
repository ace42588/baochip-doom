#!/usr/bin/env python3
"""Flash a raw IWAD image onto W25Q SPI NOR via dabao-sdk serial tools.

This is a host-side helper outline. On-device programming can also be done with
a small UF2 that calls w25q_erase/w25q_write (see examples/w25q_flash).

Usage:
  python3 scripts/flash_wad.py --port /dev/ttyACM0 assets/doom1.wad
"""

from __future__ import annotations

import argparse
import sys


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--port", required=True, help="Serial port of Dabao/badge")
    p.add_argument("wad", help="Path to IWAD file")
    p.add_argument("--offset", type=lambda x: int(x, 0), default=0)
    args = p.parse_args()

    print("flash_wad.py: host→SPI programming is board-specific.")
    print(f"  port={args.port} wad={args.wad} offset=0x{args.offset:x}")
    print("Use Dabao W25Q example or a dedicated programmer UF2 to write the IWAD.")
    print("Then build with: make WAD_BACKEND=spi")
    return 0


if __name__ == "__main__":
    sys.exit(main())
