#ifndef __PRODUTOR_H
#define __PRODUTOR_H

#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdlib.h>
#include "structures/messages_structs.h"
#include"unions/paradigm.h"

void exeuta_produz(ProducerConsumerParadigm p);
int create_message_exchange(key_t key);
void send_message(int msqid,int sender_pid,int number_of_msgs,int ack);
void read_message(int msqid,int read_time, int ack);
#endif