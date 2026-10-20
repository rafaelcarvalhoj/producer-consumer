#include <stdio.h>

static void startup_log(void)
{
    puts("Producer-Consumer");
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    startup_log();
    return 0;
}
