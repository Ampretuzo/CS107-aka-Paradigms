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

// this function asserts that given position is >= 0 and < logLen
static /* private? */ void assertPosInBounds(const vector *v, int position)
{
  assert(position >= 0);
  assert(position < v->logLen);
}

void *VectorNth(const vector *v, int position)
{
  assertPosInBounds(v, position);
  return (char*) v->start + position * v->elemSize;
}

static void* freePosition(vector *v, int position)
{
  void* dest = (void*) ((char*) v->start + v->elemSize * position);
  free(dest);
  return dest;
}

void VectorReplace(vector *v, const void *elemAddr, int position)
{
  assertPosInBounds(v, position);
  // free host memory loc
  void* dest = freePosition(v, position);
  // copy new elem
  dest = memcpy(dest, elemAddr, v->elemSize);
  assert(dest != NULL);
}

// this function adds capacity to underlying array if allocated mem not enough
static void resizeIfSaturated(vector *v)
{
  void* newStart = 
    realloc(v->start, (v->allocLen + v->allocIncLen) * v->elemSize);
  assert(newStart != NULL);
  v->allocLen += v->allocIncLen;
}

// this function shifts every element from position left or right.
// ! >>> allocations and mem free must be handled beforehand <<< !
// when l_ir_r is true shift is to the right
static void shiftTailOneStep(vector *v, int position, bool l_or_r)
{
  int hop = -1;
  if(l_or_r) hop = 1; 
  void* src = (void*) ((char*) v->start + v->elemSize * position);
  void* dest = (void*) ((char*) src + v->elemSize * hop);
  size_t n = v->elemSize * (v->logLen - position);
  dest = memmove(dest, src, n);
  assert(dest != NULL);
  v->logLen += hop;
}

void VectorInsert(vector *v, const void *elemAddr, int position)
{
  assertPosInBounds(v, position);
  resizeIfSaturated(v);
  // move memory after position one quantum right
  shiftTailOneStep(v, position, true);
  void* dest = (void*) ((char*) v->start + v->elemSize * position);
  dest = memcpy(dest, elemAddr, v->elemSize);
  assert(dest != NULL);
}

void VectorAppend(vector *v, const void *elemAddr)
{
  if(v->logLen == v->allocLen)  resizeIfSaturated(v);
  // now feel free to append
  void* dest = (void*) ((char*) v->start + v->elemSize * v->logLen);
  dest = memcpy(dest, elemAddr, v->elemSize);
  assert(dest != NULL);
  v->logLen++;
}

void VectorDelete(vector *v, int position)
{
  assertPosInBounds(v, position);
  freePosition(v, position);
  // move everything after position one quantum left
  shiftTailOneStep(v, position, false);
}

void VectorSort(vector *v, VectorCompareFunction compare)
{}

void VectorMap(vector *v, VectorMapFunction mapFn, void *auxData)
{}

static const int kNotFound = -1;
int VectorSearch(const vector *v, const void *key, VectorCompareFunction searchFn, int startIndex, bool isSorted)
{ return -1; } 
