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
      (void*) ((char*) h->start + i * sizeof(vector)), 
      h->elemSize, 
      h->freefn, 
      HEURISTIC_VEC_SIZE
    );
}

static void disposeVectors(hashset* h)
{
  for(int i = 0; i < h->numBuckets; i++)
    VectorDispose( (void*) ( (char*) h->start + i * sizeof(vector) ) );
}

// TODO unite initVectors and disposeVectors in some sort of map function
 
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
{ return 0; }

void HashSetMap(hashset *h, HashSetMapFunction mapfn, void *auxData)
{}

void HashSetEnter(hashset *h, const void *elemAddr)
{}

void *HashSetLookup(const hashset *h, const void *elemAddr)
{ return NULL; }
