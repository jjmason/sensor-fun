#ifndef _IO_H
#define _IO_H

#ifdef __cplusplus
extern "C" {
#endif


#include "pico.h"
#include "hardware/uart.h"

typedef enum io_flags {
    E_IO_OK = 0,
    E_IO_NO_END = 1 << 0,
    E_IO_EXTRA = 1 << 1,
    E_IO_TIMEOUT = 1 << 2,
    E_IO_CHECKSUM = 1 << 3,
    E_IO_OVERFLOW = 1 << 4,
    E_NO_SENSOR_DATA = 1 << 5
} io_flags_t;

#define IO_FLAGS_MAX_SHIFT 5

typedef enum device_status {
    S_OK = 0x00,
    S_BAD_LEN = 0x01,
    S_UNKNOWN_CMD = 0x02,
    S_NO_ACCESS = 0x03,
    S_BAD_PARAM= 0x04,
    S_INTERNAL = 0x40,
    S_STATE = 0x43
} device_status_t;

typedef union {
    uint16_t v;
    struct {
        uint8_t flags;
        uint8_t status;
    } s;
} io_error_t;

void print_error(io_error_t e);

const char *flag_to_string(io_flags_t f);
const char *device_status_to_string(device_status_t s);

#define OK ((io_error_t) (uint16_t) 0)
#define IS_OK(e) ((e).v == 0)
#define E_FLAGS(e) ((io_flags_t) ((e).s.flags))
#define E_SET_FLAG(e,f) ((e).s.flags |= (f))
#define E_HAS_FLAG(e,f) (((e).s.flags & (f)) != 0)
#define E_HAS_ONE_FLAG(e,f) ((e).s.status == 0 && (e).s.flags == f)
#define E_STATUS(e) ((device_status_t) ((e).s.status))
#define E_SET_STATUS(e,st) ((e).s.status = (uint8_t)(st))


typedef struct {
    uint8_t addr;
    uint8_t cmd;
    uint8_t status;
    uint8_t data_len;
} response_header_t;

typedef enum {
    START=0x00,
    STOP=0x01,
    READ=0x03,
    SLEEP=0x10,
    WAKE=0x11,
    FAN_CLEAN=0x56,
    AUTO_CLEAN_INTERVAL=0x80,
    DEVICE_INFO=0xD0,
    DEVICE_VERSION=0xD1,
    DEVICE_STATUS=0xD2,
    RESET=0xD3,

    // sentinal
    NONE=0xFF
} command_t;

io_error_t read_response(uart_inst_t *rx, response_header_t *header, uint8_t max_data_len, uint8_t *data);

void send_command(uart_inst_t *tx, command_t command, uint8_t data_len, const uint8_t *data);

void init_uart(uart_inst_t *uart, uint rx_pin, uint tx_pin);

io_error_t send_wakeup(uart_inst_t *uart);

#ifdef __cplusplus
}
#endif
#endif