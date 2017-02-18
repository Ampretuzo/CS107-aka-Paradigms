using namespace std;
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "imdb.h"

const char *const imdb::kActorFileName = "actordata";
const char *const imdb::kMovieFileName = "moviedata";

imdb::imdb(const string& directory)
{
  const string actorFileName = directory + "/" + kActorFileName;
  const string movieFileName = directory + "/" + kMovieFileName;
  
  actorFile = acquireFileMap(actorFileName, actorInfo);
  movieFile = acquireFileMap(movieFileName, movieInfo);
}

bool imdb::good() const
{
  return !( (actorInfo.fd == -1) || 
	    (movieInfo.fd == -1) ); 
}



int imdb::playerOffsetInBytes(const string& player) const
{
  // TODO: needs bsearch
  return 0;
}

void imdb::getMovieOffsets(int playerOffset, vector<int>& movieOffsets) const
{
  // TODO
}

void imdb::pickMovieTitles(vector<int>& movieOffsets, vector<film>& films) const
{
  // TODO
}

// you should be implementing these two methods right here... 
bool imdb::getCredits(const string& player, vector<film>& films) const 
{ 
  /*
   * First we have to run bsearch to find where the record of given player
   * is located.
   * After that, we take movies he starred in by looking at offsets in movieFile
   * and insert them in given films array.
   */
   
  // get player offset in bytes
  int playerOffset = playerOffsetInBytes(player);
  
  // if player is not in the database (playerOffset = -1), return false
  if(playerOffset == -1) return false;
  
  // then, we pick offsets of movies given player played in
  vector<int> movieOffsets;
  getMovieOffsets(playerOffset, movieOffsets);
  
  // using those movie offsets, we pick actual films
  pickMovieTitles(movieOffsets, films);
   
  // finally, if we got this far, return true 
  return true;
}

bool imdb::getCast(const film& movie, vector<string>& players) const { return false; }

imdb::~imdb()
{
  releaseFileMap(actorInfo);
  releaseFileMap(movieInfo);
}

// ignore everything below... it's all UNIXy stuff in place to make a file look like
// an array of bytes in RAM.. 
const void *imdb::acquireFileMap(const string& fileName, struct fileInfo& info)
{
  struct stat stats;
  stat(fileName.c_str(), &stats);
  info.fileSize = stats.st_size;
  info.fd = open(fileName.c_str(), O_RDONLY);
  return info.fileMap = mmap(0, info.fileSize, PROT_READ, MAP_SHARED, info.fd, 0);
}

void imdb::releaseFileMap(struct fileInfo& info)
{
  if (info.fileMap != NULL) munmap((char *) info.fileMap, info.fileSize);
  if (info.fd != -1) close(info.fd);
}
