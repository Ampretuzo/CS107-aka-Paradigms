#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <expat.h>

#include "url.h"
#include "bool.h"
#include "urlconnection.h"
#include "streamtokenizer.h"
#include "html-utils.h"
#include "vector.h"
#include "hashset.h"

typedef struct {
  char title[2048];
  size_t titleSize; // Size information has to be passed, otherwise 
                    // array will decay into pointer.
  char description[2048];
  size_t descriptionSize;
  char url[2048];
  size_t urlSize;
  char *activeField;
  size_t activeFieldSize;
  // For expat callback:
  hashset* stop;
  hashset* idx;
  vector* indexedArticles;
} rssFeedItem;

static void GetStopWords(hashset* stop);
static void Welcome(const char *welcomeTextURL);
static void BuildIndices(const char *feedsFileURL, hashset* stop, hashset* idx, vector* indexedArticles);
static void ProcessFeed(const char *remoteDocumentURL, hashset* stop, hashset* idx, vector* indexedArticles);
static void PullAllNewsItems(urlconnection *urlconn, hashset* stop, hashset* idx, vector* indexedArticles);
static void ProcessStartTag(void *userData, const char *name, const char **atts);
static void ProcessEndTag(void *userData, const char *name);
static void ProcessTextData(void *userData, const char *text, int len);
static void ParseArticle(char *articleTitle, const char *articleURL, hashset* stop, hashset* idx, vector* indexedArticles);
static void ScanArticle(streamtokenizer *st, const char *articleTitle, const char *articleURL, hashset* stop, hashset* idx);
static void indexWord(char* word, size_t wordSize, const char* articleTitle, const char* articleURL, hashset* stop, hashset* idx);
static void QueryIndices(hashset* stop, hashset* idx);
static void ProcessResponse(const char *word, hashset* stop, hashset* idx);
static bool WordIsWellFormed(const char *word);








/* functions for stop words hashset */

/** 
 * Function: StringHash                     
 * --------------------
 * Taken from readme. Adapted to specific requirements.
 */  

static const signed long kHashMultiplier = -1664117991L;
static int StringHash(const void* p, int numBuckets)  
{
  char* str = * (char**) p; // deref pointer of pointer
  
  unsigned long hashcode = 0;
  
  for (int i = 0; i < strlen(str); i++)  
    hashcode = hashcode * kHashMultiplier + tolower(str[i]);  
  
  return hashcode % numBuckets;                                
}

/**
 * Function: StringCompare
 * -----------------------
 * This is basically a strcmp wrapper.
 */
static int StringCompare(const void * p1, const void * p2)
{
  char* s1 = * (char**) p1;
  char* s2 = * (char**) p2;
  
  return strcmp(s1, s2);
}

/** 
 * Function: StringPrint
 * ---------------------
 * Lifted from previous assignment for convenience.
 * Is a wrapper for print.
 * The target FILE * should
 * be passed in via the auxData parameter.
 */

static void StringPrint(void* p, void* auxData)
{
  char* s = * (char**) p;
  FILE* fp = (FILE*) auxData;
  fprintf(fp, "%s\n", s);
}

/**
 * Function: StringDispose
 * -----------------------
 * This is a wrapper for free.
 */

static void StringDispose(void* p)
{
  char* s = * (char**) p;
  free(s);
}

/* end functions for stop words hashset */













/* functions for index structure */

/*
 * Here's the basic idea.
 * We need to map words to vector of articles sorted by 
 * number of word appearences.
 * To achieve that, we need a struct which will carry a word string and  
 * a vector, storing articles and number of appearances of that word in 
 * those articles.
 * We will store those structs inside a hashset.
 * Hash function and comparison will be inherited from word string.
 * When user enters a word, we can just look up corresponding entry in hashset
 * and display its vector of articles.
 */

/**
 * Struct that keeps word and vector of artices.
 * Ultimately we will create a hashset of these, using string functions to
 * hash and compare.
 */

typedef struct {
  char* word; // the word 
  vector articles; // vector that will hold articleAppearances
} word_and_articles;

// word_and_articles functions will basically wrap string functions.
static int w_and_aHash(const void* p, int numBuckets)
{
  word_and_articles* wa = (word_and_articles*) p;
  return StringHash( &(wa->word), numBuckets);
}

