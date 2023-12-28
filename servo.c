//
// Created by jon on 11/20/23.
//

#include <pico/printf.h>
#include "servo.h"
#include "pico.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"

#define DBG printf
#define WRN printf

#define CLKDIV 255.0f
#define WRAP 9804
#define LEVEL_MIN 225
#define LEVEL_MAX 1150


// returns -1 if angle is out of bounds
static int16_t server_calc_level_f(float angle) {
    if(angle < -90 || angle > 90) {
        WRN("invalid angle %f\n", angle);
        return -1;
    }

    float level_f = (angle + 90.0f) / 180.0f;

    return (int16_t) ( (float) LEVEL_MIN + level_f * (float) (LEVEL_MAX - LEVEL_MIN) );
}

void servo_init(uint pin){
    gpio_set_function(pin, GPIO_FUNC_PWM);

    uint slice_num = pwm_gpio_to_slice_num(pin);

    pwm_set_clkdiv(slice_num, CLKDIV);
    pwm_set_wrap(slice_num, WRAP);
    pwm_set_enabled(slice_num, true);
}

static void servo_set_level_int(uint pin, uint16_t level) {
    uint slice_num = pwm_gpio_to_slice_num(pin);
    uint channel = pwm_gpio_to_channel(pin);
    pwm_set_chan_level(slice_num, channel, level);
}

int8_t servo_set_angle(uint pin, float angle_degrees) {
    int16_t level = server_calc_level_f(angle_degrees);
    if(level < 0){
        return -1;
    }
    DBG("angle=%f, level=%d\n", angle_degrees, level);
    servo_set_level_int(pin, (uint16_t) level);
}


