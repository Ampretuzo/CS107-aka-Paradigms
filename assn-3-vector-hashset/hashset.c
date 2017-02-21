#include "hashset.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>


#define HEURISTIC_VEC_SIZE 4

/* helpers */

static void assertHashSetValid(hashset* h)
{
  assert(h->elemSize > 0);
  assert(h->numBuckets > 0);
  assert(h->hashfn != NULL);
  assert(h->comparefn != NULL);
  // note that if elements do not contain pointers to dinamically
  // allocated memory, h->freefn would be NULL
  assert(h->start != NULL); // assert calloc was successful
}
 
static void initVectors(hashset* h)
{
  for(int i = 0; i < h->numBuckets; i++)
    VectorNew(
      (vector*) h->start + i, 
      h->elemSize, 
      h->freefn, 
      HEURISTIC_VEC_SIZE
    );
}

static void disposeVectors(hashset* h)
{
  for(int i = 0; i < h->numBuckets; i++)
    VectorDispose( (vector*) h->start + i );
}

/* TODO unite initVectors and disposeVectors in some sort of map function. */
/* Actually, not worth it, since some additional structs will be needed.. */

// returns corret vector to insert into for convenience
static vector* assertElem(const hashset* h, const void* elemAddr)
{
  assert(elemAddr != NULL);
  int hash = (* h->hashfn)(elemAddr, h->numBuckets);
  assert(hash >= 0);
  assert(hash < h->numBuckets);
  return (vector*) h->start + hash;
}
 
/* helpers end */


void HashSetNew(hashset *h, int elemSize, int numBuckets,
		HashSetHashFunction hashfn, HashSetCompareFunction comparefn, 
		HashSetFreeFunction freefn)
{
  h->elemSize = elemSize;
  h->numBuckets = numBuckets;
  h->hashfn = hashfn;
  h->comparefn = comparefn;
  h->freefn = freefn;
  // vector array
  h->start = calloc(h->numBuckets, sizeof(vector) );
  assertHashSetValid(h);
  // without too much super-optimization, init all vectors
  initVectors(h);
}

void HashSetDispose(hashset *h)
{
  // send dispose message to vectors first
  disposeVectors(h);
  free(h->start);
}

int HashSetCount(const hashset *h)
{
  int cnt = 0;
  for(int i = 0; i < h->numBuckets; i++)
    cnt += VectorLength( (vector*) h->start + i);
  return cnt;
}

void HashSetMap(hashset *h, HashSetMapFunction mapfn, void *auxData)
{
  assert(mapfn != NULL);
  for(int i = 0; i < h->numBuckets; i++)
    VectorMap( (vector*) h->start + i, mapfn, auxData);
}

/* this function must leave vector sorted! */
void HashSetEnter(hashset *h, const void *elemAddr)
{
  vector* v = assertElem(h, elemAddr);  // vector* to insert into
  // if an equal element is present replace it 
  int pos = VectorSearch(v, elemAddr, h->comparefn, 0, true);
  if(pos != -1)
  {
    VectorReplace(v, elemAddr, pos);  // this leaves vector sorted
    return;
  }
/* legacy O(n log n) code */
/*  VectorAppend(v, elemAddr);*/
/*  VectorSort(v, h->comparefn);*/
  VectorInsertSorted(v, elemAddr, h->comparefn);
}

/* Pointer returned from this becomes invalid after operations on hashset and*/
/* will damage structure. */
void *HashSetLookup(const hashset *h, const void *elemAddr)
{
  vector* v = assertElem(h, elemAddr);
  int pos = VectorSearch(v, elemAddr, h->comparefn, 0, true);
  if(pos == -1) return NULL;
  return VectorNth(v, pos);
}
