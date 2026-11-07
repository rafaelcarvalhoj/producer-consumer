#include "produtor.h"

void exeuta_produz(ProducerConsumerParadigm p)
{
    switch (p)
    {
    case SHARED_MEMORY:
        /* code */
        break;
    case MESSAGE_EXCHANGE:
        /* code */
        break;
    default:
        break;
    }
}

int create_message_exchange(key_t key)
{
    int msgID;
    if ((msgID = msgget(key, IPC_CREAT | 0x1FF)) < 0)
    {
        printf("deu um erro aqui paizao na hora de criar a fila %d", msgID);
        exit(-4);
    }
    return msgID;
}

void send_message(int msqid, int sender_pid, int number_of_msgs, int ack)
{
    Message_Struct msg;
    msg.sender_pid = sender_pid;
    time_t send_time;

    msg.send_time = send_time;
    for (int i = 0; i < number_of_msgs; i++)
    {
        printf("mandei o num %d do PID:%d\n",i,sender_pid);
        time(&send_time);
        msg.send_time = send_time;
        msg.sequentional_number = i;
        int wait = ack == 1 ? 0 : IPC_NOWAIT;
        int quant_enviada = msgsnd(msqid, &msg, sizeof(msg), wait);
        printf("quantidade enviada: %d\n",quant_enviada);
    }
}

void read_message(int msqid, int read_time, int ack)
{
    Message_Struct msg;
    for (int i = 0; i < read_time; i++)
    {
        int wait = ack == 1 ? 0 : IPC_NOWAIT;
        int read_return;
        read_return = msgrcv(msqid, &msg, sizeof(msg), 0, wait);
        if (read_return < 0)
        {
            printf("dei algum erro aqui na hora da leitura amigão %d", read_return);
        }
        else
        {
            printf("\t esse é a msg lida:\n");
            printf("\t pid do processo que criou a fila: %d\n", msg.sender_pid);
            printf("\t sequencia da msg: %d\n", msg.sequentional_number);
            time_t tempo_recebido;
            printf("\t tempo que foi enviado: %s\n", ctime(&msg.send_time));
            printf("\t tempo que esse consumidor leu a msg: %s\n", ctime(&tempo_recebido));
        }
    }
}
