#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <expat.h>
#include <pthread.h>  // using pthread.h instead of unavailable thread_107.h

#include "url.h"
#include "bool.h"
#include "urlconnection.h"
#include "limitedurlconnectionfactory.h"
#include "streamtokenizer.h"
#include "html-utils.h"
#include "vector.h"
#include "tsvector.h"
// #include "hashset.h"
#include "tshashset.h"
// threads library not available
// #include "thread_107.h"
#include "threadutils.h"

#define MAX_TOT_CONNS 36
#define MAX_SER_CONNS 8


typedef struct {
  ts_hashset stopWords;
  ts_hashset indices;
  ts_vector previouslySeenArticles;
} rssDatabase;

typedef struct {
  char title[2048];
  char url[2048];
  char *activeField; // field that should be populated... 
} rssFeedEntry;

typedef struct {
  rssDatabase *db;
  rssFeedEntry entry;
} rssFeedState;

typedef struct {
  const char *title;
  const char *server;
  const char *fullURL;
} rssNewsArticle;

typedef struct {
  const char *meaningfulWord;
  ts_vector relevantArticles;
} rssIndexEntry;

typedef struct {
  int articleIndex;
  int freq;
} rssRelevantArticleEntry;

static void Welcome(const char *welcomeTextURL);
static void LoadStopWords(ts_hashset *stopWords, const char *stopWordsURL);
static void BuildIndices(rssDatabase *db, const char *feedsFileName);
static void ProcessFeed(rssDatabase *db, limitedUrlFactory* luf, const char *remoteDocumentName);
static void PullAllNewsItems(rssDatabase *db, urlconnection *urlconn);

static void ProcessStartTag(void *userData, const char *name, const char **atts);
static void ProcessEndTag(void *userData, const char *name);
static void ProcessTextData(void *userData, const char *text, int len);

static void ParseArticle(rssDatabase *db, const char *articleTitle, const char *articleURL);
static void ScanArticle(streamtokenizer *st, int articleID, ts_hashset *indices, ts_hashset *stopWords);
static bool WordIsWorthIndexing(const char *word, ts_hashset *stopWords);
static void AddWordToIndices(ts_hashset *indices, const char *word, int articleIndex);
static void QueryIndices(rssDatabase *db);
static void ProcessResponse(rssDatabase *db, const char *word);
static void ListTopArticles(rssIndexEntry *index, ts_vector *previouslySeenArticles);
static bool WordIsWellFormed(const char *word);

static void NewsArticleClone(rssNewsArticle *article, const char *title, 
			     const char *server, const char *fullURL);
static int NewsArticleCompare(const void *elem1, const void *elem2);
static void NewsArticleFree(void *elem);

static int IndexEntryHash(const void *elem, int numBuckets);
static int IndexEntryCompare(const void *elem1, const void *elem2);
static void IndexEntryFree(void *elem);

static int ArticleIndexCompare(const void *elem1, const void *elem2);
static int ArticleFrequencyCompare(const void *elem1, const void *elem2);

// utils
static void DisposeRssDatabase(rssDatabase* db);
static void InitializeRssDatabase(rssDatabase* db);
static void PrintStopWord(char* elemAddr, void *auxData);

/**
 * Function: main
 * --------------
 * Serves as the entry point of the full RSS News Feed Aggregator.
 * 
 * @param argc the number of tokens making up the shell command invoking the
 *             application.  It should be either 1 or 2--2 when the used wants to
 *             specify what flat text file should be used to source all of the 
 *             RSS feeds.
 * @param argv the array of one of more tokens making up the command line invoking
 *             the application.  The 0th token is ignored, and the 1st one, if present,
 *             is taken to be the path identifying where the list of RSS feeds is.
 * @return always 0 if it main returns normally (although there might be exit(n) calls
 *         within the code base that end the program abnormally)
 */

static const char *const kWelcomeTextFile = "../assn-4-rss-news-search-data/welcome.txt";
static const char *const kDefaultStopWordsFile = "../assn-4-rss-news-search-data/stop-words.txt";
static const char *const kDefaultFeedsFile = "../assn-4-rss-news-search-data/rss-feeds-not9yo.txt";
int main(int argc, char **argv)
{
  const char *feedsFileName = (argc == 1) ? kDefaultFeedsFile : argv[1];
  rssDatabase db;
  InitializeRssDatabase(&db);
  
  // InitThreadPackage(false);
  Welcome(kWelcomeTextFile);
  LoadStopWords(&db.stopWords, kDefaultStopWordsFile);
  // TSHashSetMap(&(db.stopWords), PrintStopWord, NULL);
  
  BuildIndices(&db, feedsFileName);
  QueryIndices(&db);
  
  DisposeRssDatabase(&db);
  return 0;
}