static int w_and_aCompare(const void * p1, const void * p2)
{
  word_and_articles* wa1 = (word_and_articles*) p1;
  word_and_articles* wa2 = (word_and_articles*) p2;
  
  return StringCompare( &(wa1->word), &(wa2->word) );
}

static void w_and_aDispose(void* p)
{
  word_and_articles* wa = (word_and_articles*) p;
  free(wa->word);
  VectorDispose(&(wa->articles) );
}

typedef struct {
  url url;  // We could have stored char*, but this is more convenient later when 
            // we'll need to get server name.
  char* title;  // Article title
} article;

static void ArticleDispose(void* p)
{
  article* a = (article*) p;
  free(a->title);
  URLDispose(&(a->url) );
}

// Note that this function supports only linear search for vector.
static int ArticleCompare(const void * p1, const void * p2)
{
  article* a1 = (article*) p1;
  article* a2 = (article*) p2;
  // Two articles are equal when their urls are the same or when
  // theyr titles and servers are the same.
  bool sameUrl = strcmp(a1->url.fullName, a2->url.fullName) == 0;
  bool sameProvider = (
    strcmp(a1->title, a2->title) == 0 
    && 
    strcmp(a1->url.serverName, a2->url.serverName) == 0
  );
  if(sameUrl || sameProvider) return 0;
  return 1;
}

typedef struct {
  article  article;
  int cnt;
} wcnt; // Word Count in Article

// TODO these structures need functions like comparators and destructors.
// Add when needed.



/* end functions for index structure */















/* simple helper functions */

static const int numBuckets = 1009;

static void InitializeStructures(hashset* stop, hashset* idx, vector* indexedArticles)
{
  HashSetNew(stop, sizeof(char*), numBuckets, StringHash, StringCompare, StringDispose);
  HashSetNew(idx, sizeof(word_and_articles), numBuckets /* note that underlying key is string */,
    w_and_aHash, w_and_aCompare, w_and_aDispose);
  VectorNew(indexedArticles, sizeof(article), ArticleDispose, 0);
}

static void DisposeStructures(hashset* stop, hashset* idx, vector* indexedArticles)
{
  HashSetDispose(stop);
  HashSetDispose(idx);
  VectorDispose(indexedArticles);
}

/* end simple helper functions */















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

static const char *const kWelcomeTextPath = // "http://cs107.stanford.edu/rss-news/welcome.txt";
  // Above 9yo web adress is obsolete.
  // Instead, working with file paths.
  // Data folder is in the the same directory as source files:
  "../assn-4-rss-news-search-data/welcome.txt";
static const char *const kDefaultStopWordsURL = "http://cs107.stanford.edu/rss-news/stop-words.txt";
static const char *const kDefaultFeedsFilePath = // "http://cs107.stanford.edu/rss-news/rss-feeds.txt";
  // Likewise, using local file:
  "../assn-4-rss-news-search-data/rss-feeds-not9yo.txt";
  
int main(int argc, char **argv)
{
  const char *feedsFilePath = (argc == 1) ? kDefaultFeedsFilePath : argv[1];
  
  hashset stop; // this hashset will contain... well, stop words.
  hashset idx;  // index
  vector indexedArticles;
  
  InitializeStructures(&stop, &idx, &indexedArticles);
  
  Welcome(kWelcomeTextPath);
  GetStopWords(&stop);
  BuildIndices(feedsFilePath, &stop, &idx, &indexedArticles);
  QueryIndices(&stop, &idx);
  
  DisposeStructures(&stop, &idx, &indexedArticles);

  return 0;
}

/**
 * Function: GetStopWords
 * ----------------------
 * Collects stop words from file into given hashset
 *
 * @param stop hashset truct pointer where you want to collect the stop words.
 */
 
static const char* const stopFilename = "../assn-4-rss-news-search-data/stop-words.txt";
static const char *const kNewLineDelimiters = "\r\n";
static void GetStopWords(hashset* stop)
{
  FILE* fp;
  fp = fopen(stopFilename, "r");
  assert(fp != NULL);
  
  // use streamtokenizer for fun
  streamtokenizer st;
  char buffer[128];
  STNew(&st, fp, kNewLineDelimiters, true); // discarding delimiters
  while (STNextToken(&st, buffer, sizeof(buffer) ) )
  {
    char* str = (char*) malloc(strlen(buffer) + 1);
    assert(str != NULL);
    memcpy(str, buffer, strlen(buffer) + 1);
    HashSetEnter(stop, &str);
  }
   
  STDispose(&st);

  // close FILE to flush buffer
  fclose(fp);
}

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
 * No return value to speak of.
 */
 
