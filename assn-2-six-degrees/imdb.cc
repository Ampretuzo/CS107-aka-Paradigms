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

/*
 * Given player name as string, this method locates corresponding record
 * in actorFile data.
 * Returns offset in bytes.
 */
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

/*
 * Knowing where actor record is in data, this method collects his movies
 * as byte offsets of movieFile data in given vector<int>.
 */
void imdb::getMovieOffsets(int playerOffset, vector<int>& movieOffsets) const
{
  // this is where player record starts
  void* playerRecord = (void*) ((char*) actorFile + playerOffset);
  // we already had name to pass, but I think it is better to construct it anew
  string player = (char*) playerRecord;
  // name is padded with '\0' in case string name does not take even bytes,
  // actual length will be
  int paddedLength = (player.length() + 2)/2 * 2;
  // then we can pick number of movies he played in
  int n_films = * (short*) ((char*) playerRecord + paddedLength);
  // movie offsets are starting after number of films, but we have to take
  // possible mod4=0 padding into accout. Take note that occupied bytes
  // is always even!
  void* playerMovies = 
    (void*) ((char*) playerRecord + 
    (/* this is even apriori */paddedLength + 
    /* 2 for short number of films*/ 2 + 
    2)/4 * 4);
  // now iterate and record offsets in given vector
  for(int i = 0; i < n_films; i++)
  {
    int movieOffset = * ((int*) playerMovies + i);
    movieOffsets.push_back(movieOffset);
  }
  // thats it (if not buggy of course)
  
//  cout << "if this matches you input you good: " << player << endl;
//  cout << "this guy played in " << n_films << " movies." << endl;
}

/*
 * Knowing offsets of movies for movieFile data, this method collects actual
 * movies in given vector<film>.
 */
void imdb::pickMovieTitles(vector<int>& movieOffsets, vector<film>& films) const
{
  // TODO
  // iterate over movieOffsets vector
  int i = 0;
  vector<int>::const_iterator it = movieOffsets.begin();
  while(it != movieOffsets.end() )
  {
    int movieOffset = *it;
    string title = (char*) movieFile + movieOffset;
    cout << "film# " << ++i << " : " << title << endl;
    it++;
  }
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
