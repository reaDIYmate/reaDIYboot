PROGRAM = reaDIYboot
MCU = atmega1280
CC = avr-gcc

# 0x1E000 for 8kB, 0x1F000 for 4kB
BOOTADDRESS = 0x1F000

# HTTP fields
PROGRAM_HOST = ""
USER_AGENT   = ""

# Path used when URL indirection is disabled
STATIC_PROGRAM_PATH = ""

# Optionnal HTTP requests
CHECK_STATUS_REQUEST = ""
DEVICE_ID_EEPROM_ADDRESS = 0
DEVICE_ID_LENGTH = 0
CLEAR_STATUS_REQUEST = ""
CHECK_STATUS_EXPECTED_RESPONSE = ""
PATH_JSON_PREFIX = ""

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
CFLAGS += -DWIFLY_BAUD_RATE=115200

# Check the update status before downloading a program
#CFLAGS += -DCHECK_STATUS_BEFORE_DOWNLOAD
# Get the HEX file location from the API status response
#CFLAGS += -DUSE_URL_INDIRECTION
# Use the device ID from the EEPROM in the API calls
#CFLAGS += -DUSE_DEVICE_ID
# Clear the update status after downloading a program
#CFLAGS += -DCLEAR_STATUS_AFTER_DOWNLOAD

# Define the HTTP parameters
CFLAGS += '-DCHECK_STATUS_REQUEST=$(CHECK_STATUS_REQUEST)'
CFLAGS += '-DDEVICE_ID_EEPROM_ADDRESS=$(DEVICE_ID_EEPROM_ADDRESS)'
CFLAGS += '-DDEVICE_ID_LENGTH=$(DEVICE_ID_LENGTH)'
CFLAGS += '-DCLEAR_STATUS_REQUEST=$(CLEAR_STATUS_REQUEST)'
CFLAGS += '-DCHECK_STATUS_EXPECTED_RESPONSE=$(CHECK_STATUS_EXPECTED_RESPONSE)'
CFLAGS += '-DPATH_JSON_PREFIX=$(PATH_JSON_PREFIX)'
CFLAGS += '-DPROGRAM_HOST=$(PROGRAM_HOST)'
CFLAGS += '-DSTATIC_PROGRAM_PATH=$(STATIC_PROGRAM_PATH)'
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
