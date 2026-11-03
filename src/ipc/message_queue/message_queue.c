#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
int main()
{
    printf("OLHA EU ENTREI AQUI RAPAZ");
    key_t key = ftok("progfile", 65);
    return 0;
}   