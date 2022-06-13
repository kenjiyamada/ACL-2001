#ifndef TREE_H
#define TREE_H

#include <vector>
#include <string>
#include <strstream>

// misc.h must include "typedef short Word"
#include "misc.h"

class Tokens {
  int c_ptr;
  string buff;
public:
  Tokens(string&);
  bool pastEOS();
  bool nextIsLPAR();
  bool nextIsRPAR();
  bool nextIsWord();
  bool nextIsSlash();
  bool nextIsWhitespace();
  void skip();
  void skipWhitespaces();
  Word getWord();
  void error(string);

};

class Tree {
  int nwords;
 public:
  Word symbol, head;
  vector<Tree *> child;

  Tree(string&);
  ~Tree();

  int numWords();
  void AddNodeList(vector<Tree*>& list);

private:
  Tree(Tokens&);

  void init();
  void setSymbolHead(Tokens&);
};


#endif // TREE_H
