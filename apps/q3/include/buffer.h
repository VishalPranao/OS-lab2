#ifndef Q3_BUFFER_H
#define Q3_BUFFER_H

#include "lab2-api.h"

#define BUFFER_SIZE 32
#define NUM_CHARACTERS 10
#define MAX_PAIRS 15

typedef struct {
  char data[BUFFER_SIZE];
  int head;
  int tail;
  lock_t lock;
  cond_t not_full;
  cond_t digit_ready[NUM_CHARACTERS];
  sem_t completed;
} shared_buffer;

#endif
