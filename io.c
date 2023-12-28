#include <memory.h>
#include <pico/printf.h>
#include "io.h"

#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "pico/stdlib.h"
#include "hardware/timer.h"

#define BAUD 115200
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY UART_PARITY_NONE

#define MAX_DATA_LEN 255
#define RX_HEADER_SIZE sizeof(response_header_t)
#define FRAME_BYTE 0x7E
#define UNSTUFF_MARK 0x7D
#define LEN_INDEX 3

#define TIMEOUT_US 20000

typedef enum {
    ST_START,
    ST_HEADER,
    ST_DATA,
    ST_SUM,
    ST_END,
    ST_IDLE
} isr_rx_state_t;

struct {
    uint8_t data[MAX_DATA_LEN];
    uint8_t header[RX_HEADER_SIZE];
    uint8_t header_pos;
    uint8_t data_pos;
    uint8_t data_len;
    bool unstuff;
    bool overflow;
    bool complete;
    uint8_t sum;
    io_flags_t err_flags;
    uart_inst_t *uart;
    isr_rx_state_t state;
} io;


static void uart_rx_reset(uart_inst_t *uart){
    io.state = ST_START;
    io.header_pos = 0;
    io.data_len = 0;
    io.data_pos = 0;
    io.sum = 0;
    io.err_flags = E_IO_OK;
    io.unstuff = false;
    io.complete = false;
    io.overflow = false;
    io.uart = uart;
}

static inline uint8_t unstuff(uint8_t byte){
    switch (byte) {
        case 0x31: return 0x11;
        case 0x33: return 0x13;
        case 0x5D: return 0x7D;
        case 0x5E: return 0x7E;
        default: return byte;
    }
}


static inline void io_handle_byte(uint8_t byte) {
#define UNSTUFF if(io.unstuff) {byte = unstuff(byte); io.unstuff = false; }\
        else if(byte == UNSTUFF_MARK){ io.unstuff=true; return;}

    switch (io.state) {
        case ST_START:{
            if(byte == FRAME_BYTE) {
                io.state = ST_HEADER;
            }
            return;
        }

        case ST_HEADER:{
            UNSTUFF

            io.header[io.header_pos] = byte;
            if(io.header_pos == LEN_INDEX) {
                io.data_len = byte;
                if(io.data_len) {
                    io.state = ST_DATA;
                }else{
                    io.state = ST_SUM;
                }
                return;
            }
            io.header_pos ++;
            return;
        }

        case ST_DATA:{
            UNSTUFF
            io.data[io.data_pos++] = byte;
            if(io.data_pos >= io.data_len) {
                io.state = ST_SUM;
            }
            return;
        }

        case ST_SUM:{
            UNSTUFF
            io.sum = byte;
            io.state = ST_END;
            return;
        }

        case ST_END: {
            if(byte != FRAME_BYTE){
                io.err_flags |= E_IO_NO_END;
            }
            io.state = ST_IDLE;
            io.complete = true;
            return;
        }

        case ST_IDLE:{
            io.err_flags |= E_IO_EXTRA;
            return;
        }
    }
}

static uint8_t checksum(uint8_t header_sum, uint8_t data_len, const uint8_t* data){
    header_sum += data_len;
    for(uint8_t i=0; i < data_len; i++){
        header_sum += data[i];
    }
    return ~header_sum;
}

// interrupt service routine for readable data
static void uart_rx_isr(){
    while(uart_is_readable(io.uart)) {
        io_handle_byte(uart_getc(io.uart));
    }
}

static inline void send_byte_stuffed(uart_inst_t *uart, uint8_t byte) {
    switch (byte) {
        case 0x11:
        case 0x13:
        case 0x7d:
        case 0x7e:
            // byte stuffing is done by inserting 0x7d and inverting bit 5
            uart_putc(uart, 0x7D);
            uart_putc(uart, byte ^ (1 << 5));
            break;
        default:
            uart_putc(uart, byte);
            break;
    }
}

