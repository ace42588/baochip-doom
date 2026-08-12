# Top-level build for baochip-doom (dabao-sdk + doomgeneric)

ROOT        := $(abspath .)
SDK         := $(ROOT)/third_party/dabao-sdk
DG          := $(ROOT)/third_party/doomgeneric/doomgeneric
PLATFORM    := $(ROOT)/platform
BUILD       := $(ROOT)/build
ASSETS      := $(ROOT)/assets

# Toolchain: prefer SDK-local xpack, else PATH
XPACK_GCC := $(firstword $(wildcard $(SDK)/xpack-riscv-none-elf-gcc-*/bin/riscv-none-elf-gcc))
ifeq ($(XPACK_GCC),)
  CROSS := riscv-none-elf-
else
  CROSS := $(dir $(XPACK_GCC))riscv-none-elf-
endif

CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
SIZE    := $(CROSS)size

ARCH    := -march=rv32imac_zicsr_zifencei -mabi=ilp32
CFLAGS  := $(ARCH) -Os -g -Wall -Wno-unused -Wno-maybe-uninitialized \
           -ffreestanding -fno-builtin -fno-common -nostdinc \
           -DNORMALUNIX -D_DEFAULT_SOURCE \
           -DDOOMGENERIC_RESX=320 -DDOOMGENERIC_RESY=200 \
           -DSEVS_CRASH_RECORD_ADDR=0x5001FF00U

# Board: badge (default, DC34) or dabao
BOARD ?= badge
ifeq ($(BOARD),badge)
  CFLAGS += -DBOARD_BADGE
endif

# WAD backend: embedded (default) or spi
WAD_BACKEND ?= embedded
ifeq ($(WAD_BACKEND),spi)
  CFLAGS += -DWAD_BACKEND_SPI
endif

# OLED size override (default from board_*.h: 128×128)
# BOARD=badge is always the fixed 128×128 SPI SH1107; these only apply to
# I2C breakouts on the Dabao dev board.
ifdef OLED_W
  CFLAGS += -DBOARD_OLED_WIDTH=$(OLED_W)
endif
ifdef OLED_H
  CFLAGS += -DBOARD_OLED_HEIGHT=$(OLED_H)
endif

# Dabao dev-board OLED driver: ssd1327 (default) | sh1107 (I2C) | spi (baosec-style SH1107)
OLED_DRIVER ?= ssd1327
ifeq ($(OLED_DRIVER),sh1107)
  CFLAGS += -DBOARD_OLED_PREFER_SSD1327=0
endif
ifeq ($(OLED_DRIVER),spi)
  CFLAGS += -DBAO_OLED_SPI
endif

INC := \
  -I$(PLATFORM)/include \
  -I$(shell $(CC) -print-file-name=include) \
  -I$(PLATFORM) \
  -I$(DG) \
  -I$(SDK)/src/common/bao_base/include \
  -I$(SDK)/src/common/bao_stdlib/include \
  -I$(SDK)/src/bao1x/hardware_regs/include \
  -I$(SDK)/src/bao1x/hardware_gpio/include \
  -I$(SDK)/src/bao1x/hardware_uart/include \
  -I$(SDK)/src/bao1x/hardware_pwm/include \
  -I$(SDK)/src/bao1x/hardware_spi/include \
  -I$(SDK)/src/bao1x/hardware_i2c/include \
  -I$(SDK)/src/bao1x/hardware_adc/include \
  -I$(SDK)/src/bao1x/hardware_trng/include \
  -I$(SDK)/src/bao1x/hardware_wdt/include \
  -I$(SDK)/src/bao1x/hardware_rtc/include \
  -I$(SDK)/src/bao1x/hardware_bio/include \
  -I$(SDK)/src/bao1x/hardware_timer/include \
  -I$(SDK)/src/bao1x/hardware_bio_dma/include \
  -I$(SDK)/src/bao1x/hardware_irq/include \
  -I$(SDK)/src/bao1x/hardware_rram/include \
  -I$(SDK)/src/bao1x/hardware_aes/include \
  -I$(SDK)/src/bao1x/hardware_sha/include \
  -I$(SDK)/src/bao1x/hardware_qspi/include \
  -I$(SDK)/src/bao1x/hardware_w25q/include \
  -I$(SDK)/src/bao1x/hardware_reset/include \
  -I$(SDK)/src/boards/include \
  -I$(SDK)/src/sevs

SDK_SRCS := \
  $(SDK)/src/bao1x/hardware_gpio/gpio.c \
  $(SDK)/src/bao1x/hardware_uart/uart.c \
  $(SDK)/src/bao1x/hardware_pwm/pwm.c \
  $(SDK)/src/bao1x/hardware_spi/spi.c \
  $(SDK)/src/bao1x/hardware_i2c/i2c.c \
  $(SDK)/src/bao1x/hardware_adc/adc.c \
  $(SDK)/src/bao1x/hardware_trng/trng.c \
  $(SDK)/src/bao1x/hardware_wdt/wdt.c \
  $(SDK)/src/bao1x/hardware_rtc/rtc.c \
  $(SDK)/src/bao1x/hardware_bio/bio.c \
  $(SDK)/src/bao1x/hardware_bio_dma/bio_dma.c \
  $(SDK)/src/bao1x/hardware_irq/irq.c \
  $(SDK)/src/bao1x/hardware_rram/rram.c \
  $(SDK)/src/bao1x/hardware_aes/aes.c \
  $(SDK)/src/bao1x/hardware_sha/sha.c \
  $(SDK)/src/bao1x/hardware_qspi/qspi.c \
  $(SDK)/src/bao1x/hardware_w25q/w25q.c \
  $(SDK)/src/common/bao_stdlib/stdio.c \
  $(SDK)/src/common/bao_stdlib/delay.c \
  $(SDK)/src/common/bao_stdlib/stdlib.c \
  $(SDK)/src/sevs/sevs_assert_target.c