// utility functions

static const int kNumStopWordsBuckets = 1009;
static const int kNumIndexEntryBuckets = 10007;
static void InitializeRssDatabase(rssDatabase* db)
{
  assert(db != NULL);
  TSHashSetNew(&db->stopWords, sizeof(char *), kNumStopWordsBuckets, StringHash, StringCompare, StringFree);
  TSHashSetNew(&db->indices, sizeof(rssIndexEntry), kNumIndexEntryBuckets, IndexEntryHash, IndexEntryCompare, IndexEntryFree);
  TSVectorNew(&db->previouslySeenArticles, sizeof(rssNewsArticle), NewsArticleFree, 0);
}

static void DisposeRssDatabase(rssDatabase* db)
{
  TSHashSetDispose(&db->indices);
  TSVectorDispose(&db->previouslySeenArticles); 
  TSHashSetDispose(&db->stopWords);
}

static void PrintStopWord(char* elemAddr, void *auxData)
{
  assert(auxData == NULL);
  printf("%s\n", * (char**) elemAddr);
}

// end utility functions

/** 
 * Function: Welcome
 * -----------------
 * Displays the contents of the specified file, which
 * holds the introductory remarks to be printed every time
 * the application launches.  This type of overhead may
 * seem silly, but by placing the text in an external file,
 * we can change the welcome text without forcing a recompilation and
 * build of the application.  It's as if welcomeTextFileName
 * is a configuration file that travels with the application.
 *
 * @param welcomeTextURL the URL of the document that should be pulled
 *                       and printed verbatim.
 *
 * No return value.
 */
 
static const char *const kNewLineDelimiters = "\r\n";
static void Welcome(const char *welcomeTextURL)
{
  FILE *fp;
  fp = fopen(welcomeTextURL, "r");
  assert(fp != NULL);
  
  streamtokenizer st;
  char buffer[4096];
  STNew(&st, fp, kNewLineDelimiters, true); // discarding delimiters
  while (STNextToken(&st, buffer, sizeof(buffer) ) ) {
    printf("%s\n", buffer);
  }  
  printf("\n");
  STDispose(&st);

  fclose(fp);
}

/**
 * Function: LoadStopWords
 * -----------------------
 * Initializes the raw ts_hashset addressed by stopWords to
 * store dynamically allocated C strings.
 * 
 * The stop words themselves are stored in the file named
 * by stopWordsTextFile, and is assumed to exist else the
 * program will just exit.  The file should store each
 * stop word on its own line without any whitespace other
 * than the delimiting newline characters.
 *
 * @param stopWords the (raw and uninitialized) ts_hashset to be initialized
 *                  and populated with a list of stop words.
 * @param stopWordsURL the path to the flat text file, where stop words
 *                     are listed one per line without any additional
 *                     white space.
 *
 * No return value.
 */

static void LoadStopWords(ts_hashset *stopWords, const char *stopWordsURL)
{
  FILE* fp;
  fp = fopen(stopWordsURL, "r");
  assert(fp != NULL);
  
  streamtokenizer st;
  // we can be sure that no stop word will be longer than 128, because of that
  // I don't even check if buffer containes finished word or not - 99.999% it will.
  char buffer[128];
  STNew(&st, fp, kNewLineDelimiters, true); // discarding delimiters
  while (STNextToken(&st, buffer, sizeof(buffer) ) )
  {
    char* str = (char*) malloc(strlen(buffer) + 1);
    assert(str != NULL);
    memcpy(str, buffer, strlen(buffer) + 1);
    TSHashSetEnter(stopWords, &str);
  }
   
  STDispose(&st);
  fclose(fp);
}

