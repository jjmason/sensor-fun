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
static response_data_t  g_response_data;


float compute_scaled_aqi(measurements_t *m) {
    return aqi_from_pm25_100(m->nc_2p5, m->nc_10p0);
}

response_data_t *measurement_responder(){
    g_response_data.measurements = g_have_measurements ? &g_measurements : NULL;

    if(g_have_measurements) {
        g_response_data.aqi = compute_scaled_aqi(&g_measurements);
    }else{
        g_response_data.alarms = "No measurements available!";
    }
    return &g_response_data;
}



static float g_last_servo_angle;

void update_servo_for_measurements(){
    if(!g_have_measurements) {
        return;
    }

    float aqi = compute_scaled_aqi(&g_measurements);

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


int main() {
    float servo_angle = -90.0f;

    stdio_init_all();

    sleep_ms(250);

    servo_init(PWM_PIN);
    comms_log("PWM INIT OK\n");

    init_uart(UART, UART_RX, UART_TX);
    io_error_t io_err;

    //CHECK_IO(start_measurements(UART));
    comms_log("SENSOR INIT OK\n");

    if(net_init()){
        comms_log("NET INIT FAILED\n");
    }else{
        comms_log("NET INIT OK");
    }

    data_init();

    net_err_t err = net_connect_and_run(
            data_get_ssid(),
            data_get_password(),
            "aqi",
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
