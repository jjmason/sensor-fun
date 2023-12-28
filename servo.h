#ifndef PICA_SERVO_H
#define PICA_SERVO_H
#ifdef __cplusplus
extern "C" {
#endif

#include "pico.h"

void servo_init(uint gpio_pin);

int8_t servo_set_angle(uint gpio_pin, float angle_degrees);

#ifdef __cplusplus
}
#endif
#endif //PICA_SERVO_H