/**
 * Function: BuildIndices
 * ----------------------
 * As far as the user is concerned, BuildIndices needs to read each and every
 * one of the feeds listed in the specied feedsFileName, and for each feed parse
 * content of all referenced articles and store the content in the ts_hashset of indices.
 * Each line of the specified feeds file looks like this:
 *
 *   <feed name>: <URL of remote xml document>
 *
 * Each iteration of the supplied while loop parses and discards the feed name (it's
 * in the file for humans to read, but our aggregator doesn't care what the name is)
 * and then extracts the URL.  It then relies on ProcessFeed to pull the remote
 * document and index its content.
 *
 * @param db the rssDatabase holding on to the three large repositories of information.
 * @param feedsFileURL the full path leading to the flat text file storing up all of the
 *                     URLs of XML RSS feeds.
 * 
 * No return value.
 */

static void BuildIndices(rssDatabase *db, const char *feedsFileURL)
{
  FILE *fp;
  fp = fopen(feedsFileURL, "r");
  if(fp == NULL) 
  {
    printf("Feeds file path is not good: %s\nTry again.\n", feedsFileURL);
    exit(1);
  }
  
  streamtokenizer st;
  char remoteDocumentURL[2048];
    
  // Create connection factory
  limitedUrlFactory luf;
  LimitedURLConnectionFactoryNew(&luf, MAX_TOT_CONNS, MAX_SER_CONNS);
    
  STNew(&st, fp, kNewLineDelimiters, true);
   // ignore everything up to the first selicolon of the line
  while (STSkipUntil(&st, ":") != EOF) {
    // now ignore the semicolon and any whitespace directly after it
    STSkipOver(&st, ": ");
    // Assuming 2048 chars can hold that url, if not: url owners problem
    STNextToken(&st, remoteDocumentURL, sizeof(remoteDocumentURL));
    printf("%s\n", remoteDocumentURL);
    ProcessFeed(db, &luf, remoteDocumentURL);
  }
  
  printf("\n");
  STDispose(&st);
  
  // Dispose of connection factory
  LimitedURLConnectionFactoryDispose(&luf);
  
  fclose(fp);
}

/**
 * Function: ProcessFeed
 * ---------------------
 * ProcessFeed locates the specified RSS document, and if a (possibly redirected) connection to that remote
 * document can be established, then PullAllNewsItems is tapped to actually read the feed.  Check out the
 * documentation of the PullAllNewsItems function for more information, and inspect the documentation
 * for ParseArticle for information about what the different response codes mean.
 *
 * @param db the address of the rssDatabase housing the three respositories
 *           of information: the stop words set, the set of previously indexed articles,
 *           and the set of indices.
 * @remoteDocumentName the full URL of the document to be processed.
 *
 * No return value.
 */

static void ProcessFeed(rssDatabase *db, limitedUrlFactory* luf, const char *remoteDocumentName)
{
  url u;
  urlconnection urlconn;
  
  URLNewAbsolute(&u, remoteDocumentName);
  LimitedURLConnectionNew(luf, &urlconn, &u);

  char* redirectUrl = NULL;

  switch (urlconn.responseCode) {
      case 0: printf("Unable to connect to \"%s\".  Ignoring...", u.serverName);
	      break;
      case 200: PullAllNewsItems(db, &urlconn);
                break;
      case 301: 
      case 302: redirectUrl = strdup(urlconn.newUrl);
                break;
      default: printf("Connection to \"%s\" was established, but unable to retrieve \"%s\". [response code: %d, response message:\"%s\"]\n",
		      u.serverName, u.fileName, urlconn.responseCode, urlconn.responseMessage);
	       break;
  };
  
  LimitedURLConnectionDispose(luf, &urlconn);
  URLDispose(&u);
  
  /*
   * Force redirects to be sequential for now.
   * Another solution would be to have some kind of barrier to limit number of recursive calls.
   */
  if (redirectUrl != NULL) {
    ProcessFeed(db, luf, redirectUrl);
    free(redirectUrl);
  }
}

