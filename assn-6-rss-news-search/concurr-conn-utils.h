#ifndef __concurr_conn_utils__
#define __concurr_conn_utils__

#include <semaphore.h>
#include <pthread.h>
#include <errno.h>

#include "tshashset.h"

typedef struct {
  sem_t totConnSem;
  int maxServerConns;
  ts_hashset serverConnSems;
} concurrConnUtil;

int StringHash(const void *elem, int numBuckets);
int StringCompare(const void *elem1, const void *elem2);
void StringFree(void *elem);

void ConcurrConnUtilsNew(concurrConnUtil* ccu, int maxTotalConns, int maxServerConns);
void ConcurrConnUtilsDispose(concurrConnUtil* ccu);
void ConcurrConnUtilsWait(concurrConnUtil* ccu, const char* serverName);
void ConcurrConnUtilsPost(concurrConnUtil* ccu, const char* serverName);



#endif
