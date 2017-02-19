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
 * This structs help to pass offset records.
 */
struct asd {
  const string* player;
  const void* start;  // "file" would be a better name. refactor when on IDE
};
typedef struct asd playerAndRecords;// start with uppercase next time. +bad name
struct jkl {
  const film* filmm;
  const void* file;
};
typedef struct jkl MovieAndFile;

/////

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
int compareActors(const void* /* we know this is struct pointer */ p, 
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
 * This function reads necessary data about the movie and records it 
 * in given struct.
 */
void readFilm(film& film, void* film_pointer)
{
  // simple parsing according to how movie records are laid out
  film.title = (char*) film_pointer; 
  char yearByte = * ((char*) film_pointer + film.title.length() + 1);
  film.year = 1900 + (int) yearByte;
}

/*
 * Comparator for bsearch.
 * First argument is a void* to key, second is void* to offset number.
 */
int compareMovies(const void* m, const void* offsetPointer) 
{
  // key film
  const MovieAndFile* mTyped = (MovieAndFile*) m;
  const film* first = (film*) mTyped->filmm;
  
  // second film
  film second;
  int offsetInBytes = * (int*) offsetPointer;
  void* recordPointer = (void*) ((char*) mTyped->file + offsetInBytes);
  readFilm(second, recordPointer);

  if(second == *first) return 0;
  if(*first < second) return -1;
  return +1;
}

/*
 * Knowing offsets of movies for movieFile data, this method collects actual
 * movies in given vector<film>.
 */
void imdb::pickMovieTitles(vector<int>& movieOffsets, vector<film>& films) const
{
//  int i = 0;
  vector<int>::const_iterator it = movieOffsets.begin();
  // iterate over movieOffsets vector and construct film structs for each
  while(it != movieOffsets.end() )
  {
    int movieOffset = *it;
    void* film_pointer = (void*) ((char*) movieFile + movieOffset);
    film film;
    readFilm(film, film_pointer);
    films.push_back(film);
//    cout << "film# " << ++i << " : " << film.title << endl;
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
  
  // first, collect variables for bsearch:
  
  // easy, first int is number of actors
  const int numberOfActors = * (int*) actorFile;
  // offsets 'array' starts after first 2 bytes, that is, after first int
  const void* start = (void*) ( (int*) actorFile + 1);
  // bsearch key has to carry file pointer as well, hence passing struct
  playerAndRecords p; // struct name is bad, refactor when using IDE
  p.player = &player;
  p.start = actorFile;
  // run bsearch
  void* recordOffset = 
    bsearch(
      &p,  // pointer to what we are looking for
      start,
      numberOfActors, // easy
      sizeof (int), // size of each element is integer
      compareActors  // function that compares key to given element
    ); 
   
  // if player is not in the database, return false
  if(recordOffset == NULL) return false;
  
  // then, we pick offsets of movies given player played in
  vector<int> movieOffsets;
  getMovieOffsets(* (int*) recordOffset, movieOffsets);
  
  // using those movie offsets, we pick actual films
  pickMovieTitles(movieOffsets, films);
   
  // finally, if we got this far, return true 
  return true;
}

/*
 * This function gets pointer to movie record. 
 * It extracts player names according to how information is laid out on data f.
 */
void getPlayers(const void* movieRecord, 
  const void* actorFile, vector<string>& players)
{
  string title = (char*) movieRecord;
  // after title.length() bytes there is '\0' symbol and one byte for year.
  // then it is padded with '\0's to make full mod4byte = 0.
  int usedBytes = title.length() + 2;
  // used size in bytes considering padding
  int offsetBytes = (usedBytes + 1) / 2 * 2;
  // # of actors
  short n_actors = * (short*) ((char*) movieRecord + offsetBytes);
  // update offsetBytes to include 2 bytes from short
  offsetBytes = offsetBytes + 2;
  // consider padding
  offsetBytes = (offsetBytes + 2) / 4 * 4;
  // now loop and to collect actors
  for(int i = 0; i < n_actors; i++)
  {
    // juggling with pointers, easy
    int actorOffset = * ((int*) ((char*) movieRecord + offsetBytes) + i);
    players.push_back((char*) actorFile + actorOffset);
  }
}

bool imdb::getCast(const film& movie, vector<string>& players) const 
{
  /*
   * First we run bsearch to find where the record of given movie is located.
   * Then, we parse that record and populate players array.
   * Much like in getCredits method.
   */
  
  const int numberOfMovies = * (int*) movieFile;
  const void* start = (void*) ( (int*) movieFile + 1);
  MovieAndFile m;
  m.file = movieFile;
  m.filmm = &movie;
  // run bsearch
  void* recordOffset =
    bsearch(
      &m,
      start,
      numberOfMovies,
      sizeof (int),
      compareMovies
    );

  if(recordOffset == NULL) return false;
  
  // if we are here, then record exists
  int offsetInBytes = * (int*) recordOffset;
  void* movieRecord = (void*) ((char*) movieFile + offsetInBytes);
  
  // mic check
//  cout << movie.title <<" -- does it match? -- " << (char*) movieRecord << endl;

  // populate players vector by actors of given movie
  getPlayers(movieRecord, actorFile, players);
    
  // if we get this far, movie exists
  return true; 
}

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
