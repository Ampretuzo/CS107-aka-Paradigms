#include "vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define DEFAULT_ALLOC_SIZE 16


/* helpers */

// returns pointer to elem on given position <should this be inline?>
static void* elemAddr(const vector* v, int position)
{
  return (char*) v->start + v->elemSize * position;
}

// this function asserts that given position is >= 0 and < logLen
static /* private? */ void assertPosInBounds(const vector *v, int position)
{
  assert(position >= 0);
  assert(position < v->logLen);
}

static void* freePosition(vector* v, int position)
{
  void* dest = elemAddr(v, position);
  if(v->freeElem != NULL) (* v->freeElem)(dest);
  return dest;
}

// this function adds capacity to underlying array if allocated mem not enough
static void resizeIfSaturated(vector *v)
{
  if(v->allocLen != v->logLen) return;  // nothing to do
  int newAllocLen = v->allocLen + v->allocIncLen;
  void* newStart = 
    realloc(v->start, v->elemSize * newAllocLen);
  assert(newStart != NULL);
  v->allocLen += v->allocIncLen;
  v->start = newStart;  // this was the bug
}

// this function shifts array tail left or right.
// ! >>> allocations and mem free must be handled beforehand <<< !
// when l_ir_r is true shift is to the right
static void shiftTailOneStep(vector *v, int position, bool l_or_r)
{
  int hop = -1;
  if(l_or_r) hop = 1; 
  void* src = elemAddr(v, position);
  void* dest = (void*) ((char*) src + v->elemSize * hop);
  size_t n = v->elemSize * (v->logLen - position);
  dest = memmove(dest, src, n);
  assert(dest != NULL);
  v->logLen += hop;
}

// this function returns index of element with given base.
// consistency checking must be done outside. 
static int getIndex(const vector* v, void* base)
{
  int bytesDist = (char*) base - (char*) v->start;
  return bytesDist / v->elemSize;
}

/* returns position at which inserting specified key in array will not*/
/* violate array ordering.*/
/* follows bsearch prototype.   */
static const void* bsearch1(const void *key, const void *base,
  size_t nmemb, size_t size,
  int (*compar)(const void *, const void *) )
{
  // base case
  if(nmemb == 0) return base; // bsearch returns NULL in this case
  void* mid = (void*) ((char*) base + (nmemb / 2) * size);
  int cmp = (*compar)(key, mid);
  if(cmp == 0) return mid;
  return cmp < 0 ? 
    bsearch1(key, base, nmemb/2, size, compar) : 
    bsearch1(key, (char*) mid + size, nmemb - nmemb/2 - 1, size, compar);
}


/* end helpers */






void VectorNew(vector *v, int elemSize, 
  VectorFreeFunction freeFn, int initialAllocation)
{

  assert(elemSize > 0);
  v->elemSize = elemSize;

  v->logLen = 0;
  
  assert(initialAllocation >= 0);
  if(initialAllocation == 0) initialAllocation = DEFAULT_ALLOC_SIZE;
  v->allocLen = initialAllocation;
  v->allocIncLen = initialAllocation;
  
  v->freeElem = freeFn;
  
  v->start = calloc(v->allocLen, v->elemSize);
  
}

void VectorDispose(vector *v)
{
  // free elements
  for(int i = 0; i < v->logLen; i++)
    freePosition(v, i);
  // free alloc array
  free(v->start);
}

int VectorLength(const vector *v) { return v->logLen; }

void* VectorNth(const vector *v, int position)
{
  assertPosInBounds(v, position);
  return elemAddr(v, position);
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

void VectorInsert(vector *v, const void *elemAddr1, int position)
{
/*  // this assert is for IN bounds, whereas insert might on the edge*/
/*  assertPosInBounds(v, position);*/
  // proper assert
  assert(position >= 0);
  assert(position <= v->logLen);
  resizeIfSaturated(v);
  // move memory after position one quantum right
  shiftTailOneStep(v, position, true);
  void* dest = elemAddr(v, position);
  dest = memcpy(dest, elemAddr1, v->elemSize);
  assert(dest != NULL);
}

void VectorInsertSorted(vector *v, const void *elemAddr, 
  VectorCompareFunction searchFn)
{
  assert(elemAddr != NULL);
  const void* there = bsearch1(elemAddr, v->start, v->logLen, v->elemSize, searchFn);
  int pos = ((char*) there - (char*) v->start) / v->elemSize;
  VectorInsert(v, elemAddr, pos);
}

void VectorAppend(vector *v, const void *elemAddr1)
{
/*  resizeIfSaturated(v);*/
/*  // now feel free to append*/
/*  void* dest = elemAddr(v, v->logLen);*/
/*  dest = memcpy(dest, elemAddr1, v->elemSize);*/
/*  assert(dest != NULL);*/
/*  v->logLen++;*/
  // it makes more sense to just use VectorInsert.
  // only redundant call is shiftTailOneStep, which only decreases performance
  // by negligible O(1), while code becomes much cleaner.
  VectorInsert(v, elemAddr1, v->logLen);
}

void VectorDelete(vector *v, int position)
{
  assertPosInBounds(v, position);
  freePosition(v, position);
  // move everything after position one quantum left
  shiftTailOneStep(v, position + 1, false);
}

void VectorSort(vector *v, VectorCompareFunction compare)
{
  assert(compare != NULL);
  qsort(
    v->start, 
    v->logLen,
    v->elemSize,
    compare
  );
}

void VectorMap(vector *v, VectorMapFunction mapFn, void *auxData)
{
  for(int i = 0; i < v->logLen; i++)
    (* mapFn) (elemAddr(v, i), auxData);
}

static const int kNotFound = -1;

int VectorSearch(const vector *v, const void *key, 
  VectorCompareFunction searchFn, int startIndex, bool isSorted)
{
  assert(searchFn != NULL);
  assert(key != NULL);
  assert(startIndex >= 0);
  assert(startIndex <= v->logLen);
  if(isSorted)  // bsearch
  {
    void* match = bsearch(
      key,
      elemAddr(v, startIndex),
      v->logLen - startIndex,
      v->elemSize,
      searchFn
    );
    return match == NULL ? kNotFound : getIndex(v, match);
  }
  // linear search
  for(int i = 0; i < v->logLen - startIndex; i++)
    if((* searchFn)(key, elemAddr(v, startIndex + i)) == 0)
      return startIndex + i;
  return -1;
}
