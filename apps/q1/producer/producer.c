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
    if ((buffer->head + 1) % BUFFER_SIZE != buffer->tail) {
      buffer->data[buffer->head] = text[next];
      buffer->head = (buffer->head + 1) % BUFFER_SIZE;
      Printf("Producer %d inserted: %c\n", Getpid(), text[next]);
      next++;
    }
    /* Release even when unable to proceed, so other children can run. */
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
