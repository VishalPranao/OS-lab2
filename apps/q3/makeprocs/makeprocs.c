#include "usertraps.h"
#include "misc.h"
#include "buffer.h"

void main(int argc, char *argv[]) {
  int pairs;
  int i;
  uint32 h_mem;
  char h_mem_str[12];
  shared_buffer *buffer;

  if (argc != 2) {
    Printf("Usage: makeprocs.dlx.obj <number of producer/consumer pairs>\n");
    Exit();
  }
  /* Check digits before conversion; the OS has only 32 process slots. */
  pairs = 0;
  for (i = 0; argv[1][i] != '\0'; i++) {
    if (argv[1][i] < '0' || argv[1][i] > '9') {
      Printf("ERROR: pair count must be an integer from 1 to %d\n", MAX_PAIRS);
      Exit();
    }
    pairs = pairs * 10 + argv[1][i] - '0';
    if (pairs > MAX_PAIRS) {
      Printf("ERROR: at most %d pairs are supported\n", MAX_PAIRS);
      Exit();
    }
  }
  if (pairs < 1) {
    Printf("ERROR: at least one pair is required\n");
    Exit();
  }

  h_mem = shmget();
  if (h_mem == 0) {
    Printf("ERROR: shmget failed\n");
    Exit();
  }
  buffer = (shared_buffer *)shmat(h_mem);
  if (buffer == NULL) {
    Printf("ERROR: parent shmat failed\n");
    Exit();
  }
  buffer->head = 0;
  buffer->tail = 0;
  buffer->lock = lock_create();
  if (buffer->lock == SYNC_FAIL) {
    Printf("ERROR: lock_create failed\n");
    Exit();
  }
  buffer->not_full = cond_create(buffer->lock);
  if (buffer->not_full == SYNC_FAIL) {
    Printf("ERROR: not_full creation failed\n");
    Exit();
  }
  for (i = 0; i < NUM_CHARACTERS; i++) {
    buffer->digit_ready[i] = cond_create(buffer->lock);
    if (buffer->digit_ready[i] == SYNC_FAIL) {
      Printf("ERROR: digit condition creation failed\n");
      Exit();
    }
  }
  /* As in the example: one parent wait needs all 2*pairs signals. */
  buffer->completed = sem_create(1 - 2 * pairs);
  if (buffer->completed == SYNC_FAIL) {
    Printf("ERROR: sem_create failed\n");
    Exit();
  }

  ditoa(h_mem, h_mem_str);
  for (i = 0; i < pairs; i++) {
    process_create("producer.dlx.obj", h_mem_str, NULL);
    process_create("consumer.dlx.obj", h_mem_str, NULL);
  }
  if (sem_wait(buffer->completed) != SYNC_SUCCESS) {
    Printf("ERROR: parent completion wait failed\n");
    Exit();
  }
}
