//
// Created by jon on 11/22/23.
//

#ifndef PICA_NET_H
#define PICA_NET_H
#include "pico.h"

// An error type
typedef enum net_err {
    E_NET_OK = 0x00,
    E_NET_ALREADY_STARTED = 0x01,
    E_NET_PCB_INIT_FAIL = 0x02,
    E_NET_ADDR_IN_USE = 0x03,
    E_NET_BIND_OTHER = 0x04,
    E_NET_LISTEN_FAIL = 0x05,
    E_NET_NO_MEM = 0x06,
    E_NET_NO_WIFI = 0x07,
    E_NET_INTERNAL = 0x08
} net_err_t;

// passed to net_start_server, serves a response to a client by writing it
// into the given buffer.
typedef int (*response_source_f)(char *buffer, size_t buffer_size);


int net_init();
void net_led_blink(bool on);
int net_connect_to_wifi(const char *ssid, const char *passwd);
net_err_t net_start_server(uint16_t port, response_source_f responder);
net_err_t net_connect_and_run(   const char *wifi_ssid,
                                  const char *wifi_pass,
                                  const char *mdns_hostname,
                                  uint16_t server_port,
                                  response_source_f response_source);

#endif //PICA_NET_H