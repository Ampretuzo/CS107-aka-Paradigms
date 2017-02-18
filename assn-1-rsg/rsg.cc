/**
 * File: rsg.cc
 * ------------
 * Provides the implementation of the full RSG application, which
 * relies on the services of the built-in string, ifstream, vector,
 * and map classes as well as the custom Production and Definition
 * classes provided with the assignment.
 */
 
#include <map>
#include <fstream>
#include <string>	// just in case..
#include <cassert>
#include "definition.h"
#include "production.h"
using namespace std;

// number of times we have to generate texts
#define VERSION_NUMBER 3
// starting non-terminal production is fixed
#define START_NON_TERM "<start>"

typedef map<string, Definition> grammar;

/**
 * Takes a reference to a legitimate infile (one that's been set up
 * to layer over a file) and populates the grammar map with the
 * collection of definitions that are spelled out in the referenced
 * file.  The function is written under the assumption that the
 * referenced data file is really a grammar file that's properly
 * formatted.  You may assume that all grammars are in fact properly
 * formatted.
 *
 * @param infile a valid reference to a flat text file storing the grammar.
 * @param grammar a reference to the STL map, which maps nonterminal strings
 *                to their definitions.
 */

static void readGrammar(ifstream& infile, map<string, Definition>& grammar)
{
  while (true) {
    string uselessText;
    getline(infile, uselessText, '{');
    if (infile.eof()) return;  // true? we encountered EOF before we saw a '{': no more productions!
    infile.putback('{');
    Definition def(infile);
    grammar[def.getNonterminal()] = def;
  }
}

/*
 * Easy - returns true if given string is terminal, that is, if it
 * does not start with '<'.
 */
bool isTerminal(string token)
{
  // TODO edge cases not handled
  assert(token.length() != 0);
  return token[0] != '<';
}

/*
 * Breaks program by assert function.
 * For example if given string is <dick> and no definition has that string as
 * non-terminal than program is stopped.
 */
void checkNonTerminalValidity(string nonTerm, grammar& grammar)
{
  grammar::const_iterator found = grammar.find(nonTerm);
  assert(found != grammar.end() );
}

/*
 * Depth first recursion.
 * Dives into first non-terminal occurance.
 */
void dfs(vector<string>& text, map<string, Definition>& grammar, string nonTerm)
{
  // take random production from grammar for given non-terminal
  const Production& prod = grammar[nonTerm].getRandomProduction();
  // go through its contents and dive in whenever non-terminal is encountered
  Production::const_iterator curr = prod.begin();
  while(curr != prod.end() )
  {
    // this is the token under consideration
    string token = *curr;
    if(isTerminal(token) )
    { // usual case, simple word that has to be added
      text.push_back(*curr);
    } else {
      // first check if encountered non-terminal is a valid one
      checkNonTerminalValidity(*curr, grammar);
      dfs(text, grammar, token);
    }
    curr++;
  }
  
  return;
}

/*
 * This function takes text vector and grammar by reference 
 * and puts non-terminals in it. Word by word. 
 * Effectively, this is recursion wrapper.
 */
void makeText(vector<string>& text, map<string, Definition>& grammar)
{
  // starting production is fixed to "<start>"
  string start(START_NON_TERM);
  
  // launch depth first recursion
  dfs(text, grammar, start);
}

/*
 * This function prints out given text vector contents hopefully in a smart way,
 * that is, it determines when to do new line etc...
 */
void printText(vector<string>& text)
{
  // TODO
  // this is util version
  vector<string>::const_iterator curr = text.begin();
  while(curr != text.end() )
  {
    cout << *curr << ' ';
    curr++;
  }
  cout << endl;
}

/**
 * Performs the rudimentary error checking needed to confirm that
 * the client provided a grammar file.  It then continues to
 * open the file, read the grammar into a map<string, Definition>,
 * and then print out the total number of Definitions that were read
 * in.  You're to update and decompose the main function to print
 * three randomly generated sentences, as illustrated by the sample
 * application.
 *
 * @param argc the number of tokens making up the command that invoked
 *             the RSG executable.  There must be at least two arguments,
 *             and only the first two are used.
 * @param argv the sequence of tokens making up the command, where each
 *             token is represented as a '\0'-terminated C string.
 */

int main(int argc, char *argv[])
{
  if (argc == 1) {
    cerr << "You need to specify the name of a grammar file." << endl;
    cerr << "Usage: rsg <path to grammar text file>" << endl;
    return 1; // non-zero return value means something bad happened 
  }
  
  ifstream grammarFile(argv[1]);
  if (grammarFile.fail()) {
    cerr << "Failed to open the file named \"" << argv[1] << "\".  Check to ensure the file exists. " << endl;
    return 2; // each bad thing has its own bad return value
  }
  
  // things are looking good...
  map<string, Definition> grammar;
  readGrammar(grammarFile, grammar);
  cout << "The grammar file called \"" << argv[1] << "\" contains "
       << grammar.size() << " definitions." << endl;
  
  
  
  
  // iteration counter
  int count = 0;
  
  do {
  
    // vector in which terminals will be collected
    vector<string> text;
    
    makeText(text, grammar);
    cout << "Version #" << (count + 1) << ":   ---------------------" << endl;
    printText(text);
    
    // clean text vector for further executions
    text.clear();
    
  } while(++count < VERSION_NUMBER);
  
  return 0;
}
