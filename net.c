//
// Created by jon on 11/22/23.
//

#include "net.h"

#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/apps/mdns.h"
#include "lwip/apps/httpd.h"
#include "comms.h"

#define DBG printf
#define WRN printf

#define UNUSED __attribute__((unused))

#define  RESPONSE_BUFFER_SIZE 2048

typedef enum client_state {
    CS_NONE = 0,
    CS_WAIT_FOR_HTTP,
    CS_RESPONSE_SENT,
} client_state_t;

typedef struct client {
    struct tcp_pcb *pcb;
    client_state_t  state;
    uint16_t bytes_to_send;
    uint16_t bytes_sent;
    response_source_f response_source;
    char response_buffer[RESPONSE_BUFFER_SIZE];
} client_t;

typedef struct server {
    struct tcp_pcb* pcb;
    response_source_f response_source;
} server_t;

static void server_add_callbacks(server_t *server);
static void server_remove_callbacks(server_t *server);
static void client_add_callbacks(client_t *client);
static void client_remove_callbacks(client_t *client);



static err_t client_close(client_t *client) {
    DBG("server closing client\n");
    if(!client){
        WRN("client_close client = NULL\n");
        return ERR_VAL;
    }

    client_remove_callbacks(client);
    struct tcp_pcb* pcb = client->pcb;
    client->pcb = NULL;
    free(client);
    if(pcb){
        return tcp_close(pcb);
    }else{
        return ERR_OK;
    }
}

static client_t *client_open(struct tcp_pcb *pcb, response_source_f response_source){
    client_t *client = calloc(sizeof(client_t), 1);
    if(!client) {
        printf("OOM!\n");
        return NULL;
    }
    client->pcb = pcb;
    client->response_source = response_source;
    client->state = CS_WAIT_FOR_HTTP;
    return client;
}

err_t server_close(server_t *server) {
    if(!server) {
        WRN("server_close passed NULL\n");
        return ERR_VAL;
    }

    if(server->pcb) {
        server_remove_callbacks(server);
        struct tcp_pcb* pcb = server->pcb;
        server->pcb = NULL;
        return tcp_close(pcb);
    }
    return ERR_OK;
}

static bool find_http_end(struct pbuf* data) {
    struct pbuf *cur = data;
    do {
        if(pbuf_strstr(data, "\r\n\r\n") != 0xFFFF) {
            return true;
        }
        cur = cur->next;
    }while(cur && cur != data);

    return false;
}

static int write_http_response(struct tcp_pcb* pcb, const char *body, uint16_t len, err_t *err){
    char header_buf[256];
    sniprintf(header_buf, 256, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type:text/plain\r\n\r\n", len);
    uint16_t n = strlen(header_buf);
    *err = tcp_write(pcb, header_buf, n, 0);
    if(*err) {
        return -1;
    }
    *err = tcp_write(pcb, body, len, 0);
    if(*err) {
        return -1;
    }
    tcp_output(pcb);
    *err = ERR_OK;
    return n + len;
}

static err_t send_response(client_t *client) {
    uint16_t response_len = 0;
    if(client->response_source){
        response_len = client->response_source(client->response_buffer, RESPONSE_BUFFER_SIZE);
    }
    err_t e;
    int written = write_http_response(client->pcb, client->response_buffer, response_len, &e);
    if(written < 0) {
        WRN("failed to send data: %d", e);
        client_close(client);
        return e;
    }else{
        e = ERR_OK;
        client->bytes_to_send = response_len;
        client->state = CS_RESPONSE_SENT;
    }

    return e;
}

static err_t client_recv_cb(void *arg, UNUSED struct tcp_pcb *pcb, struct pbuf *data, err_t err) {
    if(err) {
        WRN("client receive error %d\n", err);
        return err;
    }


    client_t *client = (client_t *)arg;

    if(!data){
        WRN("client error: data is NULL\n");
        struct tcp_pcb *old_pcb = client->pcb;
        client->pcb = NULL;
        tcp_abort(old_pcb);
        client_close(client);
        return ERR_ABRT;
    }


    switch (client->state) {
        case CS_WAIT_FOR_HTTP:
        {
            if(find_http_end(data)){
                pbuf_free(data);
                return send_response(client);
            }
            return ERR_OK;
        }
        default:
            // weird to get data in these states but whatever
            WRN("received data in state %d\n", client->state);
            pbuf_free(data);
            return ERR_OK;
    }
}

static err_t client_sent_cb(void *arg, UNUSED struct tcp_pcb *pcb, uint16_t len) {
    client_t *client = (client_t *)arg;
    if(client->state == CS_RESPONSE_SENT) {
        client->bytes_sent += len;
        if(client->bytes_sent >= client->bytes_to_send) {
            return client_close(client);
        }
    }else{
        WRN("sent cb fired in state %d\n", client->state);
    }
    return ERR_OK;
}

static void client_err_cb(void *arg,  err_t err){
    WRN("client error: %d\n", err);
    if(err == ERR_ABRT){
        // we're already closing the client
        return;
    }

    client_t *client = (client_t *)arg;
    client_close(client);
}

static err_t server_accept_cb(void *arg, struct tcp_pcb *client_pcb, err_t err) {

    server_t* server = (server_t *)arg;

    if(err || !client_pcb) {
        WRN("accept err: %d client?: %d", err, client_pcb != NULL);
        server_close(server);
        return ERR_VAL;
    }

    client_t *client = client_open(client_pcb, server->response_source);

    if(!client) {
        tcp_close(client_pcb);
        return ERR_MEM;
    }

    client_add_callbacks(client);
    return E_NET_OK;
}


static void server_add_callbacks(server_t  *server) {
    struct tcp_pcb* pcb = server->pcb;
    tcp_arg(pcb, server);
    tcp_accept(pcb, server_accept_cb);
}

static void server_remove_callbacks(server_t *server){
    struct tcp_pcb* pcb = server->pcb;
    tcp_accept(pcb, NULL);
    tcp_arg(pcb, NULL);
}

static void client_add_callbacks(client_t *client){
    struct tcp_pcb* pcb = client->pcb;
    tcp_arg(pcb, client);
    tcp_recv(pcb, client_recv_cb);
    tcp_sent(pcb, client_sent_cb);
    tcp_err(pcb, client_err_cb);
}

static void client_remove_callbacks(client_t *client) {
    struct tcp_pcb* pcb = client->pcb;
    tcp_recv(pcb, NULL);
    tcp_sent(pcb, NULL);
    tcp_err(pcb, NULL);
    tcp_arg(pcb, NULL);
}

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
    uint16_t server_port,
    response_source_f response_source
) {
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
            if(server_port != 0){
                if(mdns_resp_add_service(netif_default, "web", "_http", DNSSD_PROTO_TCP,
                                         server_port, srv_txt_cb,
                                         NULL)){
                    comms_log("error adding service to netif");
                }else{
                    g_registered_mdns = true;
                }
            }
        }
    }
    int rc = E_NET_OK;
    if(server_port != 0 && response_source != NULL) {
        httpd_init();
        http_set_ssi_handler()
    }


    if(rc == E_NET_OK) {
        net_led_blink(true);
    }
    return rc;
}