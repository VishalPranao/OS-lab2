//
//	synch.c
//
//	Routines for synchronization
//
//

#include "ostraps.h"
#include "dlxos.h"
#include "process.h"
#include "synch.h"
#include "queue.h"

static Sem sems[MAX_SEMS]; 	// All semaphores in the system
static Cond conds[MAX_CONDS];
static Lock locks[MAX_LOCKS];   // All locks in the system

extern struct PCB *currentPCB; 
//----------------------------------------------------------------------
//	SynchModuleInit
//
//	Initializes the synchronization primitives: the semaphores
//----------------------------------------------------------------------
int SynchModuleInit() {
  int i; // Loop Index variable
  dbprintf ('p', "SynchModuleInit: Entering SynchModuleInit\n");
  for(i=0; i<MAX_SEMS; i++) {
    sems[i].inuse = 0;
  }
  for(i=0; i<MAX_LOCKS; i++) {
    locks[i].inuse = 0;
  }
  for(i=0; i<MAX_CONDS; i++) {
    conds[i].inuse = 0;
  }
  dbprintf ('p', "SynchModuleInit: Leaving SynchModuleInit\n");
  return SYNC_SUCCESS;
}

//---------------------------------------------------------------------
//
//	SemInit
//
//	Initialize a semaphore to a particular value.  This just means
//	initting the process queue and setting the counter.
//
//----------------------------------------------------------------------
int SemInit (Sem *sem, int count) {
  if (!sem) return SYNC_FAIL;
  if (AQueueInit (&sem->waiting) != QUEUE_SUCCESS) {
    printf("FATAL ERROR: could not initialize semaphore waiting queue in SemInit!\n");
    exitsim();
  }
  sem->count = count;
  return SYNC_SUCCESS;
}

//----------------------------------------------------------------------
// 	SemCreate
//
//	Grabs a Semaphore, initializes it and returns a handle to this
//	semaphore. All subsequent accesses to this semaphore should be made
//	through this handle.  Returns SYNC_FAIL on failure.
//----------------------------------------------------------------------
sem_t SemCreate(int count) {
  sem_t sem;
  uint32 intrval;

  // grabbing a semaphore should be an atomic operation
  intrval = DisableIntrs();
  for(sem=0; sem<MAX_SEMS; sem++) {
    if(sems[sem].inuse==0) {
      sems[sem].inuse = 1;
      break;
    }
  }
  RestoreIntrs(intrval);
  if(sem==MAX_SEMS) return SYNC_FAIL;

  if (SemInit(&sems[sem], count) != SYNC_SUCCESS) return SYNC_FAIL;
  return sem;
}


//----------------------------------------------------------------------
//
//	SemWait
//
//	Wait on a semaphore.  As described in Section 6.4 of _OSC_,
//	we decrement the counter and suspend the process if the
//	semaphore's value is less than 0.  To ensure atomicity,
//	interrupts are disabled for the entire operation, but must be
//      turned on before going to sleep.
//
//----------------------------------------------------------------------
int SemWait (Sem *sem) {
  Link	*l;
  int		intrval;
    
  if (!sem) return SYNC_FAIL;

  intrval = DisableIntrs ();
  dbprintf ('I', "SemWait: Old interrupt value was 0x%x.\n", intrval);
  dbprintf ('s', "SemWait: Proc %d waiting on sem %d, count=%d.\n", GetCurrentPid(), (int)(sem-sems), sem->count);
  if (sem->count <= 0) {
    dbprintf('s', "SemWait: putting process %d to sleep\n", GetCurrentPid());
    if ((l = AQueueAllocLink ((void *)currentPCB)) == NULL) {
      printf("FATAL ERROR: could not allocate link for semaphore queue in SemWait!\n");
      exitsim();
    }
    if (AQueueInsertLast (&sem->waiting, l) != QUEUE_SUCCESS) {
      printf("FATAL ERROR: could not insert new link into semaphore waiting queue in SemWait!\n");
      exitsim();
    }
    ProcessSleep();
    // Don't decrement couter here because that's handled in SemSignal for us
  } else {
    sem->count--; // Decrement internal counter
    dbprintf('s', "SemWait: Proc %d granted permission to continue by sem %d\n", GetCurrentPid(), (int)(sem-sems));
  }
  RestoreIntrs (intrval);
  return SYNC_SUCCESS;
}

