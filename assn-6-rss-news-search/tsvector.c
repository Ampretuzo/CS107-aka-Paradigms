#include <assert.h>

#include "tsvector.h"

// Not threadsafe
void TSVectorNew(ts_vector *v, int elemSize, VectorFreeFunction freefn, int initialAllocation) {
  assert(v != NULL);
  // Create mutex
  pthread_mutexattr_t reentrantAttr;
  pthread_mutexattr_init(&reentrantAttr);
  pthread_mutexattr_settype(&reentrantAttr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&v->lock, &reentrantAttr);
  // Create vector
  VectorNew(&v->v, elemSize, freefn, initialAllocation);
}

// Not threadsafe
void TSVectorDispose(ts_vector *v) {
  assert(v != NULL);
  // mutex
  assert(pthread_mutex_trylock(&v->lock) == 0);
  pthread_mutex_unlock(&v->lock);
  pthread_mutex_destroy(&v->lock);
  // vector
  VectorDispose(&v->v);
}

int TSVectorLength(ts_vector *v) {
  assert(v != NULL);
  int ret;
  pthread_mutex_lock(&v->lock);
  ret = VectorLength(&v->v);
  pthread_mutex_unlock(&v->lock);
  return ret;
}

void *TSVectorNth(ts_vector *v, int position) {
  assert(v != NULL);
  void* ret;
  pthread_mutex_lock(&v->lock);
  ret = VectorNth(&v->v, position);
  pthread_mutex_unlock(&v->lock);
  return ret;
}

void TSVectorInsert(ts_vector *v, const void *elemAddr, int position) {
  assert(v != NULL);
  pthread_mutex_lock(&v->lock);
  VectorInsert(&v->v, elemAddr, position);
  pthread_mutex_unlock(&v->lock);
}

void TSVectorAppend(ts_vector *v, const void *elemAddr) {
  assert(v != NULL);
  pthread_mutex_lock(&v->lock);
  VectorAppend(&v->v, elemAddr);
  pthread_mutex_unlock(&v->lock);
}

void TSVectorReplace(ts_vector *v, const void *elemAddr, int position) {
  assert(v != NULL);
  pthread_mutex_lock(&v->lock);
  VectorReplace(&v->v, elemAddr, position);
  pthread_mutex_unlock(&v->lock);
}

void TSVectorDelete(ts_vector *v, int position) {
  assert(v != NULL);
  pthread_mutex_lock(&v->lock);
  VectorDelete(&v->v, position);
  pthread_mutex_unlock(&v->lock);
}

int TSVectorSearch(ts_vector *v, const void *key, VectorCompareFunction searchfn, int startIndex, bool isSorted) {
  assert(v != NULL);
  int ret;
  pthread_mutex_lock(&v->lock);
  ret = VectorSearch(&v->v, key, searchfn, startIndex, isSorted);
  pthread_mutex_unlock(&v->lock);
  return ret;
}

void TSVectorSort(ts_vector *v, VectorCompareFunction comparefn) {
  assert(v != NULL);
  pthread_mutex_lock(&v->lock);
  VectorSort(&v->v, comparefn);
  pthread_mutex_unlock(&v->lock);
}

void TSVectorMap(ts_vector *v, VectorMapFunction mapfn, void *auxData) {
  
}
