# ============================================================================
# pip-f411 — Makefile
# Bare-metal STM32F411 firmware build (Pip, the desk bot).
# Toolchain: arm-none-eabi-gcc (GNU Arm Embedded Toolchain).
# ============================================================================
#
# WHAT A MAKEFILE IS, IN ONE PARAGRAPH:
# A dependency graph + recipes. Each "target" (a file we want) has prereqs
# (files it depends on) and a recipe (shell commands to produce it). Running
# `make` walks the graph, compares file timestamps, and only rebuilds things
# whose prereqs are newer. Change one .c, recompile that one + relink. That
# incremental rebuild is why we use make instead of a shell script.
#
# BUILD PIPELINE FOR THIS PROJECT:
#   src/*.c  --(arm-none-eabi-gcc -c)-->  build/*.o
#   build/*.o + startup.s + linker.ld  --(gcc link)-->  build/pip.elf
#   build/pip.elf  --(objcopy)-->  build/pip.bin / build/pip.hex
#   (.elf for debugging in GDB; .bin or .hex for flashing the chip)
# ============================================================================


# ----------------------------------------------------------------------------
# .RECIPEPREFIX — use '>' instead of tab to indent recipes
# ----------------------------------------------------------------------------
# Normally, recipe lines in a Makefile MUST start with a literal TAB character.
# Indenting with spaces gives the classic "missing separator. Stop." error.
# That tab requirement is the #1 source of broken Makefiles when files get
# copy-pasted through Slack, web forms, or editors that auto-convert tabs.
#
# .RECIPEPREFIX := > tells make "use '>' as the recipe indent instead." This
# Makefile survives any copy-paste. Requires GNU make >= 3.82 (every modern
# Mac/Linux has it).
.RECIPEPREFIX := >


# ----------------------------------------------------------------------------
# Project variables
# ----------------------------------------------------------------------------
# Simple text substitution. $(TARGET) expands to "pip" everywhere below.
TARGET = pip
BUILD  = build


# ----------------------------------------------------------------------------
# Toolchain programs
# ----------------------------------------------------------------------------
# CC, OBJCOPY, SIZE — these are the three tools we invoke during a build.
# Naming the compiler variable CC is a make CONVENTION; make's built-in
# implicit rules look for $(CC), so using that name keeps things idiomatic.
#
# arm-none-eabi-gcc  — C compiler for ARM bare-metal targets
# arm-none-eabi-objcopy — converts between binary formats (ELF -> .bin/.hex)
# arm-none-eabi-size — prints text/data/bss sizes of the final binary
CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size


# ----------------------------------------------------------------------------
# MCU flags — these tell GCC exactly what chip to generate code for
# ----------------------------------------------------------------------------
# -mcpu=cortex-m4   Generate code for the Cortex-M4 core (the F411's CPU).
#                   Without this, gcc emits generic ARM code that won't run.
#
# -mthumb           Emit THUMB instructions, not classic 32-bit ARM ones.
#                   Cortex-M chips are Thumb-2 ONLY — they literally cannot
#                   decode old 32-bit ARM instructions. Thumb instructions
#                   are 16 or 32 bits each (more compact), which is one of
#                   the reasons Cortex-M can fit useful code in 512 KB.
#                   Interview classic: "What's the difference between ARM
#                   mode and Thumb mode?" Cortex-M is Thumb-only.
#
# -mfpu=fpv4-sp-d16 The F411 has a single-precision FPU (Floating Point Unit).
#                   fpv4-sp = FPU v4, single-precision floats.
#                   d16     = 16 double registers available.
#                   Without this flag, `float` math becomes slow software
#                   emulation (calls into __aeabi_fadd etc. in libgcc).
#
# -mfloat-abi=hard  Pass float arguments in FPU registers (fast). The other
#                   options are 'soft' (no FPU, software floats) and 'softfp'
#                   (FPU used, but args still pass in integer regs for
#                   compatibility). 'hard' requires the FPU to actually exist.
#                   ABI MUST MATCH across all object files and libraries you
#                   link — mixing them causes weird linker errors.
CPU   = -mcpu=cortex-m4
FPU   = -mfpu=fpv4-sp-d16
FLOAT = -mfloat-abi=hard
MCU   = $(CPU) -mthumb $(FPU) $(FLOAT)


