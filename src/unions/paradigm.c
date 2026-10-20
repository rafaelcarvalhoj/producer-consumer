#include "unions/paradigm.h"


const char *paradigm_to_str(ProducerConsumerParadigm p)
{
    switch (p) {
        case SHARED_MEMORY:
            return "SHARED_MEMORY";
        case MESSAGE_EXCHANGE:
            return "MESSAGE_EXCHANGE";
        default:
            return "UNKNOWN";
    }
}