int SemHandleWait(sem_t sem) {
  if (sem < 0) return SYNC_FAIL;
  if (sem >= MAX_SEMS) return SYNC_FAIL;
  if (!sems[sem].inuse)    return SYNC_FAIL;
  return SemWait(&sems[sem]);
}

//----------------------------------------------------------------------
//
//	SemSignal
//
//	Signal on a semaphore.  Again, details are in Section 6.4 of
//	_OSC_.
//
//----------------------------------------------------------------------
int SemSignal (Sem *sem) {
  Link *l;
  int	intrs;
  PCB *pcb;

  if (!sem) return SYNC_FAIL;

  intrs = DisableIntrs ();
  dbprintf ('s', "SemSignal: Process %d Signalling on sem %d, count=%d.\n", GetCurrentPid(), (int)(sem-sems), sem->count);
  // Increment internal counter before checking value
  sem->count++;
  if (sem->count > 0) { // check if there is a process to wake up
    if (!AQueueEmpty(&sem->waiting)) { // there is a process to wake up
      l = AQueueFirst(&sem->waiting);
      pcb = (PCB *)AQueueObject(l);
      if (AQueueRemove(&l) != QUEUE_SUCCESS) { 
        printf("FATAL ERROR: could not remove link from semaphore queue in SemSignal!\n");
        exitsim();
      }
      dbprintf ('s', "SemSignal: Waking up PID %d.\n", (int)(GetPidFromAddress(pcb)));
      ProcessWakeup (pcb);
      // Decrement counter on behalf of woken up PCB
      sem->count--;
    }
  }
  RestoreIntrs (intrs);
  return SYNC_SUCCESS;
}

int SemHandleSignal(sem_t sem) {
  if (sem < 0) return SYNC_FAIL;
  if (sem >= MAX_SEMS) return SYNC_FAIL;
  if (!sems[sem].inuse)    return SYNC_FAIL;
  return SemSignal(&sems[sem]);
}

//-----------------------------------------------------------------------
//	LockCreate
//
//	LockCreate grabs a lock from the systeme-wide pool of locks and 
//	initializes it.
//	It also sets the inuse flag of the lock to indicate that the lock is
//	being used by a process. It returns a unique id for the lock. All the
//	references to the lock should be made through the returned id. The
//	process of grabbing the lock should be atomic.
//
//	If a new lock cannot be created, your implementation should return
//	INVALID_LOCK (see synch.h).
//-----------------------------------------------------------------------
lock_t LockCreate() {
  lock_t l;
  uint32 intrval;

  // grabbing a lock should be an atomic operation
  intrval = DisableIntrs();
  for(l=0; l<MAX_LOCKS; l++) {
    if(locks[l].inuse==0) {
      locks[l].inuse = 1;
      break;
    }
  }
  RestoreIntrs(intrval);
  if(l==MAX_LOCKS) return SYNC_FAIL;

  if (LockInit(&locks[l]) != SYNC_SUCCESS) return SYNC_FAIL;
  return l;
}

int LockInit(Lock *l) {
  if (!l) return SYNC_FAIL;
  if (AQueueInit (&l->waiting) != QUEUE_SUCCESS) {
    printf("FATAL ERROR: could not initialize lock waiting queue in LockInit!\n");
    exitsim();
  }
  l->pid = -1;
  return SYNC_SUCCESS;
}

//---------------------------------------------------------------------------
//	LockHandleAcquire
//
//	This routine acquires a lock given its handle. The handle must be a 
//	valid handle for this routine to succeed. In that case this routine 
//	returns SYNC_FAIL. Otherwise the routine returns SYNC_SUCCESS.
//
//---------------------------------------------------------------------------
int LockAcquire(Lock *k) {
  Link	*l;
  int		intrval;
    
  if (!k) return SYNC_FAIL;

  // Locks are atomic
  intrval = DisableIntrs ();
  dbprintf ('I', "LockAcquire: Old interrupt value was 0x%x.\n", intrval);

  // Check to see if the current process owns the lock
  if (k->pid == GetCurrentPid()) {
    dbprintf('s', "LockAcquire: Proc %d already owns lock %d\n", GetCurrentPid(), (int)(k-locks));
    RestoreIntrs(intrval);
    return SYNC_SUCCESS;
  }

  dbprintf ('s', "LockAcquire: Proc %d asking for lock %d.\n", GetCurrentPid(), (int)(k-locks));
  if (k->pid >= 0) { // Lock is already in use by another process
    dbprintf('s', "LockAcquire: putting process %d to sleep\n", GetCurrentPid());
    if ((l = AQueueAllocLink ((void *)currentPCB)) == NULL) {
      printf("FATAL ERROR: could not allocate link for lock queue in LockAcquire!\n");
      exitsim();
    }
    if (AQueueInsertLast (&k->waiting, l) != QUEUE_SUCCESS) {
      printf("FATAL ERROR: could not insert new link into lock waiting queue in LockAcquire!\n");
      exitsim();
    }
    ProcessSleep();
  } else {
    dbprintf('s', "LockAcquire: lock is available, assigning to proc %d\n", GetCurrentPid());
    k->pid = GetCurrentPid();
  }
  RestoreIntrs(intrval);
  return SYNC_SUCCESS;
}

