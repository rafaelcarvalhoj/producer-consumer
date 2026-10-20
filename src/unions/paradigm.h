#ifndef PRODUCER_CONSUMER_PARADIGM_H
#define PRODUCER_CONSUMER_PARADIGM_H
 
typedef enum {
    SHARED_MEMORY = 1,
    MESSAGE_EXCHANGE = 2
} ProducerConsumerParadigm;

const char *paradigm_to_str(ProducerConsumerParadigm p);

#endif
