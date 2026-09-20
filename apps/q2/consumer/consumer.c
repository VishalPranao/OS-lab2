#include "usertraps.h"
#include "misc.h"
#include "buffer.h"

void main(int argc, char *argv[]) {
  uint32 h_mem;
  shared_buffer *buffer;
  char *text = "0123456789";
  int next = 0;
  int removed;

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
    /* Reserve an item before taking the lock. */
    if (sem_wait(buffer->s_fullslots) != SYNC_SUCCESS) {
      Printf("ERROR: full-slot wait failed\n");
      Exit();
    }
    if (lock_acquire(buffer->lock) != SYNC_SUCCESS) {
      Printf("ERROR: lock_acquire failed\n");
      Exit();
    }
    removed = 0;
    if (buffer->data[buffer->tail] == text[next]) {
      Printf("Consumer %d removed: %c\n", Getpid(), buffer->data[buffer->tail]);
      buffer->tail = (buffer->tail + 1) % BUFFER_SIZE;
      next++;
      removed = 1;
    }
    /* A different consumer may need the front digit. */
    if (lock_release(buffer->lock) != SYNC_SUCCESS) {
      Printf("ERROR: lock_release failed\n");
      Exit();
    }
    /* Return the item permit if no character was removed. */
    if (sem_signal(removed ? buffer->s_emptyslots : buffer->s_fullslots)
        != SYNC_SUCCESS) {
      Printf("ERROR: slot signal failed\n");
      Exit();
    }
  }

  /* No shared-memory accesses after reporting completion. */
  if (sem_signal(buffer->completed) != SYNC_SUCCESS) {
    Printf("ERROR: completion signal failed\n");
    Exit();
  }
}
