#include "stdint.h"
#include "stdio.h"
typedef uint8_t bool;
#define false 0
#define true 1

typedef struct {
    uint8_t addr;
    uint8_t cmd;
    uint8_t status;
    uint8_t data_len;
} response_header_t;

#define MAX_DATA_LEN 255
#define RX_HEADER_SIZE sizeof(response_header_t)
#define FRAME_BYTE 0x7E
#define UNSTUFF_MARK 0x7D
#define LEN_INDEX 3

typedef enum {
    E_IO_OK = 0,
    E_IO_NO_END = 1 << 1,
    E_IO_EXTRA = 1 << 2
} io_err_t;

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
    io_err_t err_flags;
    isr_rx_state_t state;
} isr;


static void uart_rx_reset( ){
    isr.state = ST_START;
    isr.header_pos = 0;
    isr.data_len = 0;
    isr.data_pos = 0;
    isr.sum = 0;
    isr.err_flags = E_IO_OK;
    isr.unstuff = false;
    isr.complete = false;
    isr.overflow = false;
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


static inline void isr_handle_byte(uint8_t byte) {
#define UNSTUFF if(isr.unstuff) {byte = unstuff(byte); isr.unstuff = false; }\
        else if(byte == UNSTUFF_MARK){ isr.unstuff=true; return;}

    switch (isr.state) {
        case ST_START:{
            if(byte == FRAME_BYTE) {
                isr.state = ST_HEADER;
            }
            return;
        }

        case ST_HEADER:{
            UNSTUFF

            isr.header[isr.header_pos] = byte;
            if(isr.header_pos == LEN_INDEX) {
                isr.data_len = byte;
                if(isr.data_len) {
                    isr.state = ST_DATA;
                }else{
                    isr.state = ST_SUM;
                }
                return;
            }
            isr.header_pos ++;
            return;
        }

        case ST_DATA:{
            UNSTUFF
            isr.data[isr.data_pos++] = byte;
            if(isr.data_pos >= isr.data_len) {
                isr.state = ST_SUM;
            }
            return;
        }

        case ST_SUM:{
            UNSTUFF
            isr.sum = byte;
            isr.state = ST_END;
            return;
        }

        case ST_END: {
            if(byte != FRAME_BYTE){
                isr.err_flags |= E_IO_NO_END;
            }
            isr.state = ST_IDLE;
            isr.complete = true;
            return;
        }

        case ST_IDLE:{
            isr.err_flags |= E_IO_EXTRA;
            return;
        }
    }
}

void print_buf(const char *start, uint8_t *buf, uint16_t n) {
    printf("%s", start);
    while(n--) {
        printf(" 0x%02X", *(buf ++));
    }
    printf("\n");
}

void test_buf(uint8_t *in, uint16_t in_len) {
    print_buf("test input", in, in_len);
    while(in_len-- && !isr.complete) {
        isr_handle_byte(*(in ++));
    }
    if(!isr.complete) {
        printf("not finished! in state %d\n", isr.state);
    }
    response_header_t *header = (response_header_t *) isr.header;
    printf("header={addr=0x%02X,cmd=0x%02X,status=0x%02X,data_len=%d}\n",
           header->addr, header->cmd, header->status, header->data_len);
    print_buf("data", isr.data, header->data_len);


}

static uint8_t EMPTY[] = {0x7E, 0x00, 0x01, 0x00, 0x00, 0xFE, 0x7E};
static uint8_t BIG[] = {0x7E, 0x00, 0x03, 0x00, 0x28, 0x7D, 0x5E, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00,0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00,0x00 ,0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0xD4, 0x00, 0x7E};
int main(int argc, const char **argv) {
    uart_rx_reset();
    test_buf(EMPTY, 7);
    uart_rx_reset();
    test_buf(BIG, sizeof(BIG));
}