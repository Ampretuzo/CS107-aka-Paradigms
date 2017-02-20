#include "vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define DEFAULT_ALLOC_SIZE 16

void VectorNew(vector *v, int elemSize, VectorFreeFunction freeFn, int initialAllocation)
{

  assert(elemSize > 0);
  v->elemSize = elemSize;

  v->logLen = 0;
  
  assert(initialAllocation >= 0);
  if(initialAllocation == 0) initialAllocation = DEFAULT_ALLOC_SIZE;
  v->allocLen = initialAllocation;
  v->allocIncLen = initialAllocation;
  
  v->freeElem = &freeFn;
  
  v->start = calloc(v->allocLen, v->elemSize);
  
}

void VectorDispose(vector *v)
{
  for(int i = 0; i < v->logLen; i++)
  {
    void* elemAddr = (char*) v->start + i*v->elemSize;
    (* v->freeElem)(elemAddr);
  }
  free(v->start);
}

int VectorLength(const vector *v) { return v->logLen; }

void *VectorNth(const vector *v, int position)
{
  assert(position >= 0);
  assert(position <= v->logLen - 1);
  return (char*) v->start + position * v->elemSize;
}

void VectorReplace(vector *v, const void *elemAddr, int position)
{}

void VectorInsert(vector *v, const void *elemAddr, int position)
{
  // TODO
}

void VectorAppend(vector *v, const void *elemAddr)
{
  if(v->logLen == v->allocLen)  // resize if necessary
  {
    void* newStart = 
      realloc(v->start, (v->allocLen + v->allocIncLen) * v->elemSize);
    assert(newStart != NULL);
    v->allocLen += v->allocIncLen;
  }
  // now feel free to append
  void* dest = (void*) ((char*) v->start + v->elemSize * v->logLen);
  dest = memcpy(dest, elemAddr, v->elemSize);
  assert(dest != NULL);
  v->logLen++;
}

void VectorDelete(vector *v, int position)
{}

void VectorSort(vector *v, VectorCompareFunction compare)
{}

void VectorMap(vector *v, VectorMapFunction mapFn, void *auxData)
{}

static const int kNotFound = -1;
int VectorSearch(const vector *v, const void *key, VectorCompareFunction searchFn, int startIndex, bool isSorted)
{ return -1; } 
