//
// Created by jon on 12/23/23.
//
#include <hardware/sync.h>
#include <pico/time.h>
#include "hardware/flash.h"
#include "data.h"
#include "string.h"
#include "comms.h"
#include "util.h"

// need it to be waaay out here because cy43 uses a bunch of flash regions for its own stuff
#define FLASH_TARGET_OFFSET 0x180000
// we only need one to store a 33 byte ssid and 65 byte password + our magic
#define FLASH_MEM_PAGES 1
#define FLASH_BUF_SIZE (FLASH_PAGE_SIZE * FLASH_MEM_PAGES)
#define SSID_OFFSET 4
#define PASSWORD_OFFSET (SSID_OFFSET + SSID_MAX_LEN + 1)

const char magic[4] = {'P', 'I', 'C', 'A'};

// scratch area with data to save
static uint8_t buffer[FLASH_BUF_SIZE];

// pointer (read only) to the actual flash memory
static const uint8_t *flash_contents = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET);

// return true if `buffer` looks right
static bool is_buffer_valid() {
    return strncmp(magic, (const char*) buffer, 4) == 0;
}

void data_init() {
    memcpy(buffer, flash_contents, FLASH_BUF_SIZE);
    // if the buffer looks uninitialized, write our magic and zeros
    if( !is_buffer_valid() ){
        memset(buffer, 0, FLASH_BUF_SIZE);
        memcpy(buffer, magic, 4);
    }
}

void data_set_password(const char *password) {
    strncpy((char *) (buffer + PASSWORD_OFFSET), password, PASS_MAX_LEN + 1);
}

void data_set_ssid(const char *ssid) {
    strncpy((char *)(buffer + SSID_OFFSET), ssid, SSID_MAX_LEN + 1);
}

// returns a pointer to the password stored in our flash area.
const char* data_get_password() {
    if(!is_buffer_valid()){
        comms_log("warn: data_init not called");
        data_init();
    }

    return (const char *) ( buffer + PASSWORD_OFFSET );
}

const char *data_get_ssid(){
    if(!is_buffer_valid()){
        comms_log("warn: data_init not called");
        data_init();
    }
    return (const char*) (buffer + SSID_OFFSET);
}

int data_save_settings() {
    if(!is_buffer_valid()) {
        comms_log("error: tried to save settings without calling data_init!");
        return -1;
    }
    // interrupts need to be off while we write to the flash
    uint32_t mask = save_and_disable_interrupts();

    // for some reason you need to erase the whole sector before programming it
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);

    flash_range_program(FLASH_TARGET_OFFSET, buffer, FLASH_PAGE_SIZE);

    restore_interrupts(mask);

    return 0;
}