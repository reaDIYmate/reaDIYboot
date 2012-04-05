/* reaDIYboot
 * Written by Pierre Bouchet
 * Copyright (C) 2011-2012 reaDIYmate
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <inttypes.h>
#include <stdbool.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

/* WiFly reset pin */
volatile uint8_t* const RESET_PORT = &PORTL;
volatile uint8_t* const RESET_PORT_INPUT = &PINL;
volatile uint8_t* const RESET_DDR = &DDRL;
uint8_t const RESET_PIN = PINL0;

/* WiFly GPIO4 pin */
volatile uint8_t* const GPIO4_PORT = &PORTJ;
volatile uint8_t* const GPIO4_PORT_INPUT = &PINJ;
volatile uint8_t* const GPIO4_DDR = &DDRJ;
uint8_t const GPIO4_PIN = PINJ5;

/* WiFly GPIO5 pin */
volatile uint8_t* const GPIO5_PORT = &PORTJ;
volatile uint8_t* const GPIO5_PORT_INPUT = &PINJ;
volatile uint8_t* const GPIO5_DDR = &DDRJ;
uint8_t const GPIO5_PIN = PINJ6;

/* WiFly GPIO6 pin */
volatile uint8_t* const GPIO6_PORT = &PORTJ;
volatile uint8_t* const GPIO6_PORT_INPUT = &PINJ;
volatile uint8_t* const GPIO6_DDR = &DDRJ;
uint8_t const GPIO6_PIN = PINJ7;

/* Green LED pin */
volatile uint8_t* const GREEN_LED_PORT = &PORTD;
volatile uint8_t* const GREEN_LED_PORT_INPUT = &PIND;
volatile uint8_t* const GREEN_LED_DDR = &DDRD;
uint8_t const GREEN_LED_PIN = PIND5;

/* Red LED pin */
volatile uint8_t* const RED_LED_PORT = &PORTD;
volatile uint8_t* const RED_LED_PORT_INPUT = &PIND;
volatile uint8_t* const RED_LED_DDR = &DDRD;
uint8_t const RED_LED_PIN = PIND4;

/* Use Timer/Counter3 channel A to detect UART timeouts */
volatile uint16_t* const UART_OCR = &OCR3A;
uint8_t const UART_OCF = OCF3A;

/* Use Timer/Counter3 channel B to detect WLAN timeouts */
volatile uint16_t* const WLAN_OCR = &OCR3B;
uint8_t const WLAN_OCF = OCF3B;

/* Use Timer/Counter3 channel C to detect HTTP timeouts */
volatile uint16_t* const HTTP_OCR = &OCR3C;
uint8_t const HTTP_OCF = OCF3C;

/* Size of a program page in a HEX file */
#define HEX_BUFFER_SIZE 4096
/* Size of a program page in Flash memory (128 words) */
#define FLASH_PAGE_SIZE 0x80U
/* Size of the buffer used to hold the HEX file location */
#define PATH_BUFFER_SIZE 64

// STK500 protocol
/* Get parameter value */
uint8_t const STK_GET_PARAMETER = 0x41;
/* Leave program mode */
uint8_t const STK_LEAVE_PROGMODE = 0x51;
/* Load address */
uint8_t const STK_LOAD_ADDRESS = 0x55;
/* Program page */
uint8_t const STK_PROG_PAGE = 0x64;
/* Read page */
uint8_t const STK_READ_PAGE = 0x74;
/* Read signature bytes */
uint8_t const STK_READ_SIGN = 0x75;
/* Set device programming parameters */
uint8_t const STK_SET_DEVICE = 0x42;
/* Set extended device programming parameters */
uint8_t const STK_SET_DEVICE_EXT = 0x45;
/* Universal command */
uint8_t const STK_UNIVERSAL = 0x56;
/* Software version major */
uint8_t const STK_SW_MAJOR = 0x81;
/* Software version minor */
uint8_t const STK_SW_MINOR = 0x82;
/* Software version major */
uint8_t const SW_MAJOR = 0x01;
/* Software version minor */
uint8_t const SW_MINOR = 0x10;
/* No top-card detected */
uint8_t const NO_TOPCARD_DETECTED = 0x03;
/* End of packet */
uint8_t const STK_CRC_EOP = 0x20;
/* Sent after a valid command has been executed */
uint8_t const STK_OK = 0x10;
/* Sent after STK_CRC_EOP has been received */
uint8_t const STK_INSYNC = 0x14;
/* Device Signature Byte 1 */
uint8_t const SIG1 = 0x1E;
/* Device Signature Byte 2 */
uint8_t const SIG2 = 0x97;
/* Device Signature Byte 3 */
uint8_t const SIG3 = 0x03;

// Error thresholds for the state machines
/* STK programmer */
uint8_t const MAX_STK_ERROR_COUNT = 5;
/* WiFly module */
uint8_t const MAX_COMMAND_ERRORS = 3;
uint8_t const MAX_WIFLY_CRITICAL_ERRORS = 3;
uint8_t const MAX_WLAN_ERRORS = 3;
/* Web server  */
uint8_t const MAX_DOWNLOAD_CRITICAL_ERRORS = 3;
uint8_t const MAX_HTTP_ERRORS = 3;
uint8_t const MAX_SOCKET_ERRORS = 3;

/* HTTP fields sent with each request */
char* const HTTP_FIELDS =
    " HTTP/1.1\r\n"
    "User-Agent: " USER_AGENT "\r\n"
    "Host: " PROGRAM_HOST "\r\n"
    "Connection: Keep-Alive\r\n";

