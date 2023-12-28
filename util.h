//
// Created by jon on 12/23/23.
//

#ifndef PICA_UTIL_H
#define PICA_UTIL_H
#include "pico.h"
#include "stdio.h"

void print_buf(uint8_t *buf, size_t len) {
    for (int i = 0; i < len; ++i) {
        printf("%02X", buf[i]);
        if(i % 16 == 15) {
            printf("\n");
        }
        else{
            printf(" ");
        }
    }
}

#endif //PICA_UTIL_H