/**
 * Function: PullAllNewsItems
 * --------------------------
 * Steps though the data of what is assumed to be an RSS feed identifying the names and
 * URLs of online news articles.  Check out "assn-4-rss-news-search-data/sample-rss-feed.txt"
 * for an idea of what an RSS feed from the www.nytimes.com (or anything other server that 
 * syndicates is stories).
 *
 * PullAllNewsItems views a typical RSS feed as a sequence of "items", where each item is detailed
 * using a generalization of HTML called XML.  A typical XML fragment for a single news item will certainly
 * adhere to the format of the following example:
 *
 * <item>
 *   <title>At Installation Mass, New Pope Strikes a Tone of Openness</title>
 *   <link>http://www.nytimes.com/2005/04/24/international/worldspecial2/24cnd-pope.html</link>
 *   <description>The Mass, which drew 350,000 spectators, marked an important moment in the transformation of Benedict XVI.</description>
 *   <author>By IAN FISHER and LAURIE GOODSTEIN</author>
 *   <pubDate>Sun, 24 Apr 2005 00:00:00 EDT</pubDate>
 *   <guid isPermaLink="false">http://www.nytimes.com/2005/04/24/international/worldspecial2/24cnd-pope.html</guid>
 * </item>
 *
 * We're only interested in the title and link tags.
 *
 * @param db the address of the rssDatabase housing the three respositories
 *           of information: the stop words set, the set of previously indexed articles,
 *           and the set of indices.
 * @param urlconn the address of the urlconnection instance that manages the network
 *                connection to the class.
 */

static void PullAllNewsItems(rssDatabase *db, urlconnection *urlconn)
{
  rssFeedState state = {db}; // passed through the parser by address as auxiliary data.
  streamtokenizer st;
  char buffer[2048];

  XML_Parser rssFeedParser = XML_ParserCreate(NULL);
  XML_SetUserData(rssFeedParser, &state);
  XML_SetElementHandler(rssFeedParser, ProcessStartTag, ProcessEndTag);
  XML_SetCharacterDataHandler(rssFeedParser, ProcessTextData);

  STNew(&st, urlconn->dataStream, "\n", false);
  while (STNextToken(&st, buffer, sizeof(buffer))) {
    XML_Parse(rssFeedParser, buffer, strlen(buffer), false);
  }
  STDispose(&st);
  
  XML_Parse(rssFeedParser, "", 0, true); // instructs the xml parser that we're done parsing..
  XML_ParserFree(rssFeedParser);  
}

/**
 * This is the function that gets invoked by the XML_Parser every time
 * it parses a start tag.  We're only interested in <item>, <title>,
 * and <link> tags.  Every time we open an item tag, we want to clear the item
 * state of the user data to contain all empty strings (which is what the memset
 * does).  Every time we open a title, description, or link tag, we update the
 * activeField field of the userData struct to address the array that should
 * be written to by the CharacterDataHandler.  Note that we do absolutely nothing
 * 
 * @param userData the address of the client data original fed to the XML_SetUserData
 *                 call within PullAllNewsItems.  The struct is really of type
 *                 rssFeedState, which contains an rssFeedItem, which is the working
 *                 space used to build up all of the information needed to retrieve
 *                 a single news article.
 * @param name the start tag type, which could be "item", "title", description", "link"
 *             or any other null-terminated string naming a tag in the XML document
 *             being parsed.
 * @param atts an array-based map of attribute-value pairs.  We don't use this
 *             parameter here.
 *
 * No return type.
 */

static void ProcessStartTag(void *userData, const char *name, const char **atts)
{
  rssFeedState *state = userData;
  rssFeedEntry *entry = &state->entry;
  if (strcasecmp(name, "item") == 0) {
    memset(entry, 0, sizeof(rssFeedEntry));
  } else if (strcasecmp(name, "title") == 0) {
    entry->activeField = entry->title;
  } else if (strcasecmp(name, "link") == 0) {
    entry->activeField = entry->url;
  }
}

/**
 * This is the handler that's invoked whenever a close (explicit or implicit)
 * tag is detected.  For our purposes, we want to turn off the activeField
 * by setting it to NULL, which will be detected by the CharacterData handler
 * as an instruction to not bother writing text into the userData struct.
 * If we notice this is being invoked for an item close tag, then we take
 * the information which has been built up and stored in the userData struct
 * and use to parse and index another online news article.
 *
 * @param userData client data pointer that addresses an rssFeedState struct, which
 *                 houses our rssFeedItem struct, which is the place where we write
 *                 the title, description, and the url of the document we're looking
 *                 to parse.
 * @param name the end tag type, which could be "item", "title", "description", "link",
 *             or some other tag type we're not concerned with.
 *
 * No return type.
 */

static void ProcessEndTag(void *userData, const char *name)
{
  rssFeedState *state = userData;
  rssFeedEntry *entry = &state->entry;
  entry->activeField = NULL;
  if (strcasecmp(name, "item") == 0) 
    ParseArticle(state->db, entry->title, entry->url);
}