void send_command(uart_inst_t *tx, command_t command, uint8_t data_len, const uint8_t *data) {
    uint8_t cks = checksum((uint8_t) command, data_len, data);
    uart_putc(tx, FRAME_BYTE);

    send_byte_stuffed(tx, 0x00); // ADR is always 0
    send_byte_stuffed(tx, (uint8_t) command);
    send_byte_stuffed(tx, data_len);
    for(uint8_t i=0; i < data_len; i ++) send_byte_stuffed(tx, data[i]);
    send_byte_stuffed(tx, cks);

    uart_putc(tx, 0x7E);
}

io_error_t send_wakeup(uart_inst_t *uart) {
    uart_putc(uart, 0xFF);
    send_command(uart, WAKE, 0, NULL);
    response_header_t r;
    io_error_t rc = read_response(uart, &r, 0, NULL);
    if((r.status & 0x7F) != 0) {

    }
    return rc;
}

io_error_t read_response(uart_inst_t *rx, response_header_t *header, uint8_t max_data_len, uint8_t *data) {
    uart_rx_reset(rx);

    uint32_t t = time_us_32();

    while(!io.complete && (time_us_32() - t < TIMEOUT_US)){
        while(uart_is_readable(rx)){
            io_handle_byte(uart_getc(rx));
        }
        tight_loop_contents();
    }

    io_error_t rc = OK;
    memcpy(header, io.header, RX_HEADER_SIZE);

    if(!io.complete) {
        E_SET_FLAG(rc, E_IO_TIMEOUT);
    }

    E_SET_FLAG(rc, io.err_flags);

    uint8_t cks = checksum(header->addr + header->cmd + header->status, header->data_len, io.data);

    if(io.sum != cks) {
        printf("bad checksum got %X but expected %X\n", io.sum, cks);
        E_SET_FLAG(rc, E_IO_CHECKSUM);
    }

    if(header->data_len > 0) {
        if (max_data_len < header->data_len) {
            E_SET_FLAG(rc, E_IO_OVERFLOW);
            header->data_len = max_data_len;
        }
        if (header->data_len > 0) {
            memcpy(data, io.data, header->data_len);
        }
    }

    if((header->status & 0x80) != 0) {
        E_SET_STATUS(rc, header->status & 0x7F);
    }

    return rc;
}

void print_error(io_error_t e){
    if(IS_OK(e)) {
        printf("OK");
        return;
    }

    printf("[status=%s,flags=", device_status_to_string(E_STATUS(e)));
    bool first = true;
    for(int i=0; i < IO_FLAGS_MAX_SHIFT; i ++){
        if(E_HAS_FLAG(e, 1 << i)) {
            if(!first) {
                printf("|");
            }else{
                first = false;
            }
            printf("%s", flag_to_string(1 << i));
        }
    }
    printf("]");
}

const char *flag_to_string(io_flags_t f) {
    switch (f) {
        case E_IO_OK: return "OK";
        case E_IO_TIMEOUT: return "E_IO_TIMEOUT";
        case E_IO_CHECKSUM: return "E_IO_CHECKSUM";
        case E_IO_OVERFLOW: return "E_IO_OVERFLOW";
        case E_NO_SENSOR_DATA: return "E_NO_SENSOR_DATA";
        case E_IO_NO_END: return "E_IO_NO_END";
        case E_IO_EXTRA: return "E_IO_EXTRA";
        default: return "UNKNOWN";
    }
}

const char *device_status_to_string(device_status_t s) {
    switch (s) {
        case S_OK: return "OK";
        case S_BAD_LEN: return "S_BAD_LEN";
        case S_UNKNOWN_CMD: return "S_UNKNOWN_CMD";
        case S_NO_ACCESS: return "S_NO_ACCESS";
        case S_BAD_PARAM: return "S_BAD_PARAM";
        case S_INTERNAL: return "S_INTERNAL";
        case S_STATE: return "S_STATE";
        default: return "UNKNOWN";
    }
}

void init_uart(uart_inst_t *uart, uint tx_pin, uint rx_pin){
    uart_init(uart, BAUD);
    gpio_set_function(tx_pin, GPIO_FUNC_UART);
    gpio_set_function(rx_pin, GPIO_FUNC_UART);
    uart_set_format(uart, DATA_BITS, STOP_BITS, PARITY);
    uart_set_hw_flow(uart, false, false);
}
