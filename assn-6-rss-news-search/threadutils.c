#include <assert.h>
#include <pthread.h>
#include <stdio.h>

#include "threadutils.h"

/*
typedef struct {
vector threads;
ThreadUtilsRunnableFn runnable;
ThreadUtilsDisposeArg argFreeFn;
} threads;
*/

typedef struct {
  pthread_t thread;
  void* runnableArg;
  ThreadUtilsDisposeArg argFreeFn;
} thread;

static void ThreadUtilsDisposeThreadArg(void *elemAddr);
static void ThreadUtilsJoinThread(void *elemAddr, void *auxData);


void ThreadUtilsNew(threads* ths, ThreadUtilsRunnableFn runnable, ThreadUtilsDisposeArg argFreeFn) {
  assert(ths != NULL);
  ths->runnable = runnable;
  ths->argFreeFn = argFreeFn;
  VectorNew(&ths->threads, sizeof(thread), ThreadUtilsDisposeThreadArg, 0);
}

void ThreadUtilsDispose(threads* ths) {
  assert(ths != NULL);
  VectorDispose(&ths->threads);
  ths->runnable = NULL;
  // After we disposed with the ths->threads vector its safe to erase argFreeFn
  ths->argFreeFn = NULL;
}

void ThreadUtilsStartThread(threads* ths, void* runnableArg) {
  assert(ths != NULL);
  thread th;
  th.runnableArg = runnableArg;
  th.argFreeFn = ths->argFreeFn;
  if (pthread_create(&th.thread, NULL, ths->runnable, runnableArg) != 0) {
    // TODO: Handle bad things
    printf("%s\n", "\t\t!@#: could not create thread!");
  }
  VectorAppend(&ths->threads, &th);
}

void ThreadUtilsJoin(threads* ths) {
  assert(ths != NULL);
  VectorMap(&ths->threads, ThreadUtilsJoinThread, NULL);
}

// private functions

static void ThreadUtilsDisposeThreadArg(void *elemAddr) {
  assert(elemAddr != NULL);
  thread* th = (thread*) elemAddr;
  if (th->argFreeFn != NULL) {
    (* th->argFreeFn)(th->runnableArg);
  }
  th->argFreeFn = NULL;
}

static void ThreadUtilsJoinThread(void *elemAddr, void *auxData) {
  assert(auxData == NULL);
  assert(elemAddr != NULL);
  thread* th = (thread*) elemAddr;
  if (pthread_join(th->thread, NULL) != 0 ) {
    // TODO: handle
    printf("%s\n", "\t\t!@#: could not join the thread!");
  }
}