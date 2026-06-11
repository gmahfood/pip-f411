# pip-f411 — bare-metal STM32F411 firmware (Pip, the desk bot)
# Toolchain: arm-none-eabi-gcc
#
# NOTE: recipe lines use '>' as the prefix (see .RECIPEPREFIX below) instead of
# tabs, so this file survives copy/paste through any editor. GNU make >= 3.82.
.RECIPEPREFIX := >

TARGET = pip
BUILD  = build

# --- toolchain ---
CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

# --- MCU: Cortex-M4F ---
CPU   = -mcpu=cortex-m4
FPU   = -mfpu=fpv4-sp-d16
FLOAT = -mfloat-abi=hard
MCU   = $(CPU) -mthumb $(FPU) $(FLOAT)

# --- linker script ---
LDSCRIPT = linker/STM32F411CEUx_FLASH.ld

# --- CMSIS (vendored by you — see cmsis/README.md) ---
CMSIS_INC = -Icmsis/Include -Icmsis/Device/ST/STM32F4xx/Include
CMSIS_SRC = cmsis/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c
STARTUP   = cmsis/Device/ST/STM32F4xx/Source/Templates/gcc/startup_stm32f411xe.s

# --- defines ---
DEFS = -DSTM32F411xE

# --- includes ---
INC = -Iinc -Ilib/ssd1306 $(CMSIS_INC)

# --- sources ---
SRC = $(wildcard src/*.c) $(wildcard lib/ssd1306/*.c) $(CMSIS_SRC)
ASM = $(STARTUP)

# --- flags ---
CFLAGS  = $(MCU) $(DEFS) $(INC) -Wall -Wextra -g3 -O2 \
          -ffunction-sections -fdata-sections -std=gnu11
LDFLAGS = $(MCU) -T$(LDSCRIPT) -Wl,--gc-sections \
          -Wl,-Map=$(BUILD)/$(TARGET).map \
          --specs=nano.specs --specs=nosys.specs

OBJ  = $(addprefix $(BUILD)/, $(notdir $(SRC:.c=.o)))
OBJ += $(addprefix $(BUILD)/, $(notdir $(ASM:.s=.o)))
VPATH = src lib/ssd1306 $(dir $(CMSIS_SRC)) $(dir $(STARTUP))

all: $(BUILD)/$(TARGET).bin $(BUILD)/$(TARGET).hex
> $(SIZE) $(BUILD)/$(TARGET).elf

$(BUILD):
> mkdir -p $(BUILD)

$(BUILD)/%.o: %.c | $(BUILD)
> $(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.s | $(BUILD)
> $(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJ)
> $(CC) $(LDFLAGS) $^ -o $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
> $(OBJCOPY) -O binary $< $@

$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
> $(OBJCOPY) -O ihex $< $@

# --- flashing (pick whichever matches your probe) ---
flash: $(BUILD)/$(TARGET).bin
> st-flash write $(BUILD)/$(TARGET).bin 0x08000000

flash-openocd: $(BUILD)/$(TARGET).elf
> openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
    -c "program $(BUILD)/$(TARGET).elf verify reset exit"

clean:
> rm -rf $(BUILD)

.PHONY: all clean flash flash-openocd