/**
 * Takes the character data address by text and appends it to any previously
 * pulled text stored in the buffer addressed by the activeField of the userData
 * struct.  The text paramter is NOT NULL-terminated, so we need to rely on
 * len to know exactly how many characters to copy.  The reason we call strncat
 * instead of strncpy: the stream of text making up the full description or 
 * title or link of an RSS news feed item might be broken up across two or more
 * call backs, so our implementation needs to be sensitive to that.
 *
 * @param userData the address of the rssFeedState struct where character data should
 *                 be copied.  Previous invocations of the ProcessStartTag handler
 *                 sets up the active field to point to the receiving buffer, but 
 *                 invocations of the ProcessEndTag handler sets that activeField field
 *                 to be NULL.  If the activeField field is non-NULL, when we know we're
 *                 appending character data to *some* character buffer.
 * @param text a pointer to a buffer of character data, which isn't NULL-terminated.
 * @param len the number of meaningful characters addressed by text.
 *
 * No return value.
 */

static void ProcessTextData(void *userData, const char *text, int len)
{
  rssFeedState *state = userData;
  rssFeedEntry *entry = &state->entry;
  if (entry->activeField == NULL) return; // no place to put data
  char buffer[len + 1];
  memcpy(buffer, text, len);
  buffer[len] = '\0';
  strncat(entry->activeField, buffer, 2048);
}

/** 
 * Function: ParseArticle
 * ----------------------
 * Attempts to establish a network connect to the news article identified by the three
 * parameters.  The network connection is either established of not.  The implementation
 * is prepared to handle a subset of possible (but by far the most common) scenarios,
 * and those scenarios are categorized by response code:
 *
 *    0 means that the server in the URL doesn't even exist or couldn't be contacted.
 *    200 means that the document exists and that a connection to that very document has
 *        been established.
 *    301 means that the document has moved to a new location
 *    302 also means that the document has moved to a new location
 *    4xx and 5xx (which are covered by the default case) means that either
 *        we didn't have access to the document (403), the document didn't exist (404),
 *        or that the server failed in some undocumented way (5xx).
 *
 * The are other response codes, but for the time being we're punting on them, since
 * no others appears all that often, and it'd be tedious to be fully exhaustive in our
 * enumeration of all possibilities.
 *
 * @param db the address of the rssDatabase surrounding the three primary repositories
 *           of information.
 * @param articleTitle the title of the article being parsed.
 * @param articleURL the URL of the article being parsed.
 *
 * No return value.
 */

static const char *const kTextDelimiters = " \t\n\r\b!@$%^*()_+={[}]|\\'\":;/?.>,<~`";
static void ParseArticle(rssDatabase *db, const char *articleTitle, const char *articleURL)
{
  url u;
  urlconnection urlconn;
  streamtokenizer st;
  int articleID;
  
  URLNewAbsolute(&u, articleURL);
  rssNewsArticle newsArticle = { articleTitle, u.serverName, u.fullName };
  if (TSVectorSearch(&db->previouslySeenArticles, &newsArticle, NewsArticleCompare, 0, false) >= 0) {
    printf("[Ignoring \"%s\": we've seen it before.]\n", articleTitle);
    URLDispose(&u); 
    return; 
  }
  
  URLConnectionNew(&urlconn, &u);
  switch (urlconn.responseCode) {
      case 0: printf("Unable to connect to \"%s\".  Domain name or IP address is nonexistent.\n", articleURL);
              break;
      case 200: printf("[%s] Indexing \"%s\"\n", u.serverName, articleTitle);
		NewsArticleClone(&newsArticle, articleTitle, u.serverName, u.fullName);
		TSVectorAppend(&db->previouslySeenArticles, &newsArticle);
		articleID = TSVectorLength(&db->previouslySeenArticles) - 1;
	        STNew(&st, urlconn.dataStream, kTextDelimiters, false);
		ScanArticle(&st, articleID, &db->indices, &db->stopWords);
		STDispose(&st);
		break;
      case 301: 
      case 302: // just pretend we have the redirected URL all along, though index using the new URL and not the old one...
	        ParseArticle(db, articleTitle, urlconn.newUrl);
		break;
      default: printf("Unable to pull \"%s\" from \"%s\". [Response code: %d] Punting...\n", articleTitle, u.serverName, urlconn.responseCode);
	       break;
  }
  
  URLConnectionDispose(&urlconn);
  URLDispose(&u);
}

