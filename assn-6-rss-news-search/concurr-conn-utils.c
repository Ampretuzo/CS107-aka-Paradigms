#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <semaphore.h>

#include "concurr-conn-utils.h"

/*
 *
 * typedef struct {
 *   sem_t totConnSem;
 *   int maxServerConns;
 *   pthread_mutex_t serverConnSemsLock;
 *   hashset serverConnSems;
 * } concurrConnUtil;
 *
 */
 
 typedef struct {
   const char* serverName;
   sem_t serverSemaphore;
 } serverConnSem;
 
static int ServerConnSemHash(const void *elem, int numBuckets);
static int ServerConnSemComp(const void *elem1, const void *elem2);
static void ServerConnSemFree(void *elem);

static const int kNumServerConnSemsBuckets = 1009;
void ConcurrConnUtilsNew(concurrConnUtil* ccu, int maxTotalConns, int maxServerConns) {
  assert(ccu != NULL);
  assert(maxTotalConns >= 0 && maxServerConns >= 0);
  // assert(maxTotalConns < SEM_VALUE_MAX);
  // 1
  sem_init(&ccu->totConnSem, 0, maxTotalConns);
  // 2
  ccu->maxServerConns = maxServerConns;
  TSHashSetNew(
    &ccu->serverConnSems, 
    sizeof(serverConnSem), 
    kNumServerConnSemsBuckets, 
    ServerConnSemHash, 
    ServerConnSemComp,
    ServerConnSemFree 
  );
}

void ConcurrConnUtilsDispose(concurrConnUtil* ccu) {
  assert(ccu != NULL);
  // 4
  TSHashSetDispose(&ccu->serverConnSems);
  // 2
  ccu->maxServerConns = 0;
  // 1
  if (sem_destroy(&ccu->totConnSem) == -1) {
    if (errno == EINVAL) {
      printf("%s\n", "\t\t!@#: trying to destroy invalid sem_t.");
    }
    exit(17);
  }
}

void ConcurrConnUtilsWait(concurrConnUtil* ccu, const char* serverName) {
  assert(ccu != NULL);
  // First of all wait for total connection semaphore
  sem_wait(&ccu->totConnSem);
  // Now find entry for server connection
  serverConnSem scsToFind = {serverName};
  serverConnSem* scs = TSHashSetLookup(&ccu->serverConnSems, &scsToFind);
  if (scs == NULL) {
    // Create and add a new serverConnSem
    serverConnSem scsToCreate;
    scsToCreate.serverName = strdup(serverName);
    sem_init(&scsToCreate.serverSemaphore, 0, ccu->maxServerConns);
    TSHashSetEnter(&ccu->serverConnSems, &scsToCreate);
    scs = TSHashSetLookup(&ccu->serverConnSems, &scsToFind);
    assert(scs != NULL);
  }
  sem_wait(&scs->serverSemaphore);
}

void ConcurrConnUtilsPost(concurrConnUtil* ccu, const char* serverName) {
  assert(ccu != NULL);
  if (sem_post(&ccu->totConnSem) == -1 && errno == EINVAL) {
    printf("%s\n", "\t\t!@#!@#: not a valid semaphore.");
  }
  serverConnSem scsToFind = {serverName};
  serverConnSem* scs = TSHashSetLookup(&ccu->serverConnSems, &scsToFind);
  assert(scs != NULL);
  if (sem_post(&scs->serverSemaphore) == -1 && errno == EINVAL) {
    printf("%s\n", "\t\t!@#!@#: not a valid semaphore.");
  }
}






























// -----------------------------------------------------------------------------

/** 
 * StringHash                     
 * ----------  
 * This function adapted from Eric Roberts' "The Art and Science of C"
 * It takes a string and uses it to derive a hash code, which   
 * is an integer in the range [0, numBuckets).  The hash code is computed  
 * using a method called "linear congruence."  A similar function using this     
 * method is described on page 144 of Kernighan and Ritchie.  The choice of                                                     
 * the value for the kHashMultiplier can have a significant effect on the                            
 * performance of the algorithm, but not on its correctness.                                                    
 * This hash function has the additional feature of being case-insensitive,  
 * hashing "Peter Pawlowski" and "PETER PAWLOWSKI" to the same code.
 *
 * Note that elem is a const void *, as it needs to be if the StringHash
 * routine is to be used as a HashSetHashFunction.  In this case, then
 * const void * is really a const char ** in disguise.
 *
 * @param elem the address of the C string to be hashed.
 * @param numBuckets the number of buckets in the hashset doing the hashing.
 * @return the hash code of the string addressed by elem.
 */  

static const signed long kHashMultiplier = -1664117991L;
int StringHash(const void *elem, int numBuckets)  
{            
  unsigned long hashcode = 0;
  const char *s = *(const char **) elem;

  for (int i = 0; i < strlen(s); i++)
    hashcode = hashcode * kHashMultiplier + tolower(s[i]);  
  
  return hashcode % numBuckets;                                
}

/**
 * Function: StringCompare
 * -----------------------
 * Accepts the two const void *s, casts them to be the
 * const char **s we know them to be, and then dereferences
 * them to produce to two C strings that need to be compared.
 * Once we produce the two C strings, we pass the buck to
 * strcasecmp and return whatever it returns.  Not surprisingly,
 * the string comparison is case-insensitive, which is exactly
 * what we want.
 *
 * @param elem1 the address of the first of two C strings.
 * @param elem2 the address of the second of two C strings.
 * @return a positive, negative, or zero number, depending on whether
 *         or not the first string is greater than, less than, or
 *         equal to the second string.
 */

int StringCompare(const void *elem1, const void *elem2)
{
  const char *s1 = *(const char **) elem1;
  const char *s2 = *(const char **) elem2;
  return strcasecmp(s1, s2);
}

/**
 * Function: StringFree
 * --------------------
 * Disposes of the char * address by the supplied void *.
 * The char *s point to dyanamically allocated character arrays
 * generated by strdup, so we rely on the stdlib free function
 * to donate the memory back to the heap. (strdup uses malloc)
 *
 * @param the address of the dynamically allocated string to be
 *        freed.
 *
 * No return value to speak of.
 */

void StringFree(void *elem)
{
  free(*(char **)elem);
}

// ---------------

static int ServerConnSemHash(const void *elem, int numBuckets) {
  serverConnSem* scs = (serverConnSem*) elem;
  return StringHash(&scs->serverName, numBuckets);
}

static int ServerConnSemComp(const void *elem1, const void *elem2) {
  serverConnSem* scs1 = (serverConnSem*) elem1;
  serverConnSem* scs2 = (serverConnSem*) elem2;
  return StringCompare(&scs1->serverName, &scs2->serverName);
}

static void ServerConnSemFree(void *elem) {
  serverConnSem* scs = (serverConnSem*) elem;
  StringFree(&scs->serverName);
  if (sem_destroy(&scs->serverSemaphore) == -1) {
    if (errno == EINVAL) {
      printf("%s\n", "\t\t!@#: trying to destroy invalid sem_t.");
    }
    exit(13);
  }
}
