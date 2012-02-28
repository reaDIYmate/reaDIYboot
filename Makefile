PROGRAM = reaDIYboot
MCU = atmega1280
CC = avr-gcc

# 0x1E000 for 8kB, 0x1F000 for 4kB
BOOTADDRESS = 0x1F000

# HTTP fields
PROGRAM_HOST = ""
PROGRAM_PATH = ""
USER_AGENT   = "ATmega1280"

# Optionnal HTTP requests
CHECK_STATUS_REQUEST = ""
CLEAR_STATUS_REQUEST = ""
CHECK_STATUS_EXPECTED_RESPONSE = ""

CFLAGS = -g -Wall -Os -mmcu=$(MCU)
CFLAGS += --combine
CFLAGS += -fdata-sections
CFLAGS += -ffreestanding
CFLAGS += -ffunction-sections
CFLAGS += -fno-inline-small-functions
CFLAGS += -fno-jump-tables
CFLAGS += -fno-move-loop-invariants
CFLAGS += -fno-tree-scev-cprop
CFLAGS += -fpack-struct
CFLAGS += -fshort-enums
CFLAGS += -funsigned-bitfields
CFLAGS += -funsigned-char
CFLAGS += -fwhole-program

CFLAGS += -DF_CPU=16000000L
CFLAGS += '-DMAX_TIME_COUNT=F_CPU>>4'
CFLAGS += -DSTK_BAUD_RATE=57600
CFLAGS += -DWIFLY_BAUD_RATE=57600

# Check the update status before downloading a program
#CFLAGS += -DCHECK_STATUS_BEFORE_DOWNLOAD
# Clear the update status after downloading a program
#CFLAGS += -DCLEAR_STATUS_AFTER_DOWNLOAD

# Define the HTTP parameters
CFLAGS += '-DCHECK_STATUS_REQUEST=$(CHECK_STATUS_REQUEST)'
CFLAGS += '-DCLEAR_STATUS_REQUEST=$(CLEAR_STATUS_REQUEST)'
CFLAGS += '-DCHECK_STATUS_EXPECTED_RESPONSE=$(CHECK_STATUS_EXPECTED_RESPONSE)'
CFLAGS += '-DPROGRAM_HOST=$(PROGRAM_HOST)'
CFLAGS += '-DPROGRAM_PATH=$(PROGRAM_PATH)'
CFLAGS += '-DUSER_AGENT=$(USER_AGENT)'

LDFLAGS = -Wl,--relax,--gc-sections,--section-start=.text=$(BOOTADDRESS)

all: $(PROGRAM).hex
all: $(PROGRAM).lst

%.elf: $(PROGRAM).o
	avr-gcc $(CFLAGS) $(LDFLAGS) -o $@ $^

%.lst: %.elf
	avr-objdump -h -S $< > $@
	avr-size --mcu=$(MCU) --format=avr -C $(PROGRAM).elf

%.hex: %.elf
	avr-objcopy -j .text -j .data -O ihex $< $@

%.srec: %.elf
	avr-objcopy -j .text -j .data -O srec $< $@

%.bin: %.elf
	avr-objcopy -j .text -j .data -O binary $< $@

clean:
	rm -rf *.o *.elf *.lst *.map *.sym *.lss *.eep *.srec *.bin *.hex
