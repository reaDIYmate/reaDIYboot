**reaDIYboot** is a bootloader for the ATmega1280 microcontroller from Atmel.
It gives the ATmega1280 two ways of programming itself:

1. Over a serial link using the STK500v1 protocol to receive new programs from avrdude
2. Over the internet using a WiFly GSX/EZX Wi-Fi module from Roving Networks to retrieve new programs from a remote server

## Hardware setup ##

reaDIYboot has been tested with the WiFly GSX RN131 and the WiFly EZX RN171 modules.

It was developped for a custom board with the following wiring:

1. Pin `RESET` of the WiFly to pin `PL0` of the ATmega1280
2. Pin `UART_RX` to pin `PD3` (i.e `USART1_TX`)
3. Pin `UART_TX` to pin `PD2` (i.e `USART1_RX`)
4. Pin `GPIO4` to pin `PJ5`
5. Pin `GPIO5` to pin `PJ6`
6. Pin `GPIO6` to pin `PJ7`

All the I/O definitions are written at the beginning of the source code so you can easily change them.

## WiFly firmware setup ##

This section explains how to configure a WiFly module to work with reaDIYboot.

It is recommended to update the firmware of the WiFly module as soon as possible. To do this, use the `ftp update` command and don't forget to perform a `factory RESET` once the update is complete.

Below is the WiFly configuration used to test reaDIYboot.

Setup the access point SSID and security phrase:

    set wlan ssid <you SSID here>
    set wlan phrase <your passphrase here>

enable the link monitor threshold with the recommended value:

    set wlan linkmon 5

disable access point autojoin:

    set wlan join 0

disable the socket monitor strings:

    set comm open 0
    set comm close 0
    set comm remote 0

disable echo of RX data and replace the version string with a single carriage return:

    set uart mode 0x21
    set opt replace 0xd

enable DHCP cache mode:

    set ip dhcp 3

close any open TCP connection when the connection to the access point is lost:

    set ip flags 0x6

force DNS:

    set ip tcp-mode 0x4

enable the alternate functions of the LEDs:

    set sys iofunc 0x70

increase the baudrate (250 kbaud yields 0.0% error with a 16 MHz crystal!)

    set uart raw 250000


## Compilation example 1 - building reaDIYboot for basic uploads ##

Suppose you have a board with an ATmega1280 and you want to upload new programs on it over the internet.

### 1. Setup the target program in the makefile

You need to specify a few parameters before compiling reaDIYboot.

Let's say your program is located at the following URL: `http://somedomain.com/hexfiles/some_program.hex`

The first thing to do is to edit the makefile and set the right values for `PROGRAM_HOST` and `PROGRAM_PATH`.

Find the following lines in the makefile:

    PROGRAM_HOST = ""
    PROGRAM_PATH = ""

and edit them as shown below:

    PROGRAM_HOST = "somedomain.com"
    PROGRAM_PATH = "/hexfiles/some_program.hex"

You can also set the User Agent string to whatever you want.

### 2. Compile reaDIYboot

    make clean
    make all

Notice that the makefile calls the `avr-size` utility to display the size of the compiled bootloader. Unless you have a *really* long URL, it should still be well under the 4kB boundary.

### 3. Burn reaDIYboot to the ATmega1280

### 4. You're done!

Now you have two ways of loading a new program onto the microcontroller:

1. Plug your board to a computer and use the Upload function of the Arduino IDE (or use avrdude directly).
2. Compile your program, rename the corresponding HEX file to `some_program.hex` and upload it to `somedomain.com/hexfiles/`

If the bootloader doesn't detect a computer trying to upload a new program when it starts, it is going to send a request to `somedomain.com` in order to determine whether `/hexfiles/some_program.hex` exists. If the file does exist, the bootloader will download it, write it to the Flash memory and run it after a reset.

## Compilation example 2 - building reaDIYboot for adaptative uploads ##

The previous method has a major inconvenience: the only way to prevent the microcontroller from downloading the same program every single time it resets is to remove the HEX file from the server.

Fortunately there are two options that you can enable in the makefile to help with this.

With the `CHECK_STATUS_BEFORE_DOWNLOAD` option enabled, the bootloader will send an HTTP request to the server before it tries to download the program, and if it doesn't receive the expected response it will skip the update.

With the `CLEAR_STATUS_AFTER_DOWNLOAD` option enabled, the bootloader will send an HTTP request to the server after an update has been successfully completed.

Since you can also specify which requests are sent, you can use these options to make specific calls on an API, which makes update management much more flexible.

### 1. Setup the API calls in the makefile

Let's say you have two scripts:

    somedomain.com/api/check_status.php
    somedomain.com/api/clear_status.php
which are written so that `check_status.php` returns `{"status":1}` until `clear_status.php` has been called, and `{"status":0}` afterwards.

In the makefile, uncomment the following two lines, which are commented by default:

    CFLAGS += -DCHECK_STATUS_BEFORE_DOWNLOAD
    CFLAGS += -DCLEAR_STATUS_AFTER_DOWNLOAD

Find the following lines in the makefile:

    CHECK_STATUS_REQUEST = ""
    CLEAR_STATUS_REQUEST = ""
    CHECK_STATUS_EXPECTED_RESPONSE = ""

and edit them as shown below:

    CHECK_STATUS_REQUEST = "/api/check_status.php"
    CLEAR_STATUS_REQUEST = "/api/clear_status.php"
    CHECK_STATUS_EXPECTED_RESPONSE = "{\"status\":1}"

### 2. Compile reaDIYboot

Like in Example 1, the compiled bootloader should still be less than 4096 bytes (using the strings from my example, I get 3798 bytes).

### 3. Burn the bootloader

### 4. You're done!

## A few more ideas ##

reaDIYboot is still in an early stage and there is still room for many improvements.

It has been successfully tested with program sizes up to 128kB.

Robustness could be increased by adding CRC computations and/or appending specific markers to the programs before writing them to the Flash memory.

***

**reaDIYboot** is being developped as part of the [reaDIYmate](http://readiymate.com) platform.