/**
 * Function: ScanArticle
 * ---------------------
 * Pulls all of the content from the document via the addressed tokenizer.  Each word
 * that's deemed interesting enough to catalog is added to the specified set of indices.
 *
 * @param st the address of the streamtokenzer layering over the urlconnection to some online
 *           news article.
 * @param articleID the index of the relevant article within the ts_vector of previously parsed articles.
 * @param indices the set of indices to which all content in the article being parsed should be added.
 * @param stopWords the set of stop words.
 *
 * No return value.
 */

static void ScanArticle(streamtokenizer *st, int articleID, ts_hashset *indices, ts_hashset *stopWords)
{
  char word[1024];

  while (STNextToken(st, word, sizeof(word))) {
    if (strcasecmp(word, "<") == 0) {
      SkipIrrelevantContent(st);
    } else {
      RemoveEscapeCharacters(word);
      if (WordIsWorthIndexing(word, stopWords))
	AddWordToIndices(indices, word, articleID);
    }
  }
}

/**
 * Predicate Function: WordIsWorthIndexing
 * ---------------------------------------
 * Quick and dirty utility predicate that returns true
 * if and only if the specified text should be included from
 * the set of indices.  At the moment, we only allow
 * words that:
 *
 *   - are not stop words (all of which are held by the ts_hashset 
 *     addressed by stopWords)
 *   - contain just alphabetic characters and the hyphen character.
 *   - start with a letter of the alphabet
 *
 * @param the word of interest
 * @param stopWords the set of stop words
 * @return true if and only if the word is well formed and not part of the stop
 *         word set.
 */

static bool WordIsWorthIndexing(const char *word, ts_hashset *stopWords)
{
  return WordIsWellFormed(word) && TSHashSetLookup(stopWords, &word) == NULL;
}

/**
 * Adds the specified word (already deemed to be worth indexing)
 * to the set of indices, attaching it to the specified articleID (from
 * which the actual article can be easily recovered.)
 *
 * @param indices the set of indices being built.
 * @param word the word being added to the set of indices.
 * @param articleIndex the index of the relevant article where the word was found.
 *
 * No return value.
 */

static void AddWordToIndices(ts_hashset *indices, const char *word, int articleIndex)
{
  rssIndexEntry indexEntry = { word }; // partial intialization
  rssIndexEntry *existingIndexEntry = TSHashSetLookup(indices, &indexEntry);
  if (existingIndexEntry == NULL) {
    indexEntry.meaningfulWord = strdup(word);
    TSVectorNew(&indexEntry.relevantArticles, sizeof(rssRelevantArticleEntry), NULL, 0);
    TSHashSetEnter(indices, &indexEntry);
    existingIndexEntry = TSHashSetLookup(indices, &indexEntry); // pretend like it's been there all along
    assert(existingIndexEntry != NULL);
  }

  rssRelevantArticleEntry articleEntry = { articleIndex, 0 };
  int existingArticleIndex =
    TSVectorSearch(&existingIndexEntry->relevantArticles, &articleEntry, ArticleIndexCompare, 0, false);
  if (existingArticleIndex == -1) {
    TSVectorAppend(&existingIndexEntry->relevantArticles, &articleEntry);
    existingArticleIndex = TSVectorLength(&existingIndexEntry->relevantArticles) - 1;
  }
  
  rssRelevantArticleEntry *existingArticleEntry = 
    TSVectorNth(&existingIndexEntry->relevantArticles, existingArticleIndex);
  existingArticleEntry->freq++;
}

/** 
 * Function: QueryIndices
 * ----------------------
 * Standard query loop that allows the user to specify a single search term, and
 * then proceeds (via ProcessResponse) to list up to 10 articles (sorted by relevance)
 * that contain that word.
 *
 * @param db the address of the full RSS database, with access to the master
 *           set of stop words, scanned articles
 * 
 * No return value.
 */

static void QueryIndices(rssDatabase *db)
{
  char response[1024];
  while (true) {
    printf("Please enter a single query term that might be in our set of indices [enter to quit]: ");
    fgets(response, sizeof(response), stdin);
    response[strlen(response) - 1] = '\0';
    if (strcasecmp(response, "") == 0) break;
    ProcessResponse(db, response);
  }

}

