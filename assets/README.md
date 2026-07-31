# Game assets

Place a Doom IWAD here as `doom1.wad` (not committed).

## Options

1. **Trimmed / silent shareware** (~1.4 MiB) — fits in on-chip RRAM with the engine.  
   Example: copy a silent IWAD to `assets/doom1.wad`.

2. **Full `doom1.wad`** (~4.0 MiB) — too large for RRAM alongside the engine.  
   Flash it to external W25Q SPI NOR and build with `WAD_BACKEND=spi`:

   ```sh
   make WAD_BACKEND=spi
   # Then program the WAD to flash offset 0 (see scripts/flash_wad.py)
   ```

Do **not** redistribute commercial IWADs. Shareware Doom (`doom1.wad`) is freely redistributable from id Software.