/* Pointer to a string representing the HEX file location */
#ifdef USE_URL_INDIRECTION
char* PROGRAM_PATH = 0;
#else
char* PROGRAM_PATH = STATIC_PROGRAM_PATH;
#endif

/* Buffer used to hold the location of the HEX file */
char path_buffer[PATH_BUFFER_SIZE];

void main(void) __attribute__((OS_main));

static void bootload_from_internet(void);
static void bootload_from_stk(void);

static bool add_error(uint8_t* count, uint8_t max_count);

/* Download management */
static void download_append_leftover(void);
static bool download_get_chunk(void);
static bool download_get_path(void);
static bool download_get_size(void);
static bool download_get_status(void);
static bool download_parse_chunk(void);
static bool download_parse_path(void);
static bool download_parse_size(void);
static void download_update_status(void);

/* Send HTTP requests */
static bool http_send(void (*request)(void), bool (*action)(void));
static bool http_await_response(void);
static void request_get_chunk(void);
static void request_get_size(void);
static void request_get_status(void);
static void request_update_status(void);

/* iHEX data format */
static bool ihex_check_line(void);
static bool ihex_load_byte(void);
static uint8_t ihex_parse_byte(uint8_t* source);

/* STK communication protocol */
static void stk_byte_response(uint8_t);
static uint8_t stk_get_char(void);
static void stk_get_n_char(uint8_t);
static void stk_nothing_response(void);
static void stk_put_char(uint8_t);

/* WiFly EZX Wi-Fi module management */
static bool wifly_check_socket(void);
static bool wifly_check_wlan(void);
static void wifly_close_socket(void);
static bool wifly_connect_to_host(void);
static void wifly_enter_command_mode(void);
static bool wifly_find_string(const char* target);
static uint8_t wifly_get_char(void);
static void wifly_join_wlan(void);
static void wifly_open_socket(void);
static void wifly_put_char(uint8_t ch);
static void wifly_put_string(const char* source);
static void wifly_put_long(uint32_t number);
static void wifly_reset(void);
static bool wifly_set_host(void);

/* Core self-programming function */
static void write_bin_page(void);

/* Possible states for the WiFly state machine */
enum wifly_state {
    RESETTING,
    SETTING_HOST,
    JOINING_WLAN,
    OPENING_SOCKET,
    WIFLY_CRITICAL_ERROR
};

/* Possible states for the Web downloader state machine */
enum download_state {
    CHECKING_SOCKET,
    SENDING_REQUEST,
    RECEIVING_RESPONSE,
    HTTP_ERROR,
    DOWNLOAD_CRITICAL_ERROR
};

/* Possible states for the internet bootloader state machine */
enum bootloader_state {
    ENTERING,
    FILLING_BUFFER,
    CHECKING_HEX_LINE,
    PARSING_HEX_LINE,
    WRITING_BIN_PAGE,
    EXITING,
    JUMPING_TO_APP
};

struct wifly_struct {
    enum wifly_state state;
    struct {
        uint8_t command;
        uint8_t critical;
        uint8_t wlan;
        uint8_t socket;
    } errors;
} wifly = {RESETTING, {0, 0, 0, 0}};

struct download_struct {
    enum download_state state;
    struct {
        uint8_t critical;
        uint8_t http;
        uint8_t parse;
    } errors;
} download = {CHECKING_SOCKET, {0, 0, 0}};

/* Binary program page */
struct hex_chunk_struct {
    uint32_t file_start;
    uint32_t file_stop;
    uint16_t size;
    uint16_t index;
} hex_chunk = {0, 0, 0, 0};

/* Binary program page */
struct bin_page_struct {
    uint16_t address;
    uint16_t index;
} bin_page = {0x0000, 0};

/* Target address in Flash memory */
union address_union {
  uint16_t word;
  uint8_t byte[2];
} address;

/* Source block byte count */
union length_union {
  uint16_t word;
  uint8_t byte[2];
} length;

/* State of the internet bootloader state machine */
enum bootloader_state boot_state = ENTERING;

/* UART buffer */
uint8_t bin_buffer[2*FLASH_PAGE_SIZE];
/* HEX page buffer */
uint8_t hex_buffer[HEX_BUFFER_SIZE + 1];
/* UART error counter */
uint8_t stk_errors;
/* STK communication timeout flag */
bool stk_timeout;
/* Size of the HEX file hosted on the remote server in bytes */
uint32_t hex_program_size;
/* Byte count of the current line in the HEX file */
uint8_t line_byte_count;
/* Buffer used for unsigned-to-ASCII conversions */
char utoa_buffer[7];
/* Function pointer to the start of the application */
void (*app_start)(void) = 0x0000;