/** 
 * Function: ProcessResponse
 * -------------------------
 * Placeholder implementation for what will become the search of a set of indices
 * for a list of web documents containing the specified word.
 *
 * @param db the address of the rssDatabase housing the stop word set, the set of indices,
 *           and the list of previously parsed articles.
 * @param word the word typed in by the user.
 *
 * No return value.
 */

static void ProcessResponse(rssDatabase *db, const char *word)
{
  if (!WordIsWellFormed(word)) {
    printf("That search term couldn't possibly be in our set of indices.\n\n");
    return;
  } 
  
  if (TSHashSetLookup(&db->stopWords, &word) != NULL) {
    printf("\"%s\" is too common a word to be taken seriously.  Please be more specific.\n\n", word);
    return;
  }

  rssIndexEntry entry = { word };
  rssIndexEntry *existingIndex = TSHashSetLookup(&db->indices, &entry);
  if (existingIndex == NULL) {
    printf("None of today's news articles contain the word \"%s\".\n\n", word);
    return;
  }

  ListTopArticles(existingIndex, &db->previouslySeenArticles);
}

/**
 * Brute force listing of the top ten articles (or all of them, if there are ten
 * or less of them) for the specified word.  
 *
 * @param matchingEntry the address of the rssIndexEntry housing the word of interest
 *                      and the full list of matching articles (with frequency counts).
 * @param previouslySeenAricles the list of all articles ever parsed.
 *
 * No return value.
 */

static void ListTopArticles(rssIndexEntry *matchingEntry, ts_vector *previouslySeenArticles)
{
  int i, numArticles, articleIndex, count;
  rssRelevantArticleEntry *relevantArticleEntry;
  rssNewsArticle *relevantArticle;
  
  numArticles = TSVectorLength(&matchingEntry->relevantArticles);
  printf("Nice! We found %d article%s that include%s the word \"%s\". ", 
	 numArticles, (numArticles == 1) ? "" : "s", (numArticles != 1) ? "" : "s", matchingEntry->meaningfulWord);
  if (numArticles > 10) { printf("[We'll just list 10 of them, though.]"); numArticles = 10; }
  printf("\n\n");
  
  TSVectorSort(&matchingEntry->relevantArticles, ArticleFrequencyCompare);
  for (i = 0; i < numArticles; i++) {
    relevantArticleEntry = TSVectorNth(&matchingEntry->relevantArticles, i);
    articleIndex = relevantArticleEntry->articleIndex;
    count = relevantArticleEntry->freq;
    relevantArticle = TSVectorNth(previouslySeenArticles, articleIndex);
    printf("\t%2d.) \"%s\" [search term occurs %d time%s]\n", i + 1, 
	   relevantArticle->title, count, (count == 1) ? "" : "s");
    printf("\t%2s   \"%s\"\n", "", relevantArticle->fullURL);
  }
  
  printf("\n");
}

/**
 * Predicate Function: WordIsWellFormed
 * ------------------------------------
 * Before we allow a word to be inserted into our map
 * of indices, we'd like to confirm that it's a good search term.
 * One could generalize this function to allow different criteria, but
 * this version hard codes the requirement that a word begin with 
 * a letter of the alphabet and that all letters are either letters, numbers,
 * or the '-' character.
 * 
 * @param word the C string we're examining.
 * @return true if and only if the word is something that, provided it's
 *         not a stop word, might be something we'd find in the set of indices.
 */

static bool WordIsWellFormed(const char *word)
{
  if (strlen(word) == 0) return true;
  if (!isalpha((int) word[0])) return false;
  for (int i = 1; i < strlen(word); i++)
    if (!isalnum((int) word[i]) && (word[i] != '-')) return false; 

  return true;
}

/**
 * Function: NewsArticleClone
 * --------------------------
 * Utility function that inserts deep copies of the three C strings and
 * embeds them in the three corresponding fields of the rssNewsArticle.
 * It assumes the contents of the rssNewsArticle can be safely overwritten
 * without orphaning memory.
 *
 * @param article the address of the rssNewsArticle to be populated
 * @param title the title string that should be planted into the clone.
 * @param server the server string that should be planted into the clone.
 * @param fullURL the url string that should be planted into the clone.
 *
 * No return value.  The effective return value is the rssNewsArticle
 * addressed by article.  The adopted this approach so we don't have to 
 * return (and copy) and entire struct by value.
 */

