//
// Created by jon on 11/22/23.
//

#include "net.h"

#include "pico/cyw43_arch.h"
#include "lwip/apps/mdns.h"
#include "lwip/apps/httpd.h"
#include "comms.h"

#define UNUSED __attribute__((unused))

response_source_f g_response_source = NULL;

const char *ssi_tags[] = {
    "aqi",
    "mc_1p0",
    "mc_2p5",
    "mc_4p0",
    "mc_10p0",
    "nc_0p5",
    "nc_1p0",
    "nc_2p5",
    "nc_4p0",
    "nc_10p0",
    "size_um",
    "alarms"
};

#define REP(f, ...) inserted_len = snprintf(p_insert, insert_len, (f), __VA_ARGS__)
#define REPS(...) REP("%s", __VA_ARGS__)
#define REPM(m) REP("%0.2f", data->measurements->m); break;


static u16_t ssi_handler(int index, char *p_insert, int insert_len) {
    comms_log("ssi_handler(%d)", index);
    int inserted_len = 0;
    if(g_response_source == NULL){
        if(index == 11 /* alarms */) {
            REPS("BUG: no response source provided");
        }else{
            REPS("n/a");
        }
        goto done;
    }

    response_data_t *data = g_response_source();
    if(data->measurements == NULL) {
        switch (index) {
            case 11: // alarms
                REPS(data->alarms ? data->alarms : "OK");
                break;
            case 0: // AQI
                REP("%0.1f", data->aqi);
                break;
            default:
                REPS("n/a");
        }
        goto done;
    }

    switch (index) {
        case 0: // aqi
            REP("%0.1f", data->aqi);
            break;
        case 1:  REPM(mc_1p0)
        case 2:  REPM(mc_2p5)
        case 3:  REPM(mc_4p0)
        case 4:  REPM(mc_10p0)
        case 5:  REPM(nc_0p5)
        case 6:  REPM(nc_1p0)
        case 7:  REPM(nc_2p5)
        case 8:  REPM(nc_4p0)
        case 9:  REPM(nc_10p0)
        case 10: REPM(typical_particle_size); break;
        case 11: REPS(data->alarms ? data->alarms : "OK"); break;
        default: break;
    }


    done:
    LWIP_ASSERT("sane length", inserted_len <= 0xFFFF);
    return (u16_t)inserted_len;
}

#undef REP
#undef REPS
#undef REPM

static bool g_registered_mdns = false;

static void mdns_name_result_cb(struct netif *netif, u8_t result, UNUSED s8_t slot) {
    comms_log("mdns status[netif %d]: %d", netif->num, result);
}

int net_init(){
    int rc = cyw43_arch_init();
    if(rc) {
        return rc;
    }
    cyw43_arch_enable_sta_mode();
    mdns_resp_register_name_result_cb(mdns_name_result_cb);
    mdns_resp_init();

    return 0;
}

int net_connect_to_wifi(const char *ssid, const char *passwd) {
    return cyw43_arch_wifi_connect_timeout_ms(ssid, passwd, CYW43_AUTH_WPA2_AES_PSK, 30000);
}

void net_led_blink(bool on){
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
}


static void srv_txt_cb(struct mdns_service *service, __attribute__((unused)) void *txt_userdata) {
    err_t  res;
    res = mdns_resp_add_service_txtitem(service, "path=/", 6);
    if(res != ERR_OK) {
        comms_log("error adding txt service to mdns: %d", res);
    }
}


net_err_t net_connect_and_run(
    const char *wifi_ssid,
    const char *wifi_pass,
    const char *mdns_hostname,
    response_source_f response_source
) {
    g_response_source = response_source;
    net_led_blink(false);

    if(net_connect_to_wifi(wifi_ssid, wifi_pass)) {
        return E_NET_NO_WIFI;
    }

    if(g_registered_mdns) {
        g_registered_mdns = false;
        if(mdns_resp_remove_netif(netif_default)){
            comms_log("warn: error removing MDNS?");
        }
    }

    if(mdns_hostname != NULL) {
        if(mdns_resp_add_netif(netif_default, mdns_hostname)){
            comms_log("couldn't set up MDNS on %d", netif_default->num);
        }else{
            if(response_source != NULL){
                if(mdns_resp_add_service(netif_default, "web", "_http", DNSSD_PROTO_TCP,
                                         80, srv_txt_cb,
                                         NULL)){
                    comms_log("error adding service to netif");
                }else{
                    g_registered_mdns = true;
                }
            }
        }
    }
    int rc = E_NET_OK;
    if(response_source != NULL) {
        httpd_init();
        http_set_ssi_handler(ssi_handler, ssi_tags, LWIP_ARRAYSIZE(ssi_tags));
    }


    if(rc == E_NET_OK) {
        net_led_blink(true);
    }
    return rc;
}