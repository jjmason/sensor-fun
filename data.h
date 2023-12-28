//
// Created by jon on 12/23/23.
//
#include "pico.h"

#ifndef PICA_DATA_H
#define PICA_DATA_H

#define SSID_MAX_LEN 32
#define PASS_MAX_LEN 64

// load and initialize our flash data area.
void data_init();

void data_set_password(const char* pass);

// save the wifi ssid
void data_set_ssid(const char* ssid);

const char* data_get_password();

const char *data_get_ssid();

int data_save_settings();

#endif //PICA_DATA_H
