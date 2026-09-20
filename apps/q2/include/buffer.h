#ifndef Q2_BUFFER_H
#define Q2_BUFFER_H

#include "lab2-api.h"

#define BUFFER_SIZE 32
#define NUM_CHARACTERS 10
#define MAX_PAIRS 15

typedef struct {
  char data[BUFFER_SIZE];
  int head;
  int tail;
  lock_t lock;
  sem_t s_fullslots;
  sem_t s_emptyslots;
  sem_t completed;
} shared_buffer;

#endif
