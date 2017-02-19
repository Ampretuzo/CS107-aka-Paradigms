using namespace std;
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>	// bsearch
#include "imdb.h"

const char *const imdb::kActorFileName = "actordata";
const char *const imdb::kMovieFileName = "moviedata";

/*
 * This struct helps to pass offset records.
 */
struct asd {
  const string* player;
  const void* start;
};
typedef struct asd playerAndRecords;

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

/*
 * This comparator compares two strings.
 * First string is key and is given through *p.player.
 * Second string is obtained by looking at given byte offset 
 * starting from p.start pointer.
 */
int compar(const void* /* we know this is struct pointer */ p, 
  const void* offsetPointer)
{
  const playerAndRecords* pTyped = (playerAndRecords*) p;
  const string key = * pTyped->player;
  int offsetInBytes = * (int*) offsetPointer;
  string elem = (char*) pTyped->start + offsetInBytes;
//  cout << elem << ' ' << key << endl; // track bsearch if you want
  return key.compare(elem);
}

int imdb::playerOffsetInBytes(const string& player) const
{
  // first, collect variables for bsearch:
  // easy, first int is number of actors
  const int numberOfActors = * (int*) actorFile;
  // offsets 'array' starts after first 2 bytes, that is, after first int
  const void* start = (void*) ( (int*) actorFile + 1);
  // bsearch key has to carry file pointer as well, hence passing struct
  playerAndRecords p;
  p.player = &player;
  p.start = actorFile;
  
  void* actorLocation = 
    bsearch(
      &p,  // pointer to what we are looking for
      start,
      numberOfActors, // easy
      sizeof (int), // size of each element is integer
      compar  // function that compares key to given element
    );
    
  // bad style but thats it..
  return actorLocation == NULL ? -1 : * (int*) actorLocation;
}

void imdb::getMovieOffsets(int playerOffset, vector<int>& movieOffsets) const
{
  // TODO
  cout << "if this matches you input you good: " << (char*) actorFile + playerOffset << endl;
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