void main(void)
{
    uint8_t status_register;

    status_register = MCUSR;
    // Clear the Watchdog System Reset Flag
    MCUSR = 0x00;
    // Disable the Watchdog Timer (see section 11.4 in the ATmega1280 manual)
    WDTCSR |= (1 << WDCE) | (1 << WDE);
    WDTCSR = 0x00;

    // If the last reset was triggered by the watchdog timer, skip the
    // bootloader and start the main program.
    if (status_register & (1 << WDRF))
        app_start();

    // Set the LED pins as outputs
    *GREEN_LED_DDR |= (1 << GREEN_LED_PIN);
    *RED_LED_DDR |= (1 << RED_LED_PIN);
    // Set the reset line for the Wi-Fi module as an output
    *RESET_DDR |= (1 << RESET_PIN);
    // Set the GPIO5 pin as an output
    *GPIO5_DDR |= (1 << GPIO5_PIN);
    // Set the GPIO4 and GPIO6 pins as inputs
    *GPIO4_DDR &= ~(1 << GPIO4_PIN);
    *GPIO6_DDR &= ~(1 << GPIO6_PIN);
    // Enable internal pull-up resistor
    *GPIO4_PORT |= (1 << GPIO4_PIN);

    // Initialize UART0 (for the STK programmer)
    UBRR0L = (uint8_t)(F_CPU/(STK_BAUD_RATE*16L) - 1);
    UBRR0H = (F_CPU/(STK_BAUD_RATE*16L)-1) >> 8;
    UCSR0A = 0x00;
    UCSR0B = (1 << TXEN0)|(1 << RXEN0);
    UCSR0C = (1 << UCSZ01)|(1 << UCSZ00);
    // Enable internal pull-up resistor on pin D0 (RX)
    DDRE &= ~(1 << PINE0);
    PORTE |= (1 << PINE0);

    // Initialize UART1 (for the Wi-Fi module)
    UBRR1L = (uint8_t)(F_CPU/(WIFLY_BAUD_RATE*16L) - 1);
    UBRR1H = (F_CPU/(WIFLY_BAUD_RATE*16L) - 1) >> 8;
    UCSR1A = 0x00;
    UCSR1B = (1 << TXEN1)|(1 << RXEN1);
    UCSR1C = (1 << UCSZ11)|(1 << UCSZ10);

    // Set Timer3 to normal mode
    TCCR3A = 0x00;
    // Set the prescaler to 1024
    TCCR3B = (1 << CS32) | (1 << CS30);
    // Set the WLAN timeout to 1 second
    *WLAN_OCR = 0x3d09;
    // Set the UART timeout to 4 seconds
    *UART_OCR = 0xF424;
    // Set the HTTP timeout to 4 seconds
    *HTTP_OCR = 0xF424;

    // Try to bootload using the STK protocol on UART0.
    bootload_from_stk();
    // Try to bootload using the Wi-Fi module on UART1 to fetch a program from
    // the internet.
    bootload_from_internet();
    // If either operation succeeds, the MCU will reset using the Watchdog
    // Timer, then it will skip to the main program.
}

static void bootload_from_internet(void)
{
    do {
        if (boot_state == ENTERING) {
            // Reset the Wi-Fi module in case it was left in a hanging state
            wifly_reset();
            // Switch led color to red
            *RED_LED_PORT |= (1 << RED_LED_PIN);
            *GREEN_LED_PORT &= ~(1 << GREEN_LED_PIN);
#ifdef CHECK_STATUS_BEFORE_DOWNLOAD
            // Check if an update is available
            if (!download_get_status())
                boot_state = JUMPING_TO_APP;
            else
#endif
#ifdef USE_URL_INDIRECTION
            if (!download_get_path())
                boot_state = JUMPING_TO_APP;
            else
#endif
            // Get the size of the HEX file
            if (!download_get_size())
                boot_state = JUMPING_TO_APP;
            else
                boot_state = FILLING_BUFFER;
        }
        else if (boot_state == FILLING_BUFFER) {
            // Switch led color to orange
            *RED_LED_PORT |= (1 << RED_LED_PIN);
            *GREEN_LED_PORT |= (1 << GREEN_LED_PIN);
            if (hex_chunk.file_start == hex_program_size) {
                boot_state = EXITING;
            }
            // Fetch the next chunk of HEX data
            else if (download_get_chunk()) {
                // Reset the HEX buffer indexes
                hex_chunk.size += hex_chunk.index;
                hex_chunk.index = 0;
                boot_state = CHECKING_HEX_LINE;
            }
            else
                boot_state = JUMPING_TO_APP;
        }
        else if (boot_state == CHECKING_HEX_LINE) {
            // Switch led color to orange
            *RED_LED_PORT |= (1 << RED_LED_PIN);
            *GREEN_LED_PORT |= (1 << GREEN_LED_PIN);
            if (ihex_check_line()) {
                boot_state = PARSING_HEX_LINE;
            }
            else {
                hex_chunk.file_start = hex_chunk.file_stop + 1;
                download_append_leftover();
                boot_state = FILLING_BUFFER;
            }
        }
        else if (boot_state == PARSING_HEX_LINE) {
            // Switch led color to orange
            *RED_LED_PORT |= (1 << RED_LED_PIN);
            *GREEN_LED_PORT |= (1 << GREEN_LED_PIN);
            if (bin_page.index == 2*FLASH_PAGE_SIZE) {
                boot_state = WRITING_BIN_PAGE;
            }
            else if (!ihex_load_byte())
                boot_state = CHECKING_HEX_LINE;
        }
        else if (boot_state == WRITING_BIN_PAGE) {
            // Switch led color to green
            *RED_LED_PORT &= ~(1 << RED_LED_PIN);
            *GREEN_LED_PORT |= (1 << GREEN_LED_PIN);
            // Update page byte count
            length.word = bin_page.index;
            // Update target Flash location
            address.word = bin_page.address;
            // Write the binary page to Flash
            write_bin_page();
            // The address is a word location whereas the size is a byte count,
            // so divide it by 2
            bin_page.address += length.word >> 1;
            // Reset the binary page index
            bin_page.index = 0;
            boot_state = PARSING_HEX_LINE;
        }
        else if (boot_state == EXITING) {
            // Switch led color to green
            *RED_LED_PORT &= ~(1 << RED_LED_PIN);
            *GREEN_LED_PORT |= (1 << GREEN_LED_PIN);
            // Update page byte count
            length.word = bin_page.index;
            // Update target Flash location
            address.word = bin_page.address;
            // Write the binary page to Flash
            write_bin_page();
#ifdef CLEAR_STATUS_AFTER_DOWNLOAD
            download_update_status();
#endif
            boot_state = JUMPING_TO_APP;
        }
        else if (boot_state == JUMPING_TO_APP) {
            *GREEN_LED_PORT &= ~(1 << GREEN_LED_PIN);
            *RED_LED_PORT &= ~(1 << RED_LED_PIN);
            // Watchdog Timer reset
            WDTCSR = (1 << WDE);
            while (1);
        }
    } while (1);
}