# ----------------------------------------------------------------------------
# Linker script — tells the linker the chip's memory layout
# ----------------------------------------------------------------------------
# The linker needs to know things only the chip vendor can tell it:
#   - FLASH starts at 0x08000000, is 512 KB long
#   - RAM starts at 0x20000000, is 128 KB long
#   - The vector table must go at the very start of FLASH
#   - Stack pointer initial value, heap layout, etc.
# All of that lives in linker/STM32F411CEUx_FLASH.ld. Different MCU = different
# linker script.
LDSCRIPT = linker/STM32F411CEUx_FLASH.ld


# ----------------------------------------------------------------------------
# CMSIS — the vendor scaffolding we BORROWED (per the project principle)
# ----------------------------------------------------------------------------
# CMSIS = Cortex Microcontroller Software Interface Standard. It's ARM's
# standardized way of describing a Cortex-M chip: register definitions,
# vector tables, startup code.
#
# Three pieces of it need to be in our build:
#   CMSIS_INC — header search paths (where #include "stm32f4xx.h" finds files)
#   CMSIS_SRC — system_stm32f4xx.c, which has SystemInit() (runs before main)
#               and SystemCoreClockUpdate()
#   STARTUP   — startup_stm32f411xe.s, the assembly file that contains the
#               VECTOR TABLE (the array of ISR function pointers like
#               SysTick_Handler), zeros .bss, copies .data from flash to RAM,
#               then calls main()
CMSIS_INC = -Icmsis/Include -Icmsis/Device/ST/STM32F4xx/Include
CMSIS_SRC = cmsis/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c
STARTUP   = cmsis/Device/ST/STM32F4xx/Source/Templates/gcc/startup_stm32f411xe.s


# ----------------------------------------------------------------------------
# Preprocessor defines
# ----------------------------------------------------------------------------
# -DSTM32F411xE is equivalent to writing `#define STM32F411xE` at the top of
# every file. CMSIS headers use this to pick the right register definitions
# for our exact chip variant (F411xE vs F411xC vs F411xR — they have
# different flash sizes). Without this, stm32f4xx.h errors out telling you
# to define a variant.
DEFS = -DSTM32F411xE


# ----------------------------------------------------------------------------
# Header search paths
# ----------------------------------------------------------------------------
# -I tells the preprocessor where to look for headers when you #include "x.h".
# Order matters — earlier paths win on name collisions. Our own inc/ first,
# then the borrowed SSD1306 driver headers, then CMSIS at the bottom.
INC = -Iinc -Ilib/ssd1306 $(CMSIS_INC)


