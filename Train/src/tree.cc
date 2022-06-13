#include "tree.h"

//
// Misc functions
//

#if 0
// str2word() is defined in misc.cc
Word
str2word(string& str)
{
  // convert decimal string to an integer

  Word ret = 0;
  for (int i=0; i<str.size(); i++) {
    ret = ret*10 + (str[i] - '0');
  }
  return ret;
}
#endif

//
// Tokens
//

Tokens::Tokens(string& str)
{
  // initialize internal data

  buff=str;
  c_ptr=0;
}

bool
Tokens::pastEOS()
{
  return (c_ptr >= buff.size());
}

bool
Tokens::nextIsLPAR()
{
  if (pastEOS()) return false;
  return (buff[c_ptr] == '(');
}

bool
Tokens::nextIsRPAR()
{
  if (pastEOS()) return false;
  return (buff[c_ptr] == ')');
}

bool
Tokens::nextIsWord()
{
  if (pastEOS()) return false;
  char c = buff[c_ptr];
  return ('0' <= c && c <= '9');
}

bool
Tokens::nextIsSlash()
{
  if (pastEOS()) return false;
  return (buff[c_ptr] == '/');
}

bool
Tokens::nextIsWhitespace()
{
  if (pastEOS()) return false;
  char c = buff[c_ptr];
  return (c==' ' || c=='\t' || c=='\n');
}

void
Tokens::skip()
{
  if (! (nextIsLPAR() || nextIsRPAR() || nextIsSlash())) error("cannot skip");
  c_ptr++;
  skipWhitespaces();
}

void
Tokens::skipWhitespaces()
{
  while (nextIsWhitespace()) c_ptr++;
}

Word
Tokens::getWord()
{
  Word ret=0;
  int len=0;
  char c = buff[c_ptr];
  while('0'<=c && c<='9') {
    ret = ret*10 + (c - '0');
    len++; c_ptr++;
    c=buff[c_ptr];
  }
  if (len==0) error("getWord() returns NULL");
  skipWhitespaces();
  return ret;
}

void
Tokens::error(string msg)
{
  cerr << msg << '\n';
  exit(1);
}

//
// Tree
//

Tree::Tree(string& line)
{
  // public constructor
  init();
  Tokens tok(line);
  
  // set root node, and child nodes...

  if (!tok.nextIsLPAR()) {
    // the line does not start with LPAR
    if (tok.nextIsWord()) {
      // assume single word sentence
      setSymbolHead(tok);
    } else {
      tok.error("unrecognized tree format");
    }
  } else {
    // read line using another constructor Tree(Tokens&)
    Tree tree(tok);
    *this = tree;		// copy all
    tree.child.clear();		// prevent destructor from deleting children
  }
}

Tree::Tree(Tokens& tok)
{
  // private constructor
  init();
  if (!tok.nextIsLPAR()) {
    // leaf node
    setSymbolHead(tok);
  } else {
    // nonterminal node
    tok.skip();			// skip LPAR
    setSymbolHead(tok);
    while (!tok.nextIsRPAR()) {
      Tree *c = new Tree(tok);
      child.push_back(c);
    }
    tok.skip();			// skip RPAR
  }
}

Tree::~Tree()
{
  for (int i=0; i<child.size(); i++)
    delete child[i];
}

void
Tree::init()
{
  // initialize members
  symbol=0;
  head=0;
  nwords=0;
}

void
Tree::setSymbolHead(Tokens& tok)
{
  if (!tok.nextIsWord()) tok.error("word expected");

  // label == sym OR head/sym
  symbol = tok.getWord();
  if (tok.nextIsSlash()) {
    head = symbol;
    tok.skip();
    if (!tok.nextIsWord()) tok.error("head/sym expected");
    symbol = tok.getWord();
  }
}

int
Tree::numWords()
{
  // returns number of leaf words under this node
  if (nwords==0) {
    // cache is not available yet.
    if (child.size()==0) nwords=1;
    else {
      for (int i=0; i<child.size(); i++)
	nwords += child[i]->numWords();
    }
  }
  return nwords;
}

void
Tree::AddNodeList(vector<Tree *>& list)
{
  // add nodes into the list

  // push this
  list.push_back(this);

  // push children
  for (int i=0; i<child.size(); i++) 
    child[i]->AddNodeList(list);
  
}

