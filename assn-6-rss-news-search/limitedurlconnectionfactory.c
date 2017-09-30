#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "limitedurlconnectionfactory.h"


void LimitedURLConnectionFactoryNew(limitedUrlFactory* factory, int maxTotalConns, int maxServerConns) {
  assert(factory != NULL);
  ConcurrConnUtilsNew(&factory->ccu, maxTotalConns, maxServerConns);
}

void LimitedURLConnectionFactoryDispose(limitedUrlFactory* factory) {
  assert(factory != NULL);
  ConcurrConnUtilsDispose(&factory->ccu);
}

void LimitedURLConnectionNew(limitedUrlFactory* factory, urlconnection* urlconn, const url* u) {
  assert(factory != NULL);
  ConcurrConnUtilsWait(&factory->ccu, u->serverName);
  URLConnectionNew(urlconn, u);
}

void LimitedURLConnectionDispose(limitedUrlFactory* factory, urlconnection* urlconn) {
  assert(factory != NULL);
  // Dispose of urlcon
  char* fullUrl = strdup(urlconn->fullUrl);
  URLConnectionDispose(urlconn);
  // Find out server name and return semaphores
  url u;
  URLNewAbsolute(&u, fullUrl);
  ConcurrConnUtilsPost(&factory->ccu, u.serverName);
  URLDispose(&u);
  free(fullUrl);
}