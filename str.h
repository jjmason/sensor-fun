//
// Created by jon on 12/27/23.
//

#ifndef PICA_STR_H
#define PICA_STR_H
#include <pico.h>
#include <string.h>

typedef const char* (*lookup_fn)(const char *, size_t);

size_t strpos(const char *str, size_t start, char c) {
    char *found = strchr(str+start, c);
    if(found == NULL) {
        return -1;
    }
    return found - str;
}

bool format_template(const char *str, lookup_fn lookup, char *dest, size_t dest_size) {
    size_t srcpos = 0, destpos = 0;
    size_t srclen = strlen(str);
    while(srcpos < srclen && destpos < dest_size) {
        size_t open_bracket = strpos(str, srcpos, '{');
        if(open_bracket < 0) {
            // no more brackets! copy the rest of str to dest and call it good
            if(srclen - srcpos >= dest_size - destpos) {
                return false;
            }
            strncpy(dest, str + srcpos, srclen - srcpos);
            return true;
        }
        size_t end_bracket = strpos(str, open_bracket + 1, '}');
        if(end_bracket < 0) {

        }

    }
}


#endif //PICA_STR_H