DG_SRCS := \
  dummy.c am_map.c doomdef.c doomstat.c dstrings.c d_event.c d_items.c \
  d_iwad.c d_loop.c d_main.c d_mode.c d_net.c f_finale.c f_wipe.c g_game.c \
  hu_lib.c hu_stuff.c info.c i_cdmus.c i_endoom.c i_joystick.c i_scale.c \
  i_sound.c i_system.c i_timer.c memio.c m_argv.c m_bbox.c m_cheat.c \
  m_config.c m_controls.c m_fixed.c m_menu.c m_misc.c m_random.c \
  p_ceilng.c p_doors.c p_enemy.c p_floor.c p_inter.c p_lights.c p_map.c \
  p_maputl.c p_mobj.c p_plats.c p_pspr.c p_saveg.c p_setup.c p_sight.c \
  p_spec.c p_switch.c p_telept.c p_tick.c p_user.c r_bsp.c r_data.c \
  r_draw.c r_main.c r_plane.c r_segs.c r_sky.c r_things.c sha1.c sounds.c \
  statdump.c st_lib.c st_stuff.c s_sound.c tables.c v_video.c wi_stuff.c \
  w_checksum.c w_file.c w_main.c w_wad.c z_zone.c i_input.c i_video.c \
  doomgeneric.c

PLATFORM_SRCS := \
  $(PLATFORM)/bao_doom.c \
  $(PLATFORM)/bao_libc.c \
  $(PLATFORM)/bao_wad.c \
  $(PLATFORM)/bao_display.c \
  $(PLATFORM)/bao_input.c

SDK_OBJS := $(patsubst $(SDK)/%.c,$(BUILD)/sdk/%.o,$(SDK_SRCS))
DG_OBJS  := $(patsubst %.c,$(BUILD)/dg/%.o,$(DG_SRCS))
PL_OBJS  := $(patsubst $(PLATFORM)/%.c,$(BUILD)/pl/%.o,$(PLATFORM_SRCS))

CRT0_OBJ := $(BUILD)/crt0.o
WAD_OBJ  :=

ifneq ($(WAD_BACKEND),spi)
  WAD_OBJ := $(BUILD)/doom1_wad.o
endif

TARGET := $(BUILD)/doom
UF2    := $(BUILD)/doom.uf2

.PHONY: all clean size flash help

all: $(UF2)

help:
	@echo "Targets: all clean size flash"
	@echo "Vars: BOARD=badge|dabao  WAD_BACKEND=embedded|spi"
	@echo "      OLED_DRIVER=ssd1327|sh1107|spi  OLED_W=128|96  OLED_H=128|96  (dabao only;"
	@echo "      badge display is fixed: SPI SH1107 128x128)"

$(BUILD)/crt0.o: $(SDK)/src/runtime/crt0.S
	@mkdir -p $(BUILD)
	$(CC) $(ARCH) -g -c -o $@ $<

$(BUILD)/sdk/%.o: $(SDK)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

$(BUILD)/dg/%.o: $(DG)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

$(BUILD)/pl/%.o: $(PLATFORM)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

$(BUILD)/doom1_wad.o: $(ASSETS)/doom1.wad
	@mkdir -p $(BUILD)
	@test -f $< || (echo "Missing $< — see assets/README.md" && exit 1)
	cd $(ASSETS) && $(OBJCOPY) -I binary -O elf32-littleriscv -B riscv \
		--rename-section .data=.rodata,alloc,load,readonly,data,contents \
		doom1.wad $(BUILD)/doom1_wad.o

$(TARGET).elf: $(CRT0_OBJ) $(SDK_OBJS) $(DG_OBJS) $(PL_OBJS) $(WAD_OBJ)
	$(CC) $(ARCH) -T $(SDK)/bao1x.ld -nostdlib -nostartfiles -Wl,--gc-sections \
		-Wl,-Map,$(TARGET).map \
		-o $@ \
		$(CRT0_OBJ) $(SDK_OBJS) $(DG_OBJS) $(PL_OBJS) $(WAD_OBJ) \
		-lgcc
	$(SIZE) $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(UF2): $(TARGET).bin
	python3 $(SDK)/tools/sign_and_uf2.py $< $@ 0x60060000 0xa7d76373
	@echo "BUILD SUCCESS: $@"

size: $(TARGET).elf
	$(SIZE) -A $<

clean:
	rm -rf $(BUILD)

# Flash helpers (requires board in boot1)
PORT ?= /dev/tty.usbmodem*
flash: $(UF2)
	cd $(SDK) && ./bao_flash.sh flash $(PORT) doom
	@echo "Note: copy $(UF2) into SDK build/ as doom.uf2 or use serial_flash.py directly"
	python3 $(SDK)/tools/serial_flash.py $(PORT) $(UF2)

flash-persistent: $(UF2)
	python3 $(SDK)/tools/serial_flash.py --persistent $(PORT) $(UF2)
