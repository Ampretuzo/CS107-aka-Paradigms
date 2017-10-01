#ifndef __limited_url_connection__
#define __limited_url_connection__

#include "urlconnection.h"
#include "concurr-conn-utils.h"

typedef struct {
  concurrConnUtil ccu;
} limitedUrlFactory;

void LimitedURLConnectionFactoryNew(limitedUrlFactory* factory, int maxTotalConns, int maxServerConns);
void LimitedURLConnectionFactoryDispose(limitedUrlFactory* factory);
void LimitedURLConnectionNew(limitedUrlFactory* factory, urlconnection* urlconn, const url* u);
void LimitedURLConnectionDispose(limitedUrlFactory* factory, urlconnection* urlconn);

#endif