# ----------------------------------------------------------------------------
# Source file lists
# ----------------------------------------------------------------------------
# $(wildcard PATTERN) is a make built-in that expands to all files matching
# the glob pattern. So $(wildcard src/*.c) becomes:
#   src/clock.c src/exti.c src/faces.c src/fsm.c src/gpio.c src/i2c.c
#   src/main.c src/tim_servo.c
# This means: drop a new .c file into src/ and the next `make` picks it up
# automatically. Convenient, but tradeoff: a typo in a filename gets silently
# ignored. Some teams prefer listing files explicitly for that reason.
SRC = $(wildcard src/*.c) $(wildcard lib/ssd1306/*.c) $(CMSIS_SRC)
ASM = $(STARTUP)


# ----------------------------------------------------------------------------
# CFLAGS — the flag soup passed to gcc when compiling
# ----------------------------------------------------------------------------
# -Wall -Wextra        Turn on most warnings. Always do this. Free bug catches.
#
# -g3                  Max debug info (level 3 includes macro definitions, so
#                      GDB can show macros). Compile size grows; runtime is
#                      unaffected because debug info lives in the ELF, not in
#                      the flashed binary.
#
# -O2                  Optimization level 2. -O0 = none (huge, easy to debug),
#                      -O2 = standard production, -Os = optimize for size
#                      (common in firmware where flash is tight, -Os is often
#                      used instead of -O2). INTERVIEW TRAP: code that "only
#                      works at -O0" almost always has a missing `volatile`.
#
# -ffunction-sections  Put each function in its own section in the ELF.
# -fdata-sections      Same for each global/static variable.
#                      These pair with -Wl,--gc-sections in LDFLAGS below:
#                      the linker can then garbage-collect any unused
#                      sections, dramatically shrinking the final binary.
#
# -std=gnu11           Use C11 with GNU extensions. Plain -std=c11 would
#                      forbid GNU-only features like inline `asm()` blocks.
CFLAGS  = $(MCU) $(DEFS) $(INC) -Wall -Wextra -g3 -O2 \
          -ffunction-sections -fdata-sections -std=gnu11


# ----------------------------------------------------------------------------
# LDFLAGS — flags passed to gcc when linking
# ----------------------------------------------------------------------------
# Note: $(MCU) is repeated here because the link step also needs to know the
# CPU/FPU/ABI — wrong ABI here gives a "uses VFP register arguments but
# library does not" linker error.
#
# -T $(LDSCRIPT)             Use this linker script.
#
# -Wl,X                      Pass X to the LINKER (ld), not to gcc.
#                            gcc invokes ld for us, so we tunnel flags through.
#
# -Wl,--gc-sections          Discard unused sections (works with
#                            -ffunction-sections / -fdata-sections above).
#                            Often saves significant flash.
#
# -Wl,-Map=build/pip.map     Produce a MAP FILE. This is gold for embedded:
#                            it tells you exactly where every function and
#                            variable lives in memory, how big each is, and
#                            what's eating your flash. Open build/pip.map
#                            after a build sometime, scroll to "Linker script
#                            and memory map" — it's a treasure for debugging
#                            "why is my binary so big" questions.
#
# --specs=nano.specs         Link against newlib-NANO, a stripped-down version
#                            of the C standard library tuned for embedded.
#                            Full newlib's printf alone is ~30 KB; nano's is
#                            ~5 KB.
#
# --specs=nosys.specs        Provide stub implementations of system calls
#                            (_write, _read, _sbrk, _exit, ...). We have no OS,
#                            so nothing real to call — these just return -1
#                            or do nothing. Without this, the linker complains
#                            about "undefined reference to _write" whenever
#                            you use printf-family functions.
LDFLAGS = $(MCU) -T$(LDSCRIPT) -Wl,--gc-sections \
          -Wl,-Map=$(BUILD)/$(TARGET).map \
          --specs=nano.specs --specs=nosys.specs


# ----------------------------------------------------------------------------
# Object file list — where each .c/.s ends up in build/
# ----------------------------------------------------------------------------
# This computes the list of .o files we need to build. Read it inside-out:
#
#   $(SRC:.c=.o)         Substitution: replace .c with .o in every entry.
#                        "src/gpio.c"  ->  "src/gpio.o"
#
#   $(notdir ...)        Strip directory prefixes.
#                        "src/gpio.o"  ->  "gpio.o"
#
#   $(addprefix build/, ...)
#                        Prepend "build/".
#                        "gpio.o"      ->  "build/gpio.o"
#
# Net result: every src/foo.c becomes build/foo.o — flat output, no
# subdirectories under build/. Then we append the startup .s the same way.
OBJ  = $(addprefix $(BUILD)/, $(notdir $(SRC:.c=.o)))
OBJ += $(addprefix $(BUILD)/, $(notdir $(ASM:.s=.o)))


# ----------------------------------------------------------------------------
# VPATH — where to LOOK for source files when prereqs are bare filenames
# ----------------------------------------------------------------------------
# The pattern rule below says "to build build/gpio.o, I need gpio.c". But
# gpio.c isn't in the current directory — it's in src/. VPATH tells make:
# "if you can't find a prereq here, try these directories." That's how flat
# .o output works without sub-folders breaking the rule.
VPATH = src lib/ssd1306 $(dir $(CMSIS_SRC)) $(dir $(STARTUP))


# ----------------------------------------------------------------------------
# Default target: build firmware + print size
# ----------------------------------------------------------------------------
# "all" is the conventional name for the default target. Running plain `make`
# is identical to running `make all`. It depends on the .bin and .hex, which
# forces them to be built. Then it runs arm-none-eabi-size to print:
#
#    text    data    bss    dec    hex   filename
#    1240       0     28   1268    4f4   build/pip.elf
#
# Where:
#   text = code + constants (lives in flash)
#   data = initialized globals/statics (lives in flash, copied to RAM at boot)
#   bss  = zero-initialized globals/statics (lives in RAM only)
# Sanity checks:
#   text + data must fit in 512 KB flash
#   data + bss must fit in 128 KB RAM
all: $(BUILD)/$(TARGET).bin $(BUILD)/$(TARGET).hex
> $(SIZE) $(BUILD)/$(TARGET).elf


# ----------------------------------------------------------------------------
# Make sure build/ exists before we drop object files into it
# ----------------------------------------------------------------------------
$(BUILD):
> mkdir -p $(BUILD)


# ----------------------------------------------------------------------------
# Pattern rule: compile any .c into build/something.o
# ----------------------------------------------------------------------------
# `%` is a wildcard. This rule says: "to make build/anything.o from
# anything.c, run the recipe below." With VPATH set above, "anything.c" can
# live in src/ or lib/ssd1306/ or the CMSIS template dir — make finds it.
#
# Recipe variables:
#   $<   first prerequisite       (the .c file)
#   $@   target name              (the .o file)
#
# -c   means "compile only, don't link" — produces the .o.
#
# The `| $(BUILD)` part is an ORDER-ONLY PREREQUISITE (note the pipe).
# It says: "make sure build/ exists before running this recipe, but DON'T
# rebuild me just because build/'s timestamp changed." Without the pipe,
# every .o would rebuild after each `mkdir` (since dir timestamps change),
# which would be silly.
$(BUILD)/%.o: %.c | $(BUILD)
> $(CC) $(CFLAGS) -c $< -o $@


# Same rule for assembly files (the startup file is .s, not .c).
$(BUILD)/%.o: %.s | $(BUILD)
> $(CC) $(CFLAGS) -c $< -o $@


# ----------------------------------------------------------------------------
# Link step: combine all object files into the final ELF
# ----------------------------------------------------------------------------
# ELF (Executable and Linkable Format) is the binary format GCC produces.
# It contains the program code, data, and lots of metadata (debug info,
# section headers, symbol tables) that GDB and objcopy use.
#
# $^  =  ALL prerequisites (every .o in $(OBJ)).
# $@  =  the target (build/pip.elf).
#
# We invoke the linker via gcc (not directly via `ld`) because gcc knows to
# add the C runtime startup files (crt0 etc.) and the right libgcc paths.
$(BUILD)/$(TARGET).elf: $(OBJ)
> $(CC) $(LDFLAGS) $^ -o $@


# ----------------------------------------------------------------------------
# Convert ELF to flashable formats
# ----------------------------------------------------------------------------
# The .elf has debug info, section headers, etc. The chip's flash can't
# execute that — it just wants raw instruction bytes at specific addresses.
#
# -O binary  Strip everything except the raw program bytes.
#            Small, but the flasher needs to be told the start address
#            (0x08000000 for STM32).
#
# -O ihex    Intel HEX format — ASCII text that encodes data + addresses.
#            Slightly bigger but self-describing; flashers know where each
#            chunk goes. Standard for ST-LINK tools.
$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
> $(OBJCOPY) -O binary $< $@

$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
> $(OBJCOPY) -O ihex $< $@


# ----------------------------------------------------------------------------
# Flashing targets — pick whichever matches your debug probe
# ----------------------------------------------------------------------------
# 0x08000000 is the start of flash on STM32. The reset vector at the very
# beginning of the binary points to the startup code, which sets up the
# stack, zeros bss, copies data, and jumps to main.
flash: $(BUILD)/$(TARGET).bin
> st-flash write $(BUILD)/$(TARGET).bin 0x08000000

flash-openocd: $(BUILD)/$(TARGET).elf
> openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
    -c "program $(BUILD)/$(TARGET).elf verify reset exit"


# ----------------------------------------------------------------------------
# Clean — wipe the build directory
# ----------------------------------------------------------------------------
clean:
> rm -rf $(BUILD)


# ----------------------------------------------------------------------------
# .PHONY — declare targets that aren't real files
# ----------------------------------------------------------------------------
# Without this, if you ever accidentally created a file named `clean` in this
# directory, `make clean` would see the file exists, decide there's nothing
# to do, and silently skip your cleanup. .PHONY tells make "these are
# commands, not files — always run them."
.PHONY: all clean flash flash-openocd


# ============================================================================
# KNOWN GAP — header dependency tracking
# ============================================================================
# This Makefile does NOT auto-track header dependencies. If you edit
# inc/gpio.h, make won't know that src/gpio.c and src/main.c (which include
# it) need to recompile. You'd need `make clean && make` to be safe.
#
# Fix when this starts biting: add -MMD -MP to CFLAGS and `-include $(DEPS)`
# at the end. Standard GCC dependency-generation pattern. We'll do it later
# if/when needed; not worth the complexity right now.
# ============================================================================