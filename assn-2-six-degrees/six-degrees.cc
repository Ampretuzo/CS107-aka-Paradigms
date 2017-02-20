#include <vector>
#include <queue>  // ill use queue instead of list
#include <set>
#include <string>
#include <iostream>
#include <iomanip>
#include <assert.h>
#include "imdb.h"
#include "path.h"
using namespace std;

#define HOW_FAR 6

/*
 * ...
 * At this point we already know that two actors are different and they are
 * present in the database.
 * ...
 */
bool generateShortestPath(const string& target, path& p, const imdb& db)
{
  // extract source player name from p
  string source = p.getLastPlayer();  // assuming path is empty, which it should
  path p0(source);  // genesis path to build upon
  /*
   * We are writing bfs search.
   * Hence queue.
   */
  queue<path> q;
  q.push(p0);
  /*
   * To avoid possible infinite search we have to remember which actors have
   * already been observed.
   * Using set.
   */
  set<string> seenActors;
  seenActors.insert(source);
  /*
   * We should also remember seen films.
   */
  set<film> seenMovies;
   
  while(true)
  {
    // safety measure
    if(q.size() == 0) return false;
    // take front path
    path queuedPath = q.front();
    q.pop();
    // since size of paths monotonically increases, it is safe to check front
    // paths size
    if(queuedPath.getLength() == HOW_FAR) return false;
    // take name of last actor in path
    /* const? */ string lastActor = queuedPath.getLastPlayer();
    // if this is the path, last player will be target
    if(target.compare(lastActor) == 0)
    {
      cout << queuedPath << endl;
      return false;
    }
    // if not, we move forward by generating child paths and enqueueing them
    vector<film> moviesByLastActor;
    db.getCredits(lastActor, moviesByLastActor);
    vector<film>::const_iterator it = moviesByLastActor.begin();
    while(it != moviesByLastActor.end() )
    {
      film movieByLastActor = *it++;
      // if set already contains *it film then move forward
      if(seenMovies.insert(movieByLastActor).second == false) continue;
      // if insert was successful, take actors
      vector<string> costars;
      db.getCast(movieByLastActor, costars);
      vector<string>::const_iterator itActor = costars.begin();
      while(itActor != costars.end() )
      {
        string coactor = * itActor++;
        // if seen actors set already contains coactor then move forward
        if(seenActors.insert(coactor).second == false) continue;
        // if insert was successful, build new path and queue it
        path newPath = path(queuedPath);
        newPath.addConnection(movieByLastActor, coactor);
        q.push(newPath);
      }
    }
  }
  
}




/**
 * Using the specified prompt, requests that the user supply
 * the name of an actor or actress.  The code returns
 * once the user has supplied a name for which some record within
 * the referenced imdb existsif (or if the user just hits return,
 * which is a signal that the empty string should just be returned.)
 *
 * @param prompt the text that should be used for the meaningful
 *               part of the user prompt.
 * @param db a reference to the imdb which can be used to confirm
 *           that a user's response is a legitimate one.
 * @return the name of the user-supplied actor or actress, or the
 *         empty string.
 */

static string promptForActor(const string& prompt, const imdb& db)
{
  string response;
  while (true) {
    cout << prompt << " [or <enter> to quit]: ";
    getline(cin, response);
    if (response == "") return "";
    vector<film> credits;
    if (db.getCredits(response, credits)) return response;
    cout << "We couldn't find \"" << response << "\" in the movie database. "
	 << "Please try again." << endl;
  }
}

/**
 * Serves as the main entry point for the six-degrees executable.
 * There are no parameters to speak of.
 *
 * @param argc the number of tokens passed to the command line to
 *             invoke this executable.  It's completely ignored
 *             here, because we don't expect any arguments.
 * @param argv the C strings making up the full command line.
 *             We expect argv[0] to be logically equivalent to
 *             "six-degrees" (or whatever absolute path was used to
 *             invoke the program), but otherwise these are ignored
 *             as well.
 * @return 0 if the program ends normally, and undefined otherwise.
 */

int main(int argc, const char *argv[])
{
  imdb db(determinePathToData(argv[1])); // inlined in imdb-utils.h
  if (!db.good()) {
    cout << "Failed to properly initialize the imdb database." << endl;
    cout << "Please check to make sure the source files exist and that you have permission to read them." << endl;
    exit(1);
  }
  
  while (true) {
    string source = promptForActor("Actor or actress", db);
    if (source == "") break;
    string target = promptForActor("Another actor or actress", db);
    if (target == "") break;
    if (source == target) {
      cout << "Good one.  This is only interesting if you specify two different people." << endl;
    } else {
      path p(source);
      if(!generateShortestPath(target, p, db) )
        cout << endl << "No path between those two people could be found." << endl << endl;
      else
        cout << p << endl;
    }
  }
  
  cout << "Thanks for playing!" << endl;
  return 0;
}

