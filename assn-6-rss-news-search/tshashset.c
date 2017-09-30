#include <assert.h>

#include "tshashset.h"

// Ctor does not need to be thread-safe
void TSHashSetNew(ts_hashset *h, int elemSize, int numBuckets, 
  HashSetHashFunction hashfn, HashSetCompareFunction comparefn, HashSetFreeFunction freefn) {
    assert(h != NULL);
    // Create mutex
    pthread_mutexattr_t reentrantAttr;
    pthread_mutexattr_init(&reentrantAttr);
    pthread_mutexattr_settype(&reentrantAttr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&h->lock, &reentrantAttr);
    // Create hashset
    HashSetNew(&h->h, elemSize, numBuckets, hashfn, comparefn, freefn);
}

// Destructor does not need to be thread-safe
void TSHashSetDispose(ts_hashset *h) {
  assert(h != NULL);
  // mutex
  assert(pthread_mutex_trylock(&h->lock) == 0);
  pthread_mutex_unlock(&h->lock);
  pthread_mutex_destroy(&h->lock);
  // hashset
  HashSetDispose(&h->h);
}

// TODO: no error checking done on mutex lock

int TSHashSetCount(ts_hashset *h) {
  assert(h != NULL);
  int ret;
  pthread_mutex_lock(&h->lock);
  ret = HashSetCount(&h->h);
  pthread_mutex_unlock(&h->lock);
  return ret;
}

void TSHashSetEnter(ts_hashset *h, const void *elemAddr) {
  assert(h != NULL);
  pthread_mutex_lock(&h->lock);
  HashSetEnter(&h->h, elemAddr);
  pthread_mutex_unlock(&h->lock);
}

void *TSHashSetLookup(ts_hashset *h, const void *elemAddr) {
  assert(h != NULL);
  void* ret;
  pthread_mutex_lock(&h->lock);
  ret = HashSetLookup(&h->h, elemAddr);
  pthread_mutex_unlock(&h->lock);
  return ret;
}

void TSHashSetMap(ts_hashset *h, HashSetMapFunction mapfn, void *auxData) {
  assert(h != NULL);
  pthread_mutex_lock(&h->lock);
  HashSetMap(&h->h, mapfn, auxData);
  pthread_mutex_unlock(&h->lock);
}