int LockHandleAcquire(lock_t lock) {
  if (lock < 0) return SYNC_FAIL;
  if (lock >= MAX_LOCKS) return SYNC_FAIL;
  if (!locks[lock].inuse)    return SYNC_FAIL;
  return LockAcquire(&locks[lock]);
}

//---------------------------------------------------------------------------
//	LockHandleRelease
//
//	This procedure releases the unique lock described by the handle. It
//	first checks whether the lock is a valid lock. If not, it returns SYNC_FAIL.
//	If the lock is a valid lock, it should check whether the calling
//	process actually holds the lock. If not it returns SYNC_FAIL. Otherwise it
//	releases the lock, and returns SYNC_SUCCESS.
//---------------------------------------------------------------------------
int LockRelease(Lock *k) {
  Link *l;
  int	intrs;
  PCB *pcb;

  if (!k) return SYNC_FAIL;

  intrs = DisableIntrs ();
  dbprintf ('s', "LockRelease: Proc %d releasing lock %d.\n", GetCurrentPid(), (int)(k-locks));

  if (k->pid != GetCurrentPid()) {
    dbprintf('s', "LockRelease: Proc %d does not own lock %d.\n", GetCurrentPid(), (int)(k-locks));
    RestoreIntrs(intrs);
    return SYNC_FAIL;
  }
  k->pid = -1;
  if (!AQueueEmpty(&k->waiting)) { // there is a process to wake up
    l = AQueueFirst(&k->waiting);
    pcb = (PCB *)AQueueObject(l);
    if (AQueueRemove(&l) != QUEUE_SUCCESS) { 
      printf("FATAL ERROR: could not remove link from lock queue in LockRelease!\n");
      exitsim();
    }
    dbprintf ('s', "LockRelease: Waking up PID %d, assigning lock.\n", (int)(GetPidFromAddress(pcb)));
    k->pid = GetPidFromAddress(pcb);
    ProcessWakeup (pcb);
  }
  RestoreIntrs (intrs);
  return SYNC_SUCCESS;
}

int LockHandleRelease(lock_t lock) {
  if (lock < 0) return SYNC_FAIL;
  if (lock >= MAX_LOCKS) return SYNC_FAIL;
  if (!locks[lock].inuse)    return SYNC_FAIL;
  return LockRelease(&locks[lock]);
}

//---------------------------------------------------------------------------
//	LockTransfer
//
//	This procedure transfers the lock to the process described by the PCB. It
//	first checks whether the lock is a valid lock. If not, it returns SYNC_FAIL.
//	If the lock is a valid lock, it should check whether the calling
//	process actually holds the lock. If not it returns SYNC_FAIL. Otherwise it
//	releases the lock, and returns SYNC_SUCCESS. If the target process is
//	waiting for the lock, it is woken up.
//---------------------------------------------------------------------------
int LockTransfer(Lock *k, PCB *pcb) {
  Link *l;
  int	intrs;

  if (!k) return SYNC_FAIL;

  intrs = DisableIntrs ();
  dbprintf ('s', "LockTransfer: Proc %d transferring lock %d.\n", GetCurrentPid(), (int)(k-locks));

  if (k->pid != GetCurrentPid()) {
    dbprintf('s', "LockTransfer: Proc %d does not own lock %d.\n", GetCurrentPid(), (int)(k-locks));
    RestoreIntrs(intrs);
    return SYNC_FAIL;
  }

  k->pid = GetPidFromAddress(pcb);
  dbprintf ('s', "LockTransfer: Assigning lock to %d.\n", (int)(GetPidFromAddress(pcb)));
  l = AQueueFirst(&k->waiting);
  while (l) {
    pcb = (PCB *)AQueueObject(l);
    if (GetPidFromAddress(pcb) == k->pid) {
      if (AQueueRemove(&l) != QUEUE_SUCCESS) { 
        printf("FATAL ERROR: could not remove link from lock queue in LockTransfer!\n");
        exitsim();
      }
      dbprintf ('s', "LockTransfer: Waking up PID %d.\n", (int)(GetPidFromAddress(pcb)));
      ProcessWakeup (pcb);
      break;
    }
    l = AQueueNext(l);
  }
  RestoreIntrs (intrs);
  return SYNC_SUCCESS;
}