static void NewsArticleClone(rssNewsArticle *article, const char *title, 
			     const char *server, const char *fullURL)
{
  article->title = strdup(title);
  article->server = strdup(server);
  article->fullURL = strdup(fullURL);
}

/**
 * Function: NewsArticleCompare
 * ----------------------------
 * Compares the two rssNewsArticle records to see if they
 * each correspond to the same online news article.  Two
 * online news articles are the same (for our purposes, anyway)
 * if the title and server strings match, or if the two article
 * URLS match.  Note that the two const void *s store the locations
 * of the two rssNewsArticles being compared.
 *
 * @param elem1 the address of the first rssNewsArticle being compared.
 * @param elem2 the address of the second rssNewsArticle being compared.
 * @return a positive, negative, or zero value to signal that the first article
 *         is logically larger, smaller, or equal to the second.
 */

static int NewsArticleCompare(const void *elem1, const void *elem2)
{
  const rssNewsArticle *article1 = elem1;
  const rssNewsArticle *article2 = elem2;
  if ((StringCompare(&article1->title, &article2->title) == 0) &&
      (StringCompare(&article1->server, &article2->server) == 0)) return 0;
  
  return StringCompare(&article1->fullURL, &article2->fullURL);
}

/**
 * Function: NewsArticleFree
 * -------------------------
 * Diposes of the three dynamically allocated C strings
 * held by the rssNewsArticle addressed by elem.  We
 * rely on the services to the StringFree function
 * to the freeing.
 *
 * @param elem the address of the rssNewsArticle being disposed of.
 *
 * No return value.  Note that the rssNewsArticle addressed by elem is
 * invalidated after this function returns.
 */

static void NewsArticleFree(void *elem)
{
  rssNewsArticle *article = elem;
  StringFree(&article->title);
  StringFree(&article->server);
  StringFree(&article->fullURL);
}

/**
 * Function: IndexEntryHash
 * ------------------------
 * Hashes the rssIndexEntry addressed by elem by hashing
 * its meaningful word field in that same manner the application
 * hashses standalone C strings.  We call StringHash here, so that
 * one centralized function owns the string hashing functionality.
 *
 * @param elem the address of the rssIndexEntry being hashed.
 * @param numBuckets the number of buckets making up the ts_hashset that's
 *                   doing the hashing.
 * @return the hash code of the rssIndexEntry.
 */

static int IndexEntryHash(const void *elem, int numBuckets)
{
  const rssIndexEntry *entry = elem;
  return StringHash(&entry->meaningfulWord, numBuckets);
}

/**
 * Function: IndexEntryCompare
 * ---------------------------
 * Compares the two rssIndexEntry records addressed by elem1 and elem2.
 * Two rssIndexEntry records are the same if their meaningful word
 * fields are logically identical according to StringCompare.  The
 * string comparison functionality is managed by the StringCompare
 * function, and that's why we call it here.
 *
 * @param the address of the first of two rssIndexEntry records being compared.
 * @param the address of the second of two rssIndexEntry records being compared.
 * @return a positive, negative, or zero value to indicate that the first rssIndexEntry
 *         is logically larger than, smaller than, or equal to the second one.
 */

static int IndexEntryCompare(const void *elem1, const void *elem2)
{
  const rssIndexEntry *entry1 = elem1;
  const rssIndexEntry *entry2 = elem2;
  return StringCompare(&entry1->meaningfulWord, &entry2->meaningfulWord);
}

/**
 * Function: IndexEntryFree
 * ------------------------
 * Disposes of all resources held by the rssIndexEntry.
 * Self-explanatory implementation.
 */

static void IndexEntryFree(void *elem)
{
  rssIndexEntry *entry = elem;
  StringFree(&entry->meaningfulWord);
  TSVectorDispose(&entry->relevantArticles);
}

static int ArticleIndexCompare(const void *elem1, const void *elem2)
{
  const rssRelevantArticleEntry *entry1 = elem1;
  const rssRelevantArticleEntry *entry2 = elem2;
  return entry1->articleIndex - entry2->articleIndex;
}

static int ArticleFrequencyCompare(const void *elem1, const void *elem2)
{
  const rssRelevantArticleEntry *entry1 = elem1;
  const rssRelevantArticleEntry *entry2 = elem2;
  return entry2->freq - entry1->freq;
}
