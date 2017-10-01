#ifndef __thread_utils__
#define __thread_utils__

#include "vector.h"

/**
 * Supports only void threads.
 */
 
typedef void* (*ThreadUtilsRunnableFn)(void* fnArg);
typedef void (*ThreadUtilsDisposeArg)(void* fnArg);

typedef struct {
  vector threads;
  ThreadUtilsRunnableFn runnable;
  ThreadUtilsDisposeArg argFreeFn;
} threads;


void ThreadUtilsNew(threads* ths, ThreadUtilsRunnableFn runnable, ThreadUtilsDisposeArg disposeRunnableArg);
void ThreadUtilsDispose(threads* ths);
void ThreadUtilsStartThread(threads* ths, void* runnableArg);
void ThreadUtilsJoin(threads* ths);

#endif