//--------------------------------------------------------------------------
//	CondCreate
//
//	This function grabs a condition variable from the system-wide pool of
//	condition variables and associates the specified lock with
//	it. It should also initialize all the fields that need to initialized.
//	The lock being associated should be a valid lock, which means that
//	it should have been obtained via previous call to LockCreate(). 
//	
//	If for some reason a condition variable cannot be created (no more
//	condition variables left, or the specified lock is not a valid lock),
//	this function should return INVALID_COND (see synch.h). Otherwise it
//	should return handle of the condition variable.
//--------------------------------------------------------------------------
cond_t CondCreate(lock_t lock) {
  cond_t c;
  uint32 intrs;

  if (lock < 0 || lock >= MAX_LOCKS || !locks[lock].inuse)
    return SYNC_FAIL;
  intrs = DisableIntrs();
  for (c = 0; c < MAX_CONDS; c++) {
    if (!conds[c].inuse) {
      if (AQueueInit(&conds[c].waiting) != QUEUE_SUCCESS) {
        RestoreIntrs(intrs);
        return SYNC_FAIL;
      }
      conds[c].lock = lock;
      conds[c].inuse = 1;
      RestoreIntrs(intrs);
      return c;
    }
  }
  RestoreIntrs(intrs);
  return SYNC_FAIL;
}

/* Enqueue, release, and sleep atomically to avoid a lost signal.
 * A signaler transfers the lock to us before waking us. */
int CondHandleWait(cond_t c) {
  Lock *k;
  Link *l;
  uint32 intrs;

  if (c < 0 || c >= MAX_CONDS || !conds[c].inuse) return SYNC_FAIL;
  intrs = DisableIntrs();
  k = &locks[conds[c].lock];
  if (k->pid != GetCurrentPid()) {
    RestoreIntrs(intrs);
    return SYNC_FAIL;
  }
  l = AQueueAllocLink((void *)currentPCB);
  if (l == NULL) {
    RestoreIntrs(intrs);
    return SYNC_FAIL;
  }
  if (AQueueInsertLast(&conds[c].waiting, l) != QUEUE_SUCCESS) {
    printf("FATAL ERROR: condition wait queue insertion failed!\n");
    exitsim();
  }
  if (LockRelease(k) != SYNC_SUCCESS) {
    printf("FATAL ERROR: condition wait lock release failed!\n");
    exitsim();
  }
  ProcessSleep();
  RestoreIntrs(intrs);
  return SYNC_SUCCESS;
}

/* Hoare variant: hand the lock to one waiter, then join the ordinary
 * lock queue. Returning from LockAcquire guarantees we own it again. */
int CondHandleSignal(cond_t c) {
  Lock *k;
  Link *l;
  PCB *waiter;
  uint32 intrs;
  int result;

  if (c < 0 || c >= MAX_CONDS || !conds[c].inuse) return SYNC_FAIL;
  intrs = DisableIntrs();
  k = &locks[conds[c].lock];
  if (k->pid != GetCurrentPid()) {
    RestoreIntrs(intrs);
    return SYNC_FAIL;
  }
  if (AQueueEmpty(&conds[c].waiting)) {
    RestoreIntrs(intrs);
    return SYNC_SUCCESS;
  }
  l = AQueueFirst(&conds[c].waiting);
  waiter = (PCB *)AQueueObject(l);
  if (AQueueRemove(&l) != QUEUE_SUCCESS) {
    printf("FATAL ERROR: condition signal queue removal failed!\n");
    exitsim();
  }
  if (LockTransfer(k, waiter) != SYNC_SUCCESS) {
    printf("FATAL ERROR: condition signal lock transfer failed!\n");
    exitsim();
  }
  ProcessWakeup(waiter);
  result = LockAcquire(k);
  RestoreIntrs(intrs);
  return result;
}