static void bootload_from_stk(void)
{
    uint16_t b;
    uint8_t ch, ch2;
    // RAMPZ Flag used to indicate memory writes beyond the 64kB boundary
    uint8_t flag_rampz;

     while (!stk_timeout && stk_errors < MAX_STK_ERROR_COUNT) {
        ch = stk_get_char();

        // Get parameter value
        if (ch == STK_GET_PARAMETER) {
            ch2 = stk_get_char();
            // Software major version
            if (ch2 == STK_SW_MAJOR) {
                stk_byte_response(SW_MAJOR);
            }
            // Software minor version
            else if (ch2 == STK_SW_MINOR) {
                stk_byte_response(SW_MINOR);
            }
            // Required by AVR Studio
            else {
                stk_byte_response(NO_TOPCARD_DETECTED);
            }
        }
        // Leave program mode
        else if (ch == STK_LEAVE_PROGMODE) {
            stk_nothing_response();
            // Watchdog Timer reset
            WDTCSR = (1 << WDE);
            while (1);
        }
        // Load word address
        else if (ch == STK_LOAD_ADDRESS) {
            // Address is little endian and is in words
            address.byte[0] = stk_get_char();
            address.byte[1] = stk_get_char();
            stk_nothing_response();
        }
        // Program page
        else if (ch == STK_PROG_PAGE) {
            // Length is big endian and is in bytes
            length.byte[1] = stk_get_char();
            length.byte[0] = stk_get_char();
            stk_get_char();
            // Receive the binary page and store it to the buffer
            for (b = 0; b < length.word; b++) {
                bin_buffer[b] = stk_get_char();
            }
            if (stk_get_char() == STK_CRC_EOP) {
                // Write the binary page to the Flash memory
                write_bin_page();
                stk_put_char(STK_INSYNC);
                stk_put_char(STK_OK);
            }
            else {
                ++stk_errors;
            }
        }
        // Read page
        else if (ch == STK_READ_PAGE) {
            *GREEN_LED_PORT |= (1 << GREEN_LED_PIN);
            // Length is big endian and is in bytes
            length.byte[1] = stk_get_char();
            length.byte[0] = stk_get_char();
            if (address.word > 0x7FFF) {
                flag_rampz = 1;
            }
            else {
                flag_rampz = 0;
            }
            // Since the address sent via STK is the word address, address*2
            // yields the byte address
            address.word = address.word << 1;
            stk_get_char();
            if (stk_get_char() == STK_CRC_EOP) {
                stk_put_char(STK_INSYNC);
                for (b = 0; b < length.word; b++) {
                    if (!flag_rampz) {
                        stk_put_char(pgm_read_byte_near(address.word));
                    }
                    else {
                        stk_put_char(pgm_read_byte_far(address.word +
                            0x10000));
                    }
                    address.word++;
                }
                stk_put_char(STK_OK);
            }
            *GREEN_LED_PORT &= ~(1 << GREEN_LED_PIN);
        }
        // Read signature bytes
        else if (ch == STK_READ_SIGN) {
            if (stk_get_char() == STK_CRC_EOP) {
                stk_put_char(STK_INSYNC);
                stk_put_char(SIG1);
                stk_put_char(SIG2);
                stk_put_char(SIG3);
                stk_put_char(STK_OK);
            }
            else {
                ++stk_errors;
            }
        }
        // Set device programming parameters (ignored)
        else if (ch == STK_SET_DEVICE) {
            stk_get_n_char(20);
            stk_nothing_response();
        }
        // Set extended device programming parameters (ignored)
        else if (ch == STK_SET_DEVICE_EXT) {
            stk_get_n_char(5);
            stk_nothing_response();
        }
        // Universal command (ignored)
        else if (ch == STK_UNIVERSAL) {
            stk_get_n_char(4);
            stk_byte_response(0x00);
        }
        // Other commands are either invalid or ignored
        else {
            stk_nothing_response();
        }
    }
}


/* Increment the error counter then wait a moment */
static bool add_error(uint8_t* count, uint8_t max_count) {
    if (*count >= max_count)
        return false;
    else {
        *count += 1;
        _delay_ms(2000);
        return true;
    }
}

// Move an incomplete HEX line to the beginning of the buffer
static void download_append_leftover(void) {
    uint8_t* data = hex_buffer + hex_chunk.index;
    uint8_t length = hex_chunk.size - hex_chunk.index;

    uint8_t i;
    for (i = 0; i < length; i++) {
        hex_buffer[i] = data[i];
    }
    hex_buffer[i] = 0x00;
    hex_chunk.index = i;
}

/* Download the next HEX page and extract its binary content */
static bool download_get_chunk(void)
{
    return http_send(&request_get_chunk, &download_parse_chunk);
}