static void Welcome(const char *welcomeTextPath)
{
/* Removed the code handling web address to welcome.txt*/
/* Instead, using filesystem path: */
  // https://www.cs.bu.edu/teaching/c/file-io/intro/
  FILE *fp;
  fp = fopen(welcomeTextPath, "r");
  assert(fp != NULL); // welcome file not found
  
  streamtokenizer st;
  char buffer[4096];
  STNew(&st, fp, kNewLineDelimiters, true); // discarding delimiters
  while (STNextToken(&st, buffer, sizeof(buffer) ) ) {
    printf("%s\n", buffer);
  }  
  printf("\n");
  STDispose(&st); // remember that STDispose doesn't close the file, since STNew doesn't open one.. 

  // close FILE to flush buffer
  fclose(fp);
}

/**
 * Function: BuildIndices
 * ----------------------
 * As far as the user is concerned, BuildIndices needs to read each and every
 * one of the feeds listed in the specied feedsFileName, and for each feed parse
 * content of all referenced articles and store the content in the hashset of indices.
 * Each line of the specified feeds file looks like this:
 *
 *   <feed name>: <URL of remote xml document>
 *
 * Each iteration of the supplied while loop parses and discards the feed name (it's
 * in the file for humans to read, but our aggregator doesn't care what the name is)
 * and then extracts the URL.  It then relies on ProcessFeed to pull the remote
 * document and index its content.
 *
 * NOTE: logically, stop hashset could be consted in this function, but for 
 * some reason HashSetLookup function has not const contract. Other containers
 * are not constant for obvious reasons.
 *
 * @param feedsFileURL the full path leading to the flat text file storing up all of the
 *                     URLs of XML RSS feeds.
 */

static const int kNumIndexEntryBuckets = 10007;
static void BuildIndices(const char *feedsFilePath, hashset* stop, hashset* idx, vector* indexedArticles)
{
  FILE *fp;
  fp = fopen(feedsFilePath, "r");
  if(fp == NULL) 
  {
    printf("Feeds file path is not good: %s\nTry again.\n", feedsFilePath);
    exit(1);
  }
  
  streamtokenizer st;
  char remoteDocumentURL[2048];
    
  STNew(&st, fp, kNewLineDelimiters, true);
  while (STSkipUntil(&st, ":") != EOF) { // ignore everything up to the first selicolon of the line
    STSkipOver(&st, ": ");		   // now ignore the semicolon and any whitespace directly after it
    STNextToken(&st, remoteDocumentURL, sizeof(remoteDocumentURL));
    // Assuming 2048 chars can hold that url, if not: url owners problem
    ProcessFeed(remoteDocumentURL, stop, idx, indexedArticles);
  }
  
  printf("\n");
  STDispose(&st);
  
  fclose(fp);
}

/**
 * Function: ProcessFeed
 * ---------------------
 * ProcessFeed locates the specified RSS document, and if a (possibly redirected) connection to that remote
 * document can be established, then PullAllNewsItems is tapped to actually read the feed.  Check out the
 * documentation of the PullAllNewsItems function for more information, and inspect the documentation
 * for ParseArticle for information about what the different response codes mean.
 */

