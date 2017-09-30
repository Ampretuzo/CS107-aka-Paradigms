#ifndef __ts_vector_
#define __ts_vector_

#include <pthread.h>

#include "vector.h"
#include "bool.h"

typedef struct {
  vector v;
  pthread_mutex_t lock;
} ts_vector;



void TSVectorNew(ts_vector *v, int elemSize, VectorFreeFunction freefn, int initialAllocation);

void TSVectorDispose(ts_vector *v);

int TSVectorLength(ts_vector *v);

void *TSVectorNth(ts_vector *v, int position);

void TSVectorInsert(ts_vector *v, const void *elemAddr, int position);

void TSVectorAppend(ts_vector *v, const void *elemAddr);

void TSVectorReplace(ts_vector *v, const void *elemAddr, int position);

void TSVectorDelete(ts_vector *v, int position);

int TSVectorSearch(ts_vector *v, const void *key, VectorCompareFunction searchfn, int startIndex, bool isSorted);

void TSVectorSort(ts_vector *v, VectorCompareFunction comparefn);

void TSVectorMap(ts_vector *v, VectorMapFunction mapfn, void *auxData);


#endif