/* Get the location of the HEX file */
static bool download_get_path(void)
{
    if (!http_send(&request_get_status, 0))
        return false;
    if (!wifly_find_string(PATH_JSON_PREFIX))
        return false;
    if (!download_parse_path())
        return false;
    else {
        PROGRAM_PATH = path_buffer;
        return true;
    }
}

/* Poll the HTTP server to get the size of the HEX file */
static bool download_get_size(void)
{
    return http_send(&request_get_size, &download_parse_size);
}

/* Send a request to check if a new program is available */
static bool download_get_status(void)
{
    if (!http_send(&request_get_status, 0))
        return false;
    else
        return wifly_find_string(CHECK_STATUS_EXPECTED_RESPONSE);
}

/* Store the incoming data into the HEX page buffer */
static bool download_parse_chunk(void)
{
    uint32_t i;
    uint8_t* dest;
    // Skip HTTP status and header
    if (!wifly_find_string("\r\n\r\n"))
        return false;
    else {
        // Download HTTP response body
        dest = hex_buffer + hex_chunk.index;
        hex_chunk.size = hex_chunk.file_stop - hex_chunk.file_start + 1;
        for (i = 0; i < hex_chunk.size; i++) {
            dest[i] = wifly_get_char();
            if (dest[i] == 0x00) {
                return false;
            }
        }
        // Add a terminating null character
        dest[i] = 0x00;
        return true;
    }
}

/* Parse the value associated to the "path" key in the JSON response */
static bool download_parse_path(void)
{
    uint8_t ch;
    uint8_t i;

    i = 0;
    do {
        ch = wifly_get_char();
        if (i >= PATH_BUFFER_SIZE - 1)
            return false;
        else if (ch == '\\')
            continue;
        else if (ch == '\"')
            break;
        else {
            path_buffer[i++] = ch;
        }
    } while (true);
    path_buffer[i] = '\0';
    return true;
}

/* Parse the Content-Length field from the incoming HTTP header */
static bool download_parse_size(void)
{
    uint8_t i, nb_digits;
    uint32_t result;
    uint8_t ch;
    uint8_t buffer[16];

    // Find the digits in the HTTP input stream
    if (!wifly_find_string("Content-Length: ")) {
        return false;
    }
    else {
        i = 0;
        do {
            ch = wifly_get_char();
            if (ch == 0)
                return false;
            else {
                buffer[i++] = ch;
            }
        } while (ch != '\r');
        buffer[i] = 0x00;
        nb_digits = i - 1;
    }

    // Compute the value represented by these digits
    result = 0;
    for (i = 0; i < nb_digits; i++) {
        ch = buffer[i];
        if (ch < '0' || ch > '9')
            return false;
        else {
            result *= 10;
            result += (uint32_t)(ch - '0');
        }
    }
    hex_program_size = result;
    return (hex_program_size > 0);
}

/* Send a request to confirm that the program was successfully downloaded */
static void download_update_status(void)
{
    http_send(&request_update_status, 0);
}

/* Wait for a response from the server to the last HTTP request sent */
static bool http_await_response(void)
{
    // Reset the Timer/Counter
    TCNT3 = 0;
    // Clear the Compare Match flag by writing a logical one to its location
    TIFR3 |= (1 << HTTP_OCF);
    // The flag is set again on compare match
    while (!(TIFR3 & (1 << HTTP_OCF))) {
        if ((UCSR1A & (1 << RXC1))) {
        return true;
        }
    }
    return false;
}

/* Send an HTTP request and process the response */
static bool http_send(void (*request)(void), bool (*action)(void)) {
    do {
        if (download.state == CHECKING_SOCKET) {
            if (wifly_connect_to_host())
                download.state = SENDING_REQUEST;
            else
                download.state = DOWNLOAD_CRITICAL_ERROR;
        }
        else if (download.state == SENDING_REQUEST) {
            (*request)();
            if (http_await_response())
                download.state = RECEIVING_RESPONSE;
            else
                download.state = HTTP_ERROR;
        }
        else if (download.state == RECEIVING_RESPONSE) {
            if (action == 0) {
                download.state = CHECKING_SOCKET;
                return true;
            }
            else if ((*action)()) {
                download.state = CHECKING_SOCKET;
                return true;
            }
            else
                download.state = HTTP_ERROR;
        }
        else if (download.state == HTTP_ERROR) {
            if (add_error(&download.errors.http, MAX_HTTP_ERRORS))
                download.state = SENDING_REQUEST;
            else
                download.state = DOWNLOAD_CRITICAL_ERROR;
        }
        else if (download.state == DOWNLOAD_CRITICAL_ERROR) {
            if (download.errors.critical >= MAX_DOWNLOAD_CRITICAL_ERRORS)
                return false;
            else {
                ++download.errors.critical;
                download.errors.parse = 0;
                download.errors.http = 0;
                download.state = CHECKING_SOCKET;
            }
        }
    } while (1);
}

/* Check if the next HEX line is complete in the HEX buffer */
static bool ihex_check_line(void)
{
    // Make sure the HEX buffer contains at least:
    // - the start code (1 byte)
    // - the byte count (2 bytes)
    // - the address (4 bytes)
    // - the record type (2 bytes)
    if (hex_chunk.size < hex_chunk.index + 9)
        return false;
    // Check the start code
    if (hex_buffer[hex_chunk.index] != ':')
        return false;
    // Parse the byte count
    line_byte_count = ihex_parse_byte(hex_buffer + hex_chunk.index + 1);
    // Make sure the HEX buffer contains the rest of the line, including the
    // the CRC (2 bytes) and the "\r\n"
    if (hex_chunk.size < hex_chunk.index + 9 + (line_byte_count << 1))
        return false;
    // Parse the record type
    uint16_t record_type = ihex_parse_byte(hex_buffer + hex_chunk.index + 7);
    // Place the index at the beginning of the data frame
    hex_chunk.index += 9;
    // Skip the data of Extended Segment Address records
    if (record_type == 0x02) {
        line_byte_count = 0;
        hex_chunk.index += 4;
    }
    return true;
}

