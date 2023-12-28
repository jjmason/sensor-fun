#include <pico/printf.h>
#include "sensor.h"


static const uint8_t START_COMMAND_DATA[] = {0x01, 0x03};

static uint32_t u32_from_bytes(const uint8_t *data) {
    return (uint32_t)data[0] << 24 | (uint32_t)data[1] << 16 |
           (uint32_t)data[2] << 8 | (uint32_t)data[3];
}

static float float_from_bytes(const uint8_t *data) {
    union {
        uint32_t u;
        float f;
    } tmp;

    tmp.u = u32_from_bytes(data);
    return tmp.f;
}

static io_error_t exec_command(
        uart_inst_t *uart,
        command_t command,
        uint8_t in_data_len,
        const uint8_t *in_data,
        uint8_t max_out_data_len,
        uint8_t *out_data,
        uint8_t *received_data_len) {
    response_header_t h;
    send_command(uart, command, in_data_len, in_data);
    io_error_t rc = read_response(uart, &h, max_out_data_len, out_data);
    *received_data_len = h.data_len;
    return rc;
}

io_error_t start_measurements(uart_inst_t *uart) {
    uint8_t dl;
    return exec_command(uart, START, sizeof(START_COMMAND_DATA),
                        START_COMMAND_DATA, 0, NULL, &dl);
}

io_error_t stop_measurements(uart_inst_t *uart) {
    uint8_t dl;
    return exec_command(uart, STOP, 0, NULL, 0, NULL, &dl);
}

io_error_t read_measurements(uart_inst_t *uart, measurements_t* measurements) {
    uint8_t buf[10][4];
    uint8_t len;
    io_error_t e = exec_command(uart, READ, 0, NULL, sizeof(buf), (uint8_t *) buf, &len);
    if(len != sizeof(buf)) {
        E_SET_FLAG(e, E_NO_SENSOR_DATA);
        return e;
    }

    measurements->mc_1p0 = float_from_bytes(buf[0]);
    measurements->mc_2p5 = float_from_bytes(buf[1]);
    measurements->mc_4p0 = float_from_bytes(buf[2]);
    measurements->mc_10p0 = float_from_bytes(buf[3]);
    measurements->nc_0p5 = float_from_bytes(buf[4]);
    measurements->nc_1p0 = float_from_bytes(buf[5]);
    measurements->nc_2p5 = float_from_bytes(buf[6]);
    measurements->nc_4p0 = float_from_bytes(buf[7]);
    measurements->nc_10p0 = float_from_bytes(buf[8]);
    measurements->typical_particle_size = float_from_bytes(buf[9]);
    return e;
}

void print_measurements(measurements_t *m) {
    printf("measurements {\n");
    printf("M  1.0: %.4f\n", m->mc_1p0);
    printf("M  2.5: %.4f\n", m->mc_2p5);
    printf("M  4.0: %.4f\n", m->mc_4p0);
    printf("M 10.0: %.4f\n", m->mc_10p0);
    printf("N  0.5: %.4f\n", m->nc_0p5);
    printf("N  1.0: %.4f\n", m->nc_1p0);
    printf("N  2.5: %.4f\n", m->nc_2p5);
    printf("M  4.0: %.4f\n", m->nc_4p0);
    printf("M 10.0: %.4f\n", m->nc_10p0);
    printf("SIZE  : 0%.4f\n", m->typical_particle_size);
    printf("}\n");

}

io_error_t wakeup(uart_inst_t *uart){
    // pull the line high to start wakeup
    uart_putc(uart, 0xFF);
    uint8_t dl;
    return exec_command(uart, WAKE, 0, NULL, 0, NULL, &dl);
}