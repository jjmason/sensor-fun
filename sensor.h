#ifndef PICA_SENSOR_H
#define PICA_SENSOR_H
#ifdef __cplusplus
extern "C" {
#endif


#include "io.h"

typedef struct measurements {
    float mc_1p0;
    float mc_2p5;
    float mc_4p0;
    float mc_10p0;
    float nc_0p5;
    float nc_1p0;
    float nc_2p5;
    float nc_4p0;
    float nc_10p0;
    float typical_particle_size;
} measurements_t;

void print_measurements(measurements_t *m);

io_error_t start_measurements(uart_inst_t *uart);
io_error_t stop_measurements(uart_inst_t *uart);
io_error_t wakeup(uart_inst_t *uart);
io_error_t read_measurements(uart_inst_t *uart, measurements_t *measurements);
io_error_t start_auto_cleaning();
io_error_t get_auto_clean_interval_seconds(uint32_t *interval_out);
io_error_t set_auto_clean_interval_seconds(uint32_t interval);

#ifdef __cplusplus
}
#endif
#endif //PICA_SENSOR_H