/* Load one byte of binary content to the page buffer */
static bool ihex_load_byte(void)
{
    if (line_byte_count == 0) {
        // Skip the CRC and the "\r\n"
        hex_chunk.index += 4;
        return false;
    }
    else {
        // Copy one byte of the HEX line content to the binary page buffer
        bin_buffer[bin_page.index] =
            ihex_parse_byte(hex_buffer + hex_chunk.index);
        bin_page.index += 1;
        hex_chunk.index += 2;
        line_byte_count -= 1;
        return true;
    }
}

static uint8_t ihex_parse_byte(uint8_t* source)
{
    uint8_t byte_value;
    uint8_t* data;
    uint8_t ch;

    data = source;
    // Convert first digit to first nibble
    ch = *data++;
    if (ch >= '0' && ch <= '9')
        byte_value = ch - '0';
    else if (ch >= 'A' && ch <= 'F')
        byte_value = ch - 'A' + 10;
    else {
        return false;
    }
    byte_value <<= 4;
    // Convert second digit to second nibble
    ch = *data++;
    if (ch >= '0' && ch <= '9')
        byte_value += ch - '0';
    else if (ch >= 'A' && ch <= 'F')
        byte_value += ch - 'A' + 10;
    else {
        return false;
    }
    return byte_value;
}

/*
 * Send a partial GET request to the server in order to receive the next page
 * of the HEX file
 */
static void request_get_chunk(void)
{
    // Compute the position of the next program page within the HEX file
    hex_chunk.file_stop =
        hex_chunk.file_start - hex_chunk.index+ HEX_BUFFER_SIZE - 1;
    if (hex_chunk.file_stop >= hex_program_size)
        hex_chunk.file_stop = hex_program_size - 1;
    // Send HTTP GET range request
    wifly_put_string("GET ");
    wifly_put_string(PROGRAM_PATH);
    wifly_put_string(HTTP_FIELDS);
    wifly_put_string("Range: bytes=");
    wifly_put_long(hex_chunk.file_start);
    wifly_put_string("-");
    wifly_put_long(hex_chunk.file_stop);
    wifly_put_string(
        "\r\n"
        "\r\n"
    );
}

/* Send a HEAD request about the HEX file to the server */
static void request_get_size(void)
{
    wifly_put_string("HEAD ");
    wifly_put_string(PROGRAM_PATH);
    wifly_put_string(HTTP_FIELDS);
    wifly_put_string("\r\n");

}

/* Send a request to check if a new program is available */
static void request_get_status(void)
{
    wifly_put_string(CHECK_STATUS_REQUEST);
    wifly_put_string(HTTP_FIELDS);
    wifly_put_string("\r\n");
}

/* Send a request to confirm that the program was successfully downloaded */
static void request_update_status(void)
{
    wifly_put_string(CLEAR_STATUS_REQUEST);
    wifly_put_string(HTTP_FIELDS);
    wifly_put_string("\r\n");
}

/* Send a byte response to the programmer */
static void stk_byte_response(uint8_t val)
{
    if (stk_get_char() == STK_CRC_EOP) {
        stk_put_char(STK_INSYNC);
        stk_put_char(val);
        stk_put_char(STK_OK);
    }
    else {
        ++stk_errors;
    }
}

/* Read a byte from the programmer */
static uint8_t stk_get_char(void)
{
    uint32_t cycle_count = 0;
    while (!(UCSR0A & (1 << RXC0))) {
        cycle_count++;
        if (cycle_count > MAX_TIME_COUNT) {
            stk_timeout = true;
            return 0;
        }
    }
    return UDR0;
}

/* Discard a number of bytes from the programmer */
static void stk_get_n_char(uint8_t count)
{
  while (count--) {
    while (!(UCSR0A & (1 << RXC0)));
    UDR0;
  }
}

/* Send an empty response to the programmer */
static void stk_nothing_response(void)
{
    if (stk_get_char() == STK_CRC_EOP) {
        stk_put_char(STK_INSYNC);
        stk_put_char(STK_OK);
    }
    else {
        ++stk_errors;
    }
}

/* Send a byte to the programmer */
static void stk_put_char(uint8_t ch)
{
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = ch;
}

/* Check if the WiFly is still associated with the access point */
static bool wifly_check_socket(void)
{
    // Reset the Timer/Counter
    TCNT3 = 0;
    // Clear the Compare Match Flag by writing a logical one to its location
    TIFR3 |= (1 << WLAN_OCF);
    while (!(TIFR3 & (1 << WLAN_OCF))) {
        // The WiFly drives its GPIO6 pin to HIGH when connected
        if (*GPIO6_PORT_INPUT & (1 << GPIO6_PIN))
            return true;
    }
    return false;
}

/* Check if the WiFly is still associated with the access point */
static bool wifly_check_wlan(void)
{
    // Reset the Timer/Counter
    TCNT3 = 0;
    // Clear the Compare Match Flag by writing a logical one to its location
    TIFR3 |= (1 << WLAN_OCF);
    while (!(TIFR3 & (1 << WLAN_OCF))) {
        // The WiFly drives its GPIO4 pin to LOW when associated
        if (*GPIO4_PORT_INPUT & (1 << GPIO4_PIN))
            return true;
    }
    return false;
}

