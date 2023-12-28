//
// Created by jon on 12/22/23.
//

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <pico/time.h>
#include "pico/stdio.h"
#include "comms.h"
#include "data.h"

#define LOG_BUF_SIZE 512
#define MAX_CMD_LEN 512

static char cmd_buff[MAX_CMD_LEN];
static size_t buf_pos;

#define CMD_INFO 'I'
#define CMD_SET_SSID 'S'
#define CMD_SET_PASS 'P'
#define CMD_DATA 'D'
#define CMD_WRITE 'W'

static void handle_set_ssid() {
    if(buf_pos - 1 > SSID_MAX_LEN) {
        comms_log("error: ssid too long");
        return;
    }
    cmd_buff[buf_pos + 1] = 0;
    data_set_ssid(cmd_buff + 1);
    printf("S\n");
}

static void handle_set_pass() {
    if(buf_pos - 1 > PASS_MAX_LEN) {
        comms_log("error: pass too long");
        return;
    }
    cmd_buff[buf_pos + 1] = 0;
    data_set_password(cmd_buff + 1);
    printf("P\n");
}

static void handle_info() {
    printf("I SSID='%s' PASS='%s'\n",
           data_get_ssid(),
           data_get_password());
}

static void handle_write() {
    if(data_save_settings() != 0) {
        printf("E\n");
    }else{
        printf("W\n");
    }
}

static void process_command_buf() {
    switch (cmd_buff[0]) {
        case CMD_INFO:
            handle_info();
            break;
        case CMD_SET_SSID:
            handle_set_ssid();
            break;
        case CMD_SET_PASS:
            handle_set_pass();
            break;
        case CMD_WRITE:
            handle_write();
            break;
        case CMD_DATA:
            comms_log("data\n");
            break;
        default:
            comms_log("error\n");
    }
    buf_pos = 0;
}


void comms_poll() {
    int c;
    while((c=getchar_timeout_us(500)) != PICO_ERROR_TIMEOUT) {
        if(c == '\n' || c == '\r') {
            process_command_buf();
        }else{
            cmd_buff[buf_pos] = (char) c;
            buf_pos ++;
            if(buf_pos >= MAX_CMD_LEN) {
                buf_pos = 0;
                comms_log("max command len (%d) exceeded\n", MAX_CMD_LEN);
            }
        }
    }
}


void comms_log(const char *fmt, ...){
    char buf[LOG_BUF_SIZE];
    va_list argp;
    va_start(argp, fmt);
    int size = vsnprintf(buf, LOG_BUF_SIZE, fmt, argp);
    for(int i=0; i < size; i ++){
        if(buf[i] == '\n') {
            buf[i] = ' ';
        }
    }

    printf("L %s\n", buf);
}