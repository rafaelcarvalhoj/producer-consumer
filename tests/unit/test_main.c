#include <stdio.h>

void test_str_to_int(void);
void test_startup_args(void);
void test_paradigm(void);

int main(void)
{
    test_str_to_int();
    test_startup_args();
    test_paradigm();
    puts("Unit tests passed.");
    return 0;
}