/* Force the WiFly module to close the connection */
static void wifly_close_socket(void)
{
    while (*GPIO6_PORT_INPUT & (1 << GPIO6_PIN))
        *GPIO5_PORT &= ~(1 << GPIO5_PIN);
}

/* Make sure the WiFly is in command mode */
static bool wifly_connect_to_host(void)
{
    do {
        if (wifly.state == RESETTING) {
            wifly_reset();
            wifly.state = SETTING_HOST;
        }
        else if (wifly.state == SETTING_HOST) {
            wifly_enter_command_mode();
            if (wifly_set_host()) {
                wifly.state = JOINING_WLAN;
            }
            else if (!add_error(&wifly.errors.command, MAX_COMMAND_ERRORS)) {
                wifly.state = WIFLY_CRITICAL_ERROR;
            }
        }
        else if (wifly.state == JOINING_WLAN) {
            if (!(*GPIO4_PORT_INPUT & (1 << GPIO4_PIN)))
                wifly_join_wlan();
            if (wifly_check_wlan()) {
                wifly.state = OPENING_SOCKET;
            }
            else if (!add_error(&wifly.errors.wlan, MAX_WLAN_ERRORS))
                wifly.state = WIFLY_CRITICAL_ERROR;
        }
        else if (wifly.state == OPENING_SOCKET) {
            if (!(*GPIO6_PORT_INPUT & (1 << GPIO6_PIN)))
                wifly_open_socket();
            if (wifly_check_socket()) {
                wifly.state = OPENING_SOCKET;
                return true;
            }
            else {
                wifly_close_socket();
                if (!add_error(&wifly.errors.socket, MAX_SOCKET_ERRORS))
                    wifly.state = WIFLY_CRITICAL_ERROR;
            }
        }
        else if (wifly.state == WIFLY_CRITICAL_ERROR) {
            if (wifly.errors.critical >= MAX_WIFLY_CRITICAL_ERRORS) {
                return false;
            }
            else {
                ++wifly.errors.critical;
                wifly.errors.wlan = 0;
                wifly.errors.command = 0;
                wifly.state = RESETTING;
            }
        }
    } while (1);
}

/* Ask the WiFly to enter command mode */
static void wifly_enter_command_mode(void)
{
    _delay_ms(250);
    wifly_put_string("$$$");
    wifly_find_string("CMD");
}

/* Scan the stream coming from the WiFly and look for a specific string */
static bool wifly_find_string(const char* target)
{
    uint8_t length = 0;
    uint8_t index = 0;
    uint8_t ch;

    // Find target string length
    while (target[index++] != 0x00)
        ++length;

    // Scan incoming characters
    index = 0;
    while (1) {
        if (index == length) {
            // The whole target string has been found
            return true;
        }
        else
            ch = wifly_get_char();
        // UART timeouts yield null chars
        if (ch == 0x00)
            return false;
        else if (ch == target[index])
            ++index;
        else
            index = 0;
    }
}

/* Read a byte from the WiFly */
static uint8_t wifly_get_char(void)
{
    // Reset the Timer/Counter
    TCNT3 = 0;
    // Clear the Compare Match flag by writing a logical one to its location
    TIFR3 |= (1 << UART_OCF);
    // The flag is set again on compare match
    while (!(TIFR3 & (1 << UART_OCF))) {
      if ((UCSR1A & (1 << RXC1))) {
        return UDR1;
      }
    }
    return 0;
}

/* Command the WiFly to join the WLAN stored in memory */
static void wifly_join_wlan(void)
{
    wifly_put_string("join\r");
}

/* Command the WiFly to open a TCP socket to the server */
static void wifly_open_socket(void)
{
    // The WiFly opens a TCP socket to the stored host when its GPIO5 pin is
    // driven to HIGH
    *GPIO5_PORT |= (1 << GPIO5_PIN);
}

/* Send a byte to the WiFly */
static void wifly_put_char(uint8_t ch)
{
    while (!(UCSR1A & (1 << UDRE1)));
    UDR1 = ch;
}

/* Send a string to the WiFly */
static void wifly_put_string(const char* source)
{
    uint8_t index = 0;
    while (source[index] != 0x00)
        wifly_put_char(source[index++]);
}

/* Custom unsigned-to-ascii conversion */
static void wifly_put_long(uint32_t number)
{
    uint8_t index;

    utoa_buffer[6] = 0x00;
    index = 6;
    do {
        --index;
        utoa_buffer[index] = '0' + (uint8_t)(number % 10);
        number /= 10;
    } while (number != 0 && index > 0);
    wifly_put_string(utoa_buffer + index);
}

/* Perform a hardware reset */
static void wifly_reset(void)
{
    *RESET_PORT &= ~(1 << RESET_PIN);
    _delay_ms(1);
    *RESET_PORT |= (1 << RESET_PIN);
    // Boot time is 150 ms for the RN171
    _delay_ms(150);
}

/* Set the program host */
static bool wifly_set_host(void)
{
    wifly_put_string("set ip remote 80\r");
    if (!wifly_find_string("AOK"))
        return false;
    wifly_put_string("set dns name " PROGRAM_HOST "\r");
    return wifly_find_string("AOK");
}

/*
 * Write a binary page to the Flash memory
 * This function comes from the ATmegaBOOT bootloader.
 */