static void ProcessFeed(const char *remoteDocumentURL, hashset* stop, hashset* idx, vector* indexedArticles)
{
  url u;
  urlconnection urlconn;
  
  URLNewAbsolute(&u, remoteDocumentURL);
  URLConnectionNew(&urlconn, &u);

  switch (urlconn.responseCode) {
      case 0: printf("Unable to connect to \"%s\".  Ignoring...\n", u.serverName);
        break;
      case 200: PullAllNewsItems(&urlconn, stop, idx, indexedArticles);
        break;
      case 301: 
      case 302: ProcessFeed(urlconn.newUrl, stop, idx, indexedArticles);
        break;
      default: printf("Connection to \"%s\" was established, but unable to retrieve \"%s\". [response code: %d, response message:\"%s\"]\n",
		    u.serverName, u.fileName, urlconn.responseCode, urlconn.responseMessage);
	      break;
  };
  
  URLConnectionDispose(&urlconn);
  URLDispose(&u);
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
 */

static void PullAllNewsItems(urlconnection *urlconn, hashset* stop, hashset* idx, vector* indexedArticles)
{
  rssFeedItem item;
  item.titleSize = sizeof(item.title);
  item.urlSize = sizeof(item.url);
  item.descriptionSize = sizeof(item.description);
  item.activeFieldSize = sizeof(item.activeField);
  item.activeField = NULL;
  // To pass:
  item.stop = stop;
  item.idx = idx;
  item.indexedArticles = indexedArticles;
  
  streamtokenizer st;
  char buffer[2048];

  XML_Parser rssFeedParser = XML_ParserCreate(NULL);
  XML_SetUserData(rssFeedParser, &item);
  XML_SetElementHandler(rssFeedParser, ProcessStartTag, ProcessEndTag);
  XML_SetCharacterDataHandler(rssFeedParser, ProcessTextData);

  STNew(&st, urlconn->dataStream, "\n", false);
  // Assuming xml file does not go more than col 2048 deep.
  while (STNextToken(&st, buffer, sizeof(buffer))) {
    XML_Parse(rssFeedParser, buffer, strlen(buffer), false);
  }
  STDispose(&st);
  
  XML_Parse(rssFeedParser, "", 0, true);
  XML_ParserFree(rssFeedParser);  
}

/**
 * This is the function that gets invoked by the XML_Parser every time
 * it parses a start tag.  We're only interested in <item>, <title>, <description>,
 * and <link> tags.  Every time we open an item tag, we want to clear the item
 * state of the user data to contain all empty strings (which is what the memset
 * does).  Every time we open a title, description, or link tag, we update the
 * activeField field of the userData struct to address the array that should
 * be written to by the CharacterDataHandler. 
 * 
 * @param userData the address of the client data original fed to the XML_SetUserData
 *                 call within PullAllNewsItems.
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
  rssFeedItem *item = userData;
  if (strcasecmp(name, "item") == 0) {
    /*
     * After extensive GDB, I found out that this was the source of Segfault.
     * Pointers to containers would be zeroed out, which obviously causes
     * bad things in ParseArticle.
     * We have to be more careful when doing this.
     */
    memset(item->title, 0, item->titleSize );
    memset(item->description, 0, item->descriptionSize );
    memset(item->url, 0, item->urlSize );
    memset(item->activeField, 0, item->activeFieldSize );
    printf("sizes:\n%d, %d, %d, %d\n", item->titleSize, item->descriptionSize, item->urlSize, item->activeFieldSize);
  } else if (strcasecmp(name, "title") == 0) {
    item->activeField = item->title;
  } else if (strcasecmp(name, "description") == 0) {
    item->activeField = item->description;
  } else if (strcasecmp(name, "link") == 0) {
    item->activeField = item->url;
  } else item->activeField = NULL;
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
 * @param userData client data pointer that addresses our rssFeedItem struct.
 * @param name the end tag type, which could be "item", "title", "description", "link",
 *             or some other tag type we're not concerned with.
 */

