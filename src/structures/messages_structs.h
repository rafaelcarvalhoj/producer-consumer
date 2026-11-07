#ifndef __MSG_STRUCTS_H
#define __MSG_STRUCTS_H

#include<time.h>

typedef struct {
    int sender_pid;
    int sequentional_number;
    time_t send_time;
} Message_Struct;




#endif