
//
// obtain word co-occurence count in BXI files
//

// modified from co-count.cc to use ValidLen() and accept -mod and -idx

#include <string>
#include <vector>
#include <iostream>
#include <strstream>

#define DEBUG 0

typedef int Word;

int Mod = 20;
int Idx = 0;
bool ChineseMode = false;

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
  Word symbol,head;
  vector<Tree *> child;

  Tree(string&);
  ~Tree();
  int numWords();

private:
  Tree(Tokens&);

  void init();
  void setSymbolHead(Tokens&);

};


//
// Misc functions
//

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

//
// main functions
//

void
get_wordsE(Tree *node, vector<Word>& w)
{
  if (node->child.size()==0) w.push_back(node->head);
  else {
    for (int i=0; i<node->child.size(); i++)
      get_wordsE(node->child[i],w);
  }
}

void 
getValidFLen(int esize, int& minF, int& maxF)
{
  // due to hardware difference, +0.001 is needed

  if (ChineseMode) {
    minF = int(1.1 * esize - 4 + 0.001);
    maxF = int(1.7 * esize + 7 + 0.001);
  } else {
    minF = int(0.8 * esize - 2 + 0.001);
    maxF = int(1.4 * esize + 2 + 0.001);
  }
  if (minF<1) minF = 1;
}

void
count_cooccurrence(Tree *node, vector<Word>& sent)
{
#if 0
  // recursively find wordE and wordF cooccurence count and print

  int maxlenE = MaxLenE;
  int maxlenF = (MaxLenF > sent.size()) ? sent.size() : MaxLenF;
  int lenE = node->numWords();

  // unary node must be ignored.
  if (MinLenE <= lenE && lenE <= maxlenE && node->child.size() != 1) {
    vector<Word> words;
    get_wordsE(node,words);
    for (int lenF=MinLenF; lenF<=maxlenF; lenF++) {
      for (int posF=0; posF<sent.size()-(lenF-1); posF++) {
	//cout << node->numWords() << ' ' << lenF << "  ";
	for (int i=0; i<words.size(); i++) cout << words[i] << ' ';
	cout << ":";
	for (int i=0; i<lenF; i++) cout << ' ' << sent[posF+i];
	cout << "\n";
      }
    }
  }
#else
  // recursively emit E/F word pair if E matchs mod/idx, F must within validLen

  // unary node must be ignored
  if (node->child.size() != 1) {
    int lenE = node->numWords();
    vector<Word> words;
    get_wordsE(node,words);
    if (words[0] % Mod == Idx) {
      int minF,maxF;
      getValidFLen(lenE,minF,maxF);
      if (maxF > sent.size()) maxF = sent.size();
      for (int lenF=minF; lenF<=maxF; lenF++) {
	for (int posF=0; posF<sent.size()-(lenF-1); posF++) {
	  for (int i=0; i<words.size(); i++) cout << words[i] << ' ';
	  cout << ":";
	  for (int i=0; i<lenF; i++) cout << ' ' << sent[posF+i];
	  cout << "\n";
	}
      }
    }
  }
#endif

  // try for children
  for (int i=0; i<node->child.size(); i++)
    count_cooccurrence(node->child[i],sent);
}

void
process_pair(string& lineE, string& lineF)
{
  Tree *rootE;
  vector<Word> sentF;

#if DEBUG
  cout << "LineE:" << lineE << "\n";
  cout << "LineF:" << lineF << "\n";
#endif

  // set rootE;
  rootE = new Tree(lineE);

  // set sentF
  istrstream ist(lineF.c_str());
  string w;
  while(ist>>w) sentF.push_back(str2word(w));

  count_cooccurrence(rootE,sentF);
  delete rootE;
}

void
setParams(int argc, char* argv[])
{
  string arg;
  int i=0;
  while(++i<argc) {
    arg=argv[i];
    if (arg=="-mod") Mod = atoi(argv[++i]);
    else if (arg=="-idx") Idx = atoi(argv[++i]);
    else if (arg=="-C") ChineseMode=true;
    else {
      cerr << "Unrecognized option: " << arg << "\n";
      exit(1);
    }
  }
}  

main(int argc, char* argv[])
{
  setParams(argc,argv);

  int mod,numlines=0;
  string line,lineE,lineF;
  while(getline(cin,line)) {
    mod = numlines++ % 3;
    if (mod==0) lineE=line;
    else if (mod==1) {
      lineF=line;
      process_pair(lineE,lineF);
    }
  }
  cerr << "NumLines: " << numlines << "\n";
  exit(0);
}

      
