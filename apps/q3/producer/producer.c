#include "usertraps.h"
#include "misc.h"
#include "buffer.h"

void main(int argc, char *argv[]) {
  uint32 h_mem;
  shared_buffer *buffer;
  char *text = "0123456789";
  int next = 0;

  if (argc != 2) {
    Printf("ERROR: expected a shared-memory handle\n");
    Exit();
  }
  h_mem = dstrtol(argv[1], NULL, 10);
  buffer = (shared_buffer *)shmat(h_mem);
  if (buffer == NULL) {
    Printf("ERROR: child shmat failed\n");
    Exit();
  }

  while (next < NUM_CHARACTERS) {
    if (lock_acquire(buffer->lock) != SYNC_SUCCESS) {
      Printf("ERROR: lock_acquire failed\n");
      Exit();
    }
    while ((buffer->head + 1) % BUFFER_SIZE == buffer->tail) {
      if (cond_wait(buffer->not_full) != SYNC_SUCCESS) {
        Printf("ERROR: not_full wait failed\n");
        Exit();
      }
    }
    buffer->data[buffer->head] = text[next];
    buffer->head = (buffer->head + 1) % BUFFER_SIZE;
    Printf("Producer %d inserted: %c\n", Getpid(), text[next]);
    next++;
    /* Signal may sleep: inspect the current front while owning the lock. */
    if (buffer->head != buffer->tail) {
      if (cond_signal(buffer->digit_ready[buffer->data[buffer->tail] - '0'])
          != SYNC_SUCCESS) {
        Printf("ERROR: digit signal failed\n");
        Exit();
      }
    }
    if (lock_release(buffer->lock) != SYNC_SUCCESS) {
      Printf("ERROR: lock_release failed\n");
      Exit();
    }
  }

  /* No shared-memory accesses after reporting completion. */
  if (sem_signal(buffer->completed) != SYNC_SUCCESS) {
    Printf("ERROR: completion signal failed\n");
    Exit();
  }
}