static void ProcessEndTag(void *userData, const char *name)
{
  rssFeedItem *item = userData;
  item->activeField = NULL;
  if (strcasecmp(name, "item") == 0)
  {
    ParseArticle(item->title, item->url, item->stop, item->idx, item->indexedArticles);  
  }
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
 * @param userData the address of the rssFeedItem struct where character data should
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
  rssFeedItem *item = userData;
  if (item->activeField == NULL) return; // no place to put data
  char buffer[len + 1];
  memcpy(buffer, text, len);
  buffer[len] = '\0';
  strncat(item->activeField, buffer, 2048);
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
 */

static const char *const kTextDelimiters = " \t\n\r\b!@$%^*()_+={[}]|\\'\":;/?.>,<~`";
static void ParseArticle(char *articleTitle, const char *articleURL, hashset* stop, hashset* idx, vector* indexedArticles)
{
  url u;
  urlconnection urlconn;
  streamtokenizer st;
  
  URLNewAbsolute(&u, articleURL);
  // At this point we can decide if this article is being indexed twice:
  article a;
  a.url = u;
  char* title = (char*) malloc(strlen(articleTitle) + 1);
  assert(title != NULL);
  a.title = title;
  memcpy(a.title, articleTitle, strlen(articleTitle) + 1);
  if(VectorSearch(indexedArticles, &a, ArticleCompare, 0, false) != -1)
  {
    // Don't leak!!!
    ArticleDispose(&a);
    return;
  }
  // If still here, article was not found.
  // In that case, we can store it in indexedArticles.
  // Also, we don't need to pass indexedArticles any more.
  VectorAppend(indexedArticles, &a);
  
  URLConnectionNew(&urlconn, &u);

  switch (urlconn.responseCode) {
      case 0: printf("Unable to connect to \"%s\".  Domain name or IP address is nonexistent.\n", articleURL);
          break;
      case 200: printf("[%s] Indexing \"%s\"\n", u.serverName, articleTitle);
	        STNew(&st, urlconn.dataStream, kTextDelimiters, false);
          ScanArticle(&st, articleTitle, articleURL, stop, idx);
		      STDispose(&st);
	        break;
      case 301: 
      case 302: // just pretend we have the redirected URL all along, though index using the new URL and not the old one...
	        ParseArticle(articleTitle, urlconn.newUrl, stop, idx, indexedArticles);
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
 * Parses the specified article, skipping over all HTML tags, and counts the numbers
 * of well-formed words that could potentially serve as keys in the set of indices.
 * Once the full article has been scanned, the number of well-formed words is
 * printed, and the longest well-formed word we encountered along the way
 * is printed as well.
 *
 * This is really a placeholder implementation for what will ultimately be
 * code that indexes the specified content.
 */

static void ScanArticle(streamtokenizer *st, const char *articleTitle, const char *articleURL, hashset* stop, hashset* idx)
{
  char word[1024];

  while (STNextToken(st, word, sizeof(word))) 
  {
    if (strcasecmp(word, "<") == 0) {
      SkipIrrelevantContent(st); // in html-utls.h
    } else {
      RemoveEscapeCharacters(word); // in html-utils.h
      if (WordIsWellFormed(word)) {
        indexWord(word, sizeof(word), articleTitle, articleURL, stop, idx);
      }
    }
  }

/*  printf("\tWe counted %d well-formed words [including duplicates].\n", numWords);*/
/*  printf("\tThe longest word scanned was \"%s\".", longestWord);*/
/*  if (strlen(longestWord) >= 15 && (strchr(longestWord, '-') == NULL)) */
/*    printf(" [Ooooo... long word!]");*/
/*  printf("\n");*/
}

/**
 * Function: indexWord
 * -------------------
 * Update index for word.
 * Ideally, word might be incomplete char array buffer, in which case we 
 * should wait successive calls to finish started text.
 * But in reality it is good enough to just take buffer as as complete word
 * and index it that way, especially when wordSize is as large as 1024, which
 * it is.
 */

static void indexWord(char* word, size_t wordSize, const char* articleTitle, const char* articleURL, hashset* stop, hashset* idx)
{
  // TODO
}

/** 
 * Function: QueryIndices
 * ----------------------
 * Standard query loop that allows the user to specify a single search term, and
 * then proceeds (via ProcessResponse) to list up to 10 articles (sorted by relevance)
 * that contain that word.
 */
static void QueryIndices(hashset* stop, hashset* idx)
{
  char response[1024];
  while (true) {
    printf("Please enter a single query term that might be in our set of indices [enter to quit]: ");
    fgets(response, sizeof(response), stdin);
    response[strlen(response) - 1] = '\0';
    if (strcasecmp(response, "") == 0) break;
    ProcessResponse(response, stop, idx);
  }
}

/** 
 * Function: ProcessResponse
 * -------------------------
 * Placeholder implementation for what will become the search of a set of indices
 * for a list of web documents containing the specified word.
 */

static void ProcessResponse(const char *word, hashset* stop, hashset* idx)
{
  if (WordIsWellFormed(word)) {
    // First of all, take care of high entropy cases
    if(HashSetLookup(stop, &word) != NULL)
    {
      printf("The word \"%s\" is too common. Try something else.\n", word);
      return;
    }
    printf("\tWell, we don't have the database mapping words to online news articles yet, but if we DID have\n");
    printf("\tour hashset of indices, we'd list all of the articles containing \"%s\".\n", word);
  } else {
    printf("\tWe won't be allowing words like \"%s\" into our set of indices.\n", word);
  }
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
 */

static bool WordIsWellFormed(const char *word)
{
  int i;
  if (strlen(word) == 0) return true;
  if (!isalpha((int) word[0])) return false;
  for (i = 1; i < strlen(word); i++)
    if (!isalnum((int) word[i]) && (word[i] != '-')) return false; 

  return true;
}
