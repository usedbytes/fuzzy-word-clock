# Project Name
PROJECT = blink
# Source files
SOURCES = lpc11uxx/system_LPC11Uxx.c \
		  startup.c \
		  main.c \
		  ds1302.c \
		  usb_cdc.c \
		  iap.c \
		  lut.c \
		  util.c

# Linker script
LINKER_SCRIPT = lpc11u24.dld


#########################################################################

ISP_DIR ?= "/var/run/media/kernelcode/CRP DISABLD"

#########################################################################

OBJDIR = obj
OBJECTS = $(patsubst %.c,$(OBJDIR)/%.o,$(SOURCES))

#########################################################################

OPT = -Os
DEBUG = -g
INCLUDES = -Icore/ -Ilpc11uxx/

#########################################################################

# Compiler Options
CFLAGS = -fno-common -mcpu=cortex-m0 -mthumb
CFLAGS += $(OPT) $(DEBUG) $(INCLUDES)
CFLAGS += -Wall -Wextra
CFLAGS += -Wcast-align -Wcast-qual -Wimplicit -Wpointer-arith -Wswitch -Wredundant-decls -Wreturn-type -Wshadow -Wunused
# Linker options
LDFLAGS = -mcpu=cortex-m0 -mthumb $(OPT) -nostartfiles -Wl,-Map=$(OBJDIR)/$(PROJECT).map -T$(LINKER_SCRIPT)
# Assembler options
ASFLAGS = -ahls -mcpu=cortex-m0 -mthumb

# Compiler/Assembler/Linker Paths
HOSTCC = gcc
CROSS = arm-none-eabi-
CC = $(CROSS)gcc
AS = $(CROSS)as
LD = $(CROSS)ld
OBJDUMP = $(CROSS)objdump
OBJCOPY = $(CROSS)objcopy
SIZE = $(CROSS)size
REMOVE = rm -f

#########################################################################

.PHONY: all checksum
all: $(PROJECT).hex $(PROJECT).bin $(PROJECT).lss

$(PROJECT).bin: $(PROJECT).elf
	$(OBJCOPY) -R .stack -R .bss -O binary -S $(PROJECT).elf $(PROJECT).bin

$(PROJECT).hex: $(PROJECT).elf
	$(OBJCOPY) -R .stack -R .bss -O ihex $(PROJECT).elf $(PROJECT).hex

$(PROJECT).elf: $(OBJECTS) $(LINKER_SCRIPT)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $(PROJECT).elf

$(PROJECT).lss: $(PROJECT).elf
	$(OBJDUMP) -h -S $< > $@

lpc_checksum: lpc_checksum.c
	$(HOSTCC) $< -o $@

$(PROJECT)_checksum.bin: $(PROJECT).bin
	./lpc_checksum $< $@

.PHONY: stats
stats: $(PROJECT).elf
	$(OBJDUMP) -th $(PROJECT).elf
	$(SIZE) $(PROJECT).elf

.PHONY: isp
isp: $(PROJECT)_checksum.bin
	dd if=$< conv=nocreat,notrunc of='/var/run/media/kernelcode/CRP DISABLD/firmware.bin'

.PHONY: clean
clean:
	$(REMOVE) -r $(OBJDIR)
	$(REMOVE) $(PROJECT).elf
	$(REMOVE) $(PROJECT).hex
	$(REMOVE) $(PROJECT).bin

#########################################################################

$(OBJDIR)/%.o : %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

