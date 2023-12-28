//
// Created by jon on 12/22/23.
//

#ifndef PICA_COMMS_H
#define PICA_COMMS_H

// checks for a command and executes it
void comms_poll();

// formats a log to the serial out
void comms_log(const char *fmt, ...);

#endif //PICA_COMMS_H
