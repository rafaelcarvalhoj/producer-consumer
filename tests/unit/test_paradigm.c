#include "unions/paradigm.h"

#include <assert.h>
#include <string.h>

void test_paradigm(void)
{
    assert(strcmp(paradigm_to_str(SHARED_MEMORY), "SHARED_MEMORY") == 0);
    assert(strcmp(paradigm_to_str(MESSAGE_EXCHANGE), "MESSAGE_EXCHANGE") == 0);
    assert(strcmp(paradigm_to_str((ProducerConsumerParadigm)0), "UNKNOWN") == 0);
    assert(strcmp(paradigm_to_str((ProducerConsumerParadigm)3), "UNKNOWN") == 0);
}