static void write_bin_page(void)
{
    if (address.byte[1] > 127) {
        RAMPZ = 0x01;
    }
    else {
        RAMPZ = 0x00;
    }
    // Since the address sent via STK is the word address, address*2 yields
    // the byte address
    address.word = address.word << 1;
    // Even up an odd number of bytes
    if ((length.byte[0] & 0x01))
        length.word++;

    cli();
    asm volatile(
    // Clear register R17 which will be used as word counter
    "clr   r17              \n\t"
    // Load the target Flash address to pointer register Z (R31:R30)
    "lds   r30,address      \n\t"
    "lds   r31,address+1    \n\t"
    // Load the binary buffer address to pointer register Y (R29:R28)
    "ldi   r28,lo8(bin_buffer)\n\t"
    "ldi   r29,hi8(bin_buffer)\n\t"
    // Load the page length to register pair R25:R24
    "lds   r24,length       \n\t"
    "lds   r25,length+1     \n\t"

    // Main loop, repeat for each word in the page
    "length_loop:           \n\t"

    // If the word count is not 0, it means the page is already in the process
    // of being loaded to the temporary page buffer, therefore the Page Erase
    // operation must be skipped
    "cpi   r17,0x00         \n\t"
    "brne  no_page_erase    \n\t"

    // Else it means the current page is a new one and it needs to be erased
    // before it can be overwritten
    // Wait for the previous SPM instruction to complete
    "wait_spm1:             \n\t"
    "lds   r16,%0           \n\t"
    "andi  r16,1            \n\t"
    "cpi   r16,1            \n\t"
    "breq  wait_spm1        \n\t"
    // See the ATmega1280 manual, section 28.6.1: Performing Page Erase by SPM
    "ldi   r16,0x03         \n\t"
    "sts   %0,r16           \n\t"
    "spm                    \n\t"
    // Wait for the previous SPM instruction to complete
    "wait_spm2:             \n\t"
    "lds   r16,%0           \n\t"
    "andi  r16,1            \n\t"
    "cpi   r16,1            \n\t"
    "breq  wait_spm2        \n\t"
    // Re-enable the Read-While-Write section
    "ldi   r16,0x11         \n\t"
    "sts   %0,r16           \n\t"
    "spm                    \n\t"
    "no_page_erase:         \n\t"

    // Carry on loading the current page: load the next instruction
    // word from SRAM to R1:R0
    "ld    r0,Y+            \n\t"
    "ld    r1,Y+            \n\t"
    // Wait for the previous SPM instruction to complete
    "wait_spm3:             \n\t"
    "lds   r16,%0           \n\t"
    "andi  r16,1            \n\t"
    "cpi   r16,1            \n\t"
    "breq  wait_spm3        \n\t"
    // See the ATmega1280 manual, section 28.6.2: Filling the Temporary Buffer
    "ldi   r16,0x01         \n\t"
    "sts   %0,r16           \n\t"
    "spm                    \n\t"
    // Increment the word counter
    "inc   r17              \n\t"

    // If the word count is lower than the page size, it means the current
    // page is still in the process of being loaded to the temporary page
    // buffer, therefore the Page Write operation must be skipped
    "cpi   r17,%1           \n\t"
    "brlo  no_page_write    \n\t"

    // Else the word count has reached the page size, which means the page has
    // been fully loaded to the temporary page buffer and it is time to
    // perform a Page Write operation
    "write_page:            \n\t"
    // Reset the word counter to 0x00
    "clr   r17              \n\t"
    // Wait for the previous SPM instruction to complete
    "wait_spm4:             \n\t"
    "lds   r16,%0           \n\t"
    "andi  r16,1            \n\t"
    "cpi   r16,1            \n\t"
    "breq  wait_spm4        \n\t"
    // See the ATmega1280 manual, section 28.6.3: Performing a Page Write
    "ldi   r16,0x05         \n\t"
    "sts   %0,r16           \n\t"
    "spm                    \n\t"
    // Wait for the previous SPM instruction to complete
    "wait_spm5:             \n\t"
    "lds   r16,%0           \n\t"
    "andi  r16,1            \n\t"
    "cpi   r16,1            \n\t"
    "breq  wait_spm5        \n\t"
    // Re-enable the Read-While-Write section
    "ldi   r16,0x11         \n\t"
    "sts   %0,r16           \n\t"
    "spm                    \n\t"
    "no_page_write:         \n\t"

    // Increment target Flash address
    "adiw  r30,0x02         \n\t"
    // Decrement the the number of bytes to write
    "sbiw  r24,0x02         \n\t"
    // If the number of bytes to write is 0, prepare for final write
    "breq  final_write      \n\t"
    // Else start the main loop over
    "rjmp  length_loop      \n\t"

    // If the bootloader arrives here, it means the page has been
    // fully loaded to the temporary page buffer
    "final_write:           \n\t"
    // If on top of that the word count is 0, it means the buffer has
    // been written to memory, so there is nothing left to do
    "cpi   r17,0            \n\t"
    "breq  block_done       \n\t"
    // Else the current page is not full and has not yet been written, so
    // one last Page Write operation is necessary
    // Increment the value of R24 by 2 to fool the above check on length
    "adiw  r24,2            \n\t"
    // Jump to the Page Write operation
    "rjmp  write_page       \n\t"

    "block_done:            \n\t"
    // Restore register R0 and exit
    "clr   __zero_reg__     \n\t"
    : "=m" (SPMCSR)
    : "M" (FLASH_PAGE_SIZE)
    : "r0","r16","r17","r24","r25","r28","r29","r30","r31"
    );
}
