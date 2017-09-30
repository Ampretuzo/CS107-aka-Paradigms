#ifndef __ts_hashset_
#define __ts_hashset_

#include <pthread.h>

#include "hashset.h"

typedef struct {
  hashset h;
  pthread_mutex_t lock;
} ts_hashset;

void TSHashSetNew(ts_hashset *h, int elemSize, int numBuckets, 
		HashSetHashFunction hashfn, HashSetCompareFunction comparefn, HashSetFreeFunction freefn);

void TSHashSetDispose(ts_hashset *h);

int TSHashSetCount(ts_hashset *h);

void TSHashSetEnter(ts_hashset *h, const void *elemAddr);

void *TSHashSetLookup(ts_hashset *h, const void *elemAddr);

void TSHashSetMap(ts_hashset *h, HashSetMapFunction mapfn, void *auxData);
     
#endif
