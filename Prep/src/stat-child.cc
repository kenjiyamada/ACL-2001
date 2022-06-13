
//
// obtain CFG rule statistics 
//

// This code came from TTable/co-count.cc:1.5

#include <string>
#include <vector>
#include <iostream>
#include <strstream>
#include <map>

#define DEBUG 0

typedef int Word;
const int UNK = 1;

void
error_exit(string msg)
{
  cerr << msg << "\n";
  exit(1);
}

void
sout(string &str, Word w)
{
  // append w to str
  int rem = w % 10;
  if (w>=10) sout(str,(w-rem)/10);
  str += ('0' + rem);
}

// params
bool FuncSTAT = false;
bool FuncLIST = false;
bool FuncFILTER = false;
int MaxC = 99;
int MaxID = 0;
int MaxF = 0;

// global tables
int NumLines = 0;
map<int,int> NumMaxC;
map<string,int> NumRules;
map<int,int> NumC;
typedef map<int,int>::const_iterator ItrMaxC;
typedef map<string,int>::const_iterator ItrRules;
typedef map<int,int>::const_iterator ItrNumC;

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
  int getMaxChildren();
  void limitID(int maxid);
  void getLine(string& line);

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
Tree::getMaxChildren()
{
  int numc = child.size();

  if (numc==0) return 1;
  else {
    int cmax, max = numc;
    for (int i=0; i<numc; i++) {
      cmax = child[i]->getMaxChildren();
      if (cmax>max) max=cmax;
    }
    return max;
  }
}

void
Tree::limitID(int maxid)
{
  if (head > maxid) head = UNK;
  for (int i=0; i<child.size(); i++)
    child[i]->limitID(maxid);
}

void
Tree::getLine(string& line)
{
  if (child.size()>0) {
    // non-terminal
    line += "(";

    // node label (sym OR head/sym)
    if (head) {
      sout(line,head); line += "/";
    }
    sout(line,symbol);

    // child nodes
    for (int i=0; i<child.size(); i++) {
      line += " ";
      child[i]->getLine(line);
    }
    line += ")";

  } else {
    // leaf-node (label = head/sym)
    if (head==0) error_exit("head==0 in terminal node");
    sout(line,head);
    line += "/";
    sout(line,symbol);
  }
}

//
// main functions
//

void
process_node(Tree *node)
{
  int cnum = node->child.size();

  // put CFG info into NumRules
  if (cnum>0) {
    string clist;
    sout(clist,node->symbol); clist += ' ';
    for (int i=0; i<cnum; i++) {
      clist += ' '; sout(clist, node->child[i]->symbol);
    }
    
    // increment NumC[cnum] if this is a new rule
    if (NumRules[clist]++ == 0)
      NumC[cnum]++;
  }    

  // recursively call for all children
  for (int i=0; i<cnum; i++)
    process_node(node->child[i]);
}

void
limit_id(string& lineF, int maxID, int &flen)
{
  string line0 = lineF;
  istrstream ist(line0.c_str());

  lineF = "";
  string w,sep="";
  flen=0;
  while(ist>>w) {
    flen++;
    int id = str2word(w);
    if (id > maxID) id = UNK;
    lineF += sep; sep = " ";
    sout(lineF,id);
  }
}

void
show_results()
{
  // show summary
  if (FuncSTAT) {
    cout << "NumLines: " << NumLines << "\n\n";

    // show # of sents sorted by MaxC
    for (ItrMaxC p=NumMaxC.begin(); p!=NumMaxC.end(); p++)
      cout << "MaxC=" << p->first << "  " << p->second << "\n";
    cout << "\n";
    
    // show # of rules sorted by NumC
    for (ItrNumC p=NumC.begin(); p!=NumC.end(); p++)
      cout << "NumC=" << p->first << "  " << p->second << "\n";
    cout << "\n";
  } 

  // show list of all rules
  if (FuncLIST) {
    for (ItrRules p=NumRules.begin(); p!=NumRules.end(); p++)
      cout << p->second << "  " << p->first << "\n";
  }
}

void
setParams(int argc, char* argv[])
{
  string arg;
  int i=0;
  while(++i<argc) {
    arg=argv[i];
    if (arg=="-stat") FuncSTAT = true;
    else if (arg=="-list") FuncLIST = true;
    else if (arg=="-filter") FuncFILTER = true;
    else if (arg=="-maxc") MaxC = atoi(argv[++i]);
    else if (arg=="-maxid") MaxID = atoi(argv[++i]);
    else if (arg=="-maxf") MaxF = atoi(argv[++i]);
    else {
      cerr << "Unrecognized option: " << arg << "\n";
      exit(1);
    }
  }
  if (MaxID==0 && MaxF!=0) error_exit("-maxid needed for -maxf");
}  

main(int argc, char* argv[])
{
  // set params
  setParams(argc,argv);
  if (! (FuncSTAT || FuncLIST || FuncFILTER)) {
    cerr << "nothing to be done.\n";
    exit(1);
  }

  // process input BXI
  int mod;
  string line,lineE,lineF;
  while(getline(cin,line)) {
    mod = NumLines++ % 3;
    if (mod==0) lineE=line;
    else if (mod==1) {
      lineF=line;
      // process pair
      Tree *rootE = new Tree(lineE);
      int maxc = rootE->getMaxChildren();
      if (maxc <= MaxC) {
	if (FuncSTAT || FuncLIST) {
	  NumMaxC[maxc]++;
	  process_node(rootE);
	} else if (FuncFILTER) {
	  int flen;
	  if (MaxID>0) {
	    rootE->limitID(MaxID);
	    lineE = ""; rootE->getLine(lineE);
	    limit_id(lineF,MaxID,flen);
	  } 
	  if (MaxF==0 || flen<MaxF)
	    // flen is only available if -maxid was specified...
	    cout << lineE << "\n" << lineF << "\n\n";
	}
      }
      delete rootE;
    }
  }

  // show results
  if (FuncSTAT||FuncLIST) show_results();

  cerr << "NumLines: " << NumLines << "\n";
  exit(0);
}

      
