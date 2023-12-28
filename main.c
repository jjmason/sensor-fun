#include "lwipopts.h"
#include <stdio.h>
#include <string.h>
#include <pico/cyw43_arch.h>
#include <pico/stdlib.h>
#include "io.h"
#include "sensor.h"
#include "pico.h"
#include "servo.h"
#include "net.h"
#include "aqi.h"
#include "comms.h"
#include "data.h"

#define UART uart1
#define UART_RX 9
#define UART_TX 8



#define PWM_PIN 28


#define MS_SINCE_BOOT to_ms_since_boot(get_absolute_time());

#define MEASURE_INTERVAL_MS 1500

#include <malloc.h>
#include <math.h>


static bool g_have_measurements;
static measurements_t g_measurements;

int measurement_responder(char *buf, uint len){
    if(g_have_measurements) {
        snprintf(buf, len, "{\"mc_1p0\":%f,"
                           "\"mc_2p5\":%f,"
                           "\"mc_4p0\":%f,"
                           "\"mc_10p0\":%f,"
                           "\"nc_0p5\":%f,"
                           "\"nc_1p0\":%f,"
                           "\"nc_2p5\":%f,"
                           "\"nc_4p0\":%f,"
                           "\"nc_10p0\":%f,"
                           "\"size_um\":%f}",

                 g_measurements.mc_1p0,
                 g_measurements.mc_2p5,
                 g_measurements.mc_4p0,
                 g_measurements.mc_10p0,
                 g_measurements.nc_0p5,
                 g_measurements.nc_1p0,
                 g_measurements.nc_2p5,
                 g_measurements.nc_4p0,
                 g_measurements.nc_10p0,
                 g_measurements.typical_particle_size
                 );
        return (int) strlen(buf);
    } else{
        strncpy(buf, "{}", 2);
        return 2;
    }
}


float compute_scaled_aqi(measurements_t *m) {
    return aqi_from_pm25_100(m->nc_2p5, m->nc_10p0);
}

static float g_last_servo_angle;

void update_servo_for_measurements(){
    if(!g_have_measurements) {
        return;
    }

    float aqi = compute_scaled_aqi(&g_measurements);

    printf("scaled aqi is %f\n", aqi);

    // we max out at about 450 (severe), and the bottom is 25 (very good and below).  So we take
    // (AQI - 50) / 400 to get our fractional angle, then scale that into -90-90 to get the
    // actual angle to set the servo to.
    float frac = fminf(1.0f, fmaxf(0, (aqi - 25.0f) / 400.0f));
    float angle = frac * 180.0f - 90.0f;
    if(fabsf(angle - g_last_servo_angle) > 0.2) {
        servo_set_angle(PWM_PIN, angle);
        g_last_servo_angle = angle;
    }
}

void chars_available_callback(void *p) {
    printf("chars avail!");
}

int main() {
    float servo_angle = -90.0f;

    stdio_init_all();
    stdio_set_chars_available_callback(chars_available_callback, NULL);


    sleep_ms(1000);

    servo_init(PWM_PIN);
    comms_log("PWM INIT OK\n");

    init_uart(UART, UART_RX, UART_TX);
    io_error_t io_err;

    //CHECK_IO(start_measurements(UART));
    comms_log("SENSOR INIT OK\n");

    if(net_init()){
        comms_log("NET INIT FAILED\n");
    }

    data_init();

    net_err_t err = net_connect_and_run(
            data_get_ssid(),
            data_get_password(),
            "aqi",
            80,
            measurement_responder);

    if(err != E_NET_OK){
        comms_log("error: can't get online (%d)", err);
    }else{
        comms_log("net on!");
    }

    uint last_measurement = 0;
    g_have_measurements = false;
    float servo_increment = 5.0f;
    printf("R\n");
    while(true){
        uint now = MS_SINCE_BOOT;
        if(now - last_measurement > MEASURE_INTERVAL_MS) {
      //      io_err = read_measurements(UART, &g_measurements);
            if(IS_OK(io_err)) {
                g_have_measurements = true;
                last_measurement = MS_SINCE_BOOT;
                //update_servo_for_measurements();
            }else{
                g_have_measurements = false;
            }

            servo_angle += servo_increment;
            if(servo_angle >= 90.0f) {
                servo_angle = 90.0f;
                servo_increment = -5.0f;
            }else if(servo_angle <= -90.0f) {
                servo_angle = -90.0f;
                servo_increment = 5.0f;
            }

            // servo_set_angle(PWM_PIN, servo_angle);
        }
        cyw43_arch_poll();
        comms_poll();
    }

}
