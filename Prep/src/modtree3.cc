
//
// convert collins parse tree
//

// This code came from SMT/Tetun/modtree2.cc

// This code came from SMT/Parse/modtree.cc
// modified to use for dep-em3.cc

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <strstream>
#include <fstream>
#include <algorithm>

//
// Parameters
//

#define MAP_STRING_STRING 0

class VocabTable {
#if MAP_STRING_STRING
  map<string,string> table;
#else
  map<string,int> table;
#endif
public:
  VocabTable(string file);
  string Convert(const string& from);
};

namespace Params {
  enum FuncID { NONE=0, FLAT, NOFLAT, AUX, PAS, CONV, UPQPUNC, RMPUNC, RMHEAD, LOWER, VOCAB, CONVSYM, MAX_ID, UP, DOWN };
  vector<FuncID> FuncArray;
  bool FlatUp = true;
  bool ExpandLeaf = false;
  bool PrintNoOriginal = false;
  bool PrintList = false;
  bool PrintParentRight = true;
  bool PrintHead = false;
  bool PrintMaxc = false;
  bool PrintNumc = false;
  bool PrintSymbolTable = false;
  bool ProcessBtx = false;
  int LimitMaxc = 0;
  int PickMaxc = -1;
  string ConvFile = "";

  string WordVocabFile = "";
  string SymbolVocabFile = "";
  int MaxVocabId = 65500;

  VocabTable *wordVocab = 0;
  VocabTable *symbolVocab = 0;

  void SetParams(int argc, char* argv[]);
  void setAll();

  void SetConvTable(string& from, string& to);
  void GetConvTable(string& from, string& to);
  vector<string> idx2from;
  vector<string> idx2to;
}

void
Params::SetConvTable(string& from, string& to)
{
  idx2from.push_back(from);
  idx2to.push_back(to);
}

void
Params::GetConvTable(string& from, string& to)
{
  for (int i=0; i<idx2from.size(); i++) {
    if (idx2from[i]==from) {
      to = idx2to[i]; return;
    }
  }
  to = "";
}

void
Params::SetParams(int argc, char* argv[])
{
  string arg;
  int i=0;
  while(++i<argc) {
    arg=argv[i];
    if (arg=="-lf") ExpandLeaf = true;
    else if (arg=="-flat") FuncArray.push_back(FLAT);
    else if (arg=="-noflat") FuncArray.push_back(NOFLAT);
    else if (arg=="-aux") FuncArray.push_back(AUX);
    else if (arg=="-pas") FuncArray.push_back(PAS);
    else if (arg=="-upqpunc") FuncArray.push_back(UPQPUNC);
    else if (arg=="-rmpunc") FuncArray.push_back(RMPUNC);
    else if (arg=="-rmhead") FuncArray.push_back(RMHEAD);
    else if (arg=="-lower") FuncArray.push_back(LOWER);
    else if (arg=="-conv") { FuncArray.push_back(CONV); ConvFile=argv[++i]; }
    else if (arg=="-all") setAll();
    else if (arg=="-up") FlatUp = true;
    else if (arg=="-down") FlatUp = false;
    else if (arg=="-list") PrintList = true;
    else if (arg=="-pl") PrintParentRight = false;
    else if (arg=="-pr") PrintParentRight = true;
    else if (arg=="-ph") PrintHead = true;
    else if (arg=="-symtbl") PrintSymbolTable = true;
    else if (arg=="-vocab") { FuncArray.push_back(VOCAB); WordVocabFile=argv[++i]; }
    else if (arg=="-convsym") { FuncArray.push_back(CONVSYM); SymbolVocabFile=argv[++i]; }
    else if (arg=="-maxvid") MaxVocabId = atoi(argv[++i]);
    else if (arg=="-maxc") PrintMaxc = true;
    else if (arg=="-numc") PrintNumc = true;
    else if (arg=="-btx") ProcessBtx = true;
    else if (arg=="-noorg") PrintNoOriginal = true;
    else if (arg=="-limc") LimitMaxc = atoi(argv[++i]);
    else if (arg=="-pmaxc") PickMaxc = atoi(argv[++i]);
    else {
      cerr << "Unrecognized option: " << arg << "\n";
      exit(1);
    }
  }
}

void
Params::setAll()
{
  // to be used as "-conv xxx -all"

  FuncArray.push_back(RMPUNC);
  FuncArray.push_back(FLAT);
  FuncArray.push_back(PAS);
  FuncArray.push_back(AUX);
  FuncArray.push_back(RMHEAD);
}

//
// Error Handler
//

void
error_exit(const string& msg)
{
  cerr << msg << "\n";
  exit(1);
}

//
// Token
//

namespace Token {
  enum TkVal { EOS=-1, EMPTY, STR, LPAR, RPAR };
  char ch_LPAR='(', ch_RPAR=')', ch_EOS=char(-1);
  TkVal SavedToken=EMPTY;
  string TokenString;
  char SavedChar=0;

  void SetParens();
  TkVal GetToken();
  void BackToken(TkVal v) { SavedToken=v; }
  char GetChar();
  void BackChar(char ch);
  inline bool IsWhiteSpace(char ch) { return (ch==' '||ch=='\t'||ch=='\n'); }
  inline void SkipWhiteSpaces();
  void GetLine(string& line);
}

void
Token::SetParens()
{
  char c = Token::GetChar();
  if (c == '[') {
    ch_LPAR='['; ch_RPAR=']';
  } else {
    ch_LPAR='('; ch_RPAR=')';
  }
  BackChar(c);
}

char
Token::GetChar()
{
  int ch;
  if (SavedChar) {
    ch = SavedChar; SavedChar = 0;
  } else {
    ch = cin.get();
  }
  return ch;
}

void
Token::BackChar(char ch)
{
  SavedChar = ch;
}

inline void
Token::SkipWhiteSpaces()
{
  char ch;
  while (IsWhiteSpace(ch=GetChar()))
    if (ch==ch_EOS) error_exit("EOF detected");
  BackChar(ch);
}

void
Token::GetLine(string& line)
{
  // get char into line until EOL (EOL is discarded).
  char ch;
  line="";
  while((ch=GetChar())!='\n') line += ch;
}

Token::TkVal
Token::GetToken()
{
  string tmp;
  TkVal val;
  if (SavedToken!=EMPTY) {
    val = SavedToken; SavedToken=EMPTY;
  } else {
    SkipWhiteSpaces();
    char ch = GetChar();
    if (ch==ch_EOS) val=EOS;
    else if (ch==ch_LPAR) val=LPAR;
    else if (ch==ch_RPAR) val=RPAR;
    else {			// STR
      val=STR; TokenString="";
      while (!IsWhiteSpace(ch) && ch!=ch_LPAR && ch!=ch_RPAR) {
	if (ch=='\\') ch = GetChar(); // escape char
	TokenString += ch; ch = GetChar();
	tmp = TokenString;
      }
      if (!IsWhiteSpace(ch)) BackChar(ch);
    }
  }
  return val;
}

//
// SymbolTable
//

class SymbolTable {
  map<string,int> table;
public:
  void Store(string& str);
  void Print();
};

class SymFreq {
public:
  string sym;
  int freq;
  SymFreq(string s, int i) : sym(s), freq(i) {}
};

bool CountGT(SymFreq,SymFreq);

void
SymbolTable::Store(string& str)
{
  // table entry is a frequency count.
  table[str]++;
}

void
SymbolTable::Print()
{
  vector<SymFreq> syms;
  typedef map<string,int>::const_iterator CI;

  // obtain list of symbols
  for (CI p=table.begin(); p!=table.end(); p++) {
    SymFreq ent(p->first,p->second);
    syms.push_back(ent);
  }

  // sort symbols by the frequency count
  sort(syms.begin(), syms.end(), CountGT);

  // print
  for (int i=0; i<syms.size(); i++) {
    cout << i+1 << '\t' << syms[i].sym << '\t' << syms[i].freq << '\n';
  }
}

bool
CountGT(SymFreq x, SymFreq y)
{
  return (x.freq > y.freq);
}

//
// Vocab Table
//

// class definition is at the top

VocabTable::VocabTable(string file)
{
  ifstream f(file.c_str());
  if (!f) error_exit("cannot open VocabFile[" + file + "]");

  string line;
  while(!f.eof()) {
    getline(f,line);
    istrstream ist(line.c_str());
    string id,word,freq;
    ist >> id >> word >> freq;
    if (atoi(id.c_str()) <= Params::MaxVocabId) {
#if MAP_STRING_STRING
      table[word]=id;
#else
      table[word]=atoi(id.c_str());
#endif
    }
  }
}

string
VocabTable::Convert(const string& from)
{
#if MAP_STRING_STRING
  static string to;
  to = table[from];
  if (to=="") to = table["UNK"];
  return to;
#else
  int to;
  to = table[from];
  if (to==0) to = table["UNK"];
  ostrstream ost;
  ost << to << '\0';
  return ost.str();
#endif  
}

//
// Tree
//

class Tree {
public:
  // Memo: ReadTree() reads into 'label' as an original string, (eg. NP~dog~2~1 for non-terminal,
  // or dog/NNP for terminal), and calls parseLabel() to set 'symbol' and 'head'
  // (eg. symbol=NP, head=dog).
  // In main(), SetLabels() is called, which resets 'label' as formatted (such as NP/dog) string,
  // before applying several functions.
  // For example, RemoveHead() just copies 'symbol' to 'label' for all non-terminals.

  string label;
  string symbol,head;
  vector<Tree *> children;

  Tree(string& s) : label(s) {}	// single node constructor
  Tree(Tree *node);		// copy entire tree
  ~Tree();
    
  static Tree *ReadTree();		// read from stdin
  void SetLabels();
  void ExpandLeaves();
  void Flatten();
  void NoFlatten();
  void FixPassive();
  void FixAux();
  void ConvertSymbols();
  void RaiseQPunc(Tree *root);
  void RemovePunc();
  void RemoveHead();
  void LowercaseHead();
  void ConvertWordVocab();
  void ConvertSymbolVocab();

  void Print();
  void PrintPlain();
  void PrintList();
  void StoreSymbols(SymbolTable& tbl);
  int GetMaxChildren();
  void PrintNumChildren();
  bool IsLeaf() { return (numc()==0); }
private:
  inline int numc() { return children.size(); }
  inline bool hasHead() { return head.size(); }

  void parseLabel();
  void parseNonLeafLabel();
  void parseLeafLabel();

  bool beVerb();
  bool auxVerb();

  void printIndent(int indent=0);
  void printLabel();
  string getComposedLabel();

};

Tree::Tree(Tree *node)
{
  // copy entire tree
  label = node->label;
  symbol = node->symbol;
  head = node->head;
  for (int i=0; i<node->numc(); i++) {
    Tree *c = new Tree(node->children[i]);
    children.push_back(c);
  }
}

Tree::~Tree()
{
  for (int i=0; i<numc(); i++) 
    delete children[i];
}

//
// Tree Reader
//

Tree *
Tree::ReadTree()
{
  // read from stdin
  Tree *node;
  Token::TkVal v = Token::GetToken();
  if (v==Token::EOS) node = 0;
  else if (v==Token::STR) {
    node = new Tree(Token::TokenString);
  } else if (v==Token::LPAR) {
    v = Token::GetToken();
    if (v!=Token::STR) error_exit("node-label expected");
    node = new Tree(Token::TokenString);
    // get children
    v = Token::GetToken();
    while (v!=Token::RPAR) {
      Token::BackToken(v);
      Tree *child = ReadTree();
      if (!child) {
	cerr << "EOF during reading a list\n";
	break;
      }
      node->children.push_back(child);
      v = Token::GetToken();
    }
  } else {
    error_exit("LPAR or STR expected");
  }

  if (node) node->parseLabel();
  return node;
}

void
Tree::parseLabel()
{
  // parse label and store into symbol and head.
  // Label is not changed. It can be set by SetLabels() later.

  if (numc()>0) parseNonLeafLabel();
  else parseLeafLabel();
}


void
Tree::parseNonLeafLabel()
{
#if 1
  char sep = '~';
  int len = label.length();
  int pos1 = label.find(sep);

  // find second tilder from end. ex NP~head~1~2
  int pos2,cnt2=0;
  for (pos2=len-1; pos2>0; pos2--) if (label[pos2]==sep && ++cnt2>=2) break;
  if (pos1>=pos2) cerr << "Unrecognizable Leaf: [" << label << "]\n";

  symbol=label.substr(0,pos1);	// left of leftmost tilder
  head=label.substr(pos1+1,pos2-pos1-1); // between leftmost tilder and the second from end

#else
  int len = label.length();
  int i,j; char sep='~';
  symbol=""; head="";
  for (i=0; i<len; i++) if (label[i]==sep) break;
  if (i!=0) {
    symbol = label.substr(0,i);
    for (j=i+1; j<len; j++) if (label[j]==sep) break;
    if (j!=i+1) {
      head = label.substr(i+1,j-(i+1));
    }
  }
#endif
}      

void
Tree::parseLeafLabel()
{
  int pos = label.rfind('/');
  int len = label.size();
  if (0<=pos && pos<len) {
    //rind() should handle the following...
    //if (pos==0 && label[1]=='/') pos=1;	// head="/"
    head = label.substr(0,pos);
    symbol = label.substr(pos+1,len-(pos+1));
  } else {
    symbol="???"; head=label;
  }    
}

//
// SetLabels
//

void
Tree::SetLabels()
{
  label = head + "/" + symbol;
  
  for (int i=0; i<numc(); i++)
    children[i]->SetLabels();
}

//
// ExpandLeaves
//

void
Tree::ExpandLeaves()
{
  // expand a leaf head/sym => sym-head tree

  if (numc()>0) {
    for (int i=0; i<numc(); i++) children[i]->ExpandLeaves();
  } else {
    Tree *tr = new Tree(head);
    children.push_back(tr);
    label = symbol;
  }
}

//
// Flatten
//

void
Tree::Flatten()
{
  // flatten tree (also modify label from stored symbol and head.

  if (numc() == 0) {
    /* do nothing */
  } else {
    // save children to local copy, and clear
    vector<Tree *> clis = children; // copy list of original children
    children.clear();

    // re-insert children
    for (int i=0; i<clis.size(); i++) {
      Tree *c = clis[i];
      c->Flatten();
      if (hasHead() && c->head == head) {
	if (Params::FlatUp) symbol = c->symbol;
	if (c->numc()==0) {
	  // head leaf
	  if (clis.size()==1) {
	    // if this node has only one leaf child, this node
	    // itself should be the leaf.
	    label=c->label; head=c->head; symbol=c->symbol;
	  } else
	    children.push_back(c);
	} else {
	  for (int ci=0; ci<c->numc(); ci++)
	    children.push_back(c->children[ci]);
	}
	// delete c;
      } else {
	children.push_back(c);
      }
    }
  }
  
  // modify label from symbol and head
  if (hasHead())
    label = head + "/" + symbol;
}

void
Tree::NoFlatten()
{
  // not flatten tree, but propagate symbol from head and
  // modify label from stored symbol and head, as Flatten() does.

  if (numc() == 0) {
    /* do nothing */
  } else {
    for (int i=0; i<children.size(); i++) {
      Tree *c = children[i];
      c->NoFlatten();
      if (hasHead() && c->head == head) {
	if (Params::FlatUp) symbol = c->symbol;
      }
    }
  }

  // modify label from symbol and head
  if (hasHead())
    label = head + "/" + symbol;
}

//
// Fix Passive
//

void
Tree::FixPassive()
{
  bool changed = false;
  if (numc() == 0) {
    /* do nothing */
  } else {
    // save children to local copy, and clear
    vector<Tree *> clis = children; // copy list of original children
    children.clear();

    // re-insert children
    bool be = (hasHead() && beVerb());
    for (int i=0; i<clis.size(); i++) {
      Tree *c = clis[i];
      c->FixPassive();
      if (be && (c->symbol == "VP-A" || c->symbol == "VB")) {
	head = c->head; symbol = c->symbol; changed=true;
	if (c->numc()==0) children.push_back(c); // head leaf
	else
	  for (int ci=0; ci<c->numc(); ci++) children.push_back(c->children[ci]);
	// delete c;
      } else {
	children.push_back(c);
      }
    }
  }

  // update label if head or symbol was changed
  if (changed) label = head + "/" + symbol;
}

//
// Fix Aux verb
//

void
Tree::FixAux()
{
  bool changed = false;
  if (numc() == 0) {
    /* do nothing */
  } else {
    // save children to local copy, and clear
    vector<Tree *> clis = children; // copy list of original children
    children.clear();

    // re-insert children
    bool aux = (hasHead() && auxVerb());
    for (int i=0; i<clis.size(); i++) {
      Tree *c = clis[i];
      c->FixAux();
      if (aux && (c->symbol == "VP-A" || c->symbol == "VB")) {
	head = c->head; symbol = c->symbol; changed=true;
	if (c->numc()==0) children.push_back(c); // head leaf
	else
	  for (int ci=0; ci<c->numc(); ci++) children.push_back(c->children[ci]);
	// delete c;
      } else {
	children.push_back(c);
      }
    }
  }

  // update label if head or symbol was changed
  if (changed) label = head + "/" + symbol;
}

string
toLower(string& str0)
{
  string str = str0;
  for (int i=0; i<str.size(); i++) {
    char& c=str[i];
    if ('A' <= c && c <= 'Z') c = c + ('a' - 'A');
  }
  return str;
}

bool
Tree::beVerb()
{
  string head = toLower(this->head);
  return ((symbol == "VP" || symbol == "VP-A" || symbol == "VB") &&
	  (head == "be" || head == "is" || head == "are" ||
	   head == "been" || head == "am" || head == "was" ||
	   head == "were"));
}

bool
Tree::auxVerb()
{
  string head = toLower(this->head);
  return ((symbol == "VP" || symbol == "MD" || symbol == "VB" ) &&
	  (head == "must" || head == "may" || head == "will" || head == "can" || head == "should" ||
	   head == "have" || head == "would" || head == "does" || head == "do" ||
	   head == "shall" || head == "could" || head == "might" || head == "did" ||
	   head == "did" || head == "has" || head == "had"));
}

//
// Convert Symbol
//

void
ReadConvertSymbols()
{
  string fname = Params::ConvFile;
  ifstream f(fname.c_str());
  if (!f) error_exit("cannot open ConvFile[" + fname + "]");

  string line; 
  while(!f.eof()) {
    getline(f,line);
    istrstream ist(line.c_str());
    string from,to;
    ist >> from >> to;
    //    Params::ConvTable[from]=to;
    Params::SetConvTable(from,to);
  }
}

void
Tree::ConvertSymbols()
{
  if (symbol.size()) {
    //    string nsym = Params::ConvTable[symbol];
    string nsym; Params::GetConvTable(symbol,nsym);
    if (nsym.size()) {
      symbol = nsym;
      if (head.size()) label = head + "/" + symbol;
      else label = symbol;
    }
  }
  if (numc())
    for (int i=0; i<numc(); i++) children[i]->ConvertSymbols();
}

//
// Remove Punc
//

void
Tree::RemovePunc()
{
  // remove ./PUNC if it's the last child (and there are other children)

  if (numc()>0) {
    Tree *nd = children[numc()-1];
    if (numc()>1 && nd->symbol == "PUNC") {
      children.pop_back();
      delete nd;
    }
    for (int i=0; i<numc(); i++) children[i]->RemovePunc();
  }

#if 0
  // OLD CODE
  if (numc()>0) {
    Tree *nd = children[numc()-1];
    if (nd->numc()>0) nd->RemovePunc();
    else {
      // assume PUNCx is converted to PUNC by conv.tbl
      if (nd->symbol == "PUNC")
	children.pop_back();
    }
  }
#endif
}

//
// Raise Q-PUNC (! and ?)
//

void
Tree::RaiseQPunc(Tree *root)
{
  // raise !/PUNC. or ?/PUNC., if it's the last child
  // Note: this function must be called after -conv AND before -rmpunc
  if (numc()>0) {
    Tree *nd = children[numc()-1];
    if (nd->numc()>0) nd->RaiseQPunc(root);
    else {
      if (nd->symbol == "PUNC" && (nd->head=="!" || nd->head=="?")) {
	// raise it to root
	children.pop_back();
	root->children.push_back(nd);
	nd->symbol = "PUNCX";
	nd->label = nd->head + "/" + nd->symbol;
      }	
    }
  }
}

//
// Remove Head (for use by dep-em3)
//

void
Tree::RemoveHead()
{
  if (numc()>0) {
    label = symbol;
    for (int i=0; i<numc(); i++)
      children[i]->RemoveHead();
  }
}

//
// Lowercase Head
//

void
Tree::LowercaseHead()
{
  if (numc()>0)
    for (int i=0; i<numc(); i++) children[i]->LowercaseHead();

  if (hasHead()) {
    head = toLower(head);
    label = head + "/" + symbol;
  }
}

//
// Vocab Conversion
//

void
Tree::ConvertWordVocab()
{
  for (int i=0; i<numc(); i++) children[i]->ConvertWordVocab();

  if (hasHead()) {		// why need this?
    head = Params::wordVocab->Convert(head);
    label = head + "/" + symbol;
  }
}

void
Tree::ConvertSymbolVocab()
{
  for (int i=0; i<numc(); i++) children[i]->ConvertSymbolVocab();

  symbol = Params::symbolVocab->Convert(symbol);
  label = head + "/" + symbol;
}

//
// Print
//

void
Tree::Print()
{
  this->printIndent(0);
}

void
Tree::PrintPlain()
{
  if (numc()==0) this->printLabel();
  else {
    cout << "(";
    this->printLabel();
    for (int i=0; i<numc(); i++) {
      cout << " "; children[i]->PrintPlain();
    }
    cout << ")";
  }
}

void
Tree::printIndent(int indent=0)
{
  for (int i=0; i<indent; i++) cout << " ";
  cout << "[" << label << "] (" << symbol << "," << head << ")\n";
  for (int i=0; i<numc(); i++)
    children[i]->printIndent(indent+1);
}

void
Tree::printLabel()
{
  for (int i=0; i<label.length(); i++) {
    char c = label[i];
    if (c==Token::ch_LPAR || c==Token::ch_RPAR || c=='\\') cout << '\\';
    cout << c;
  }
}

void
Tree::PrintList()
{
  bool right = Params::PrintParentRight, phead = Params::PrintHead;
  int len = 30;

  if (hasHead() && numc()>0) {
    string str = "";
    if (!right) str += (symbol + " <= ");
    for (int i=0; i<numc(); i++) str += (children[i]->symbol + " ");
    if (right) str += ("=> " + symbol);

    // details
    if (phead) {
      int pad;
      if ((pad=len-str.size())>0) while (pad-->0) str += " ";
      if (!right) str += (getComposedLabel() + " <= ");
      for (int i=0; i<numc(); i++) str += (children[i]->getComposedLabel() + " ");
      if (right) str += ("=> " + getComposedLabel());
    }
    cout << str << "\n";

    // recursively call itself
    for (int i=0; i<numc(); i++) children[i]->PrintList();
  }
}

string
Tree::getComposedLabel()
{
  // returns head/symbol, taking care of expanded leaf.
  string str;
  if (hasHead()) {
    str = head;
  } else {
    if (numc() != 1 || children[0]->numc() != 0) error_exit("unexpected leaf");
    str = children[0]->label;
  }
  str += ("/" + symbol);
  return str;
}

//
// Print Max/Num Children
//

int
Tree::GetMaxChildren()
{
  int max=numc();
  for (int i=0; i<numc(); i++) {
    int cnum = children[i]->GetMaxChildren();
    if (cnum > max) max = cnum;
  }
  return max;
}

void
Tree::PrintNumChildren()
{
  if (numc()>0) cout << "Numc= " << numc() << "\n";
  for (int i=0; i<numc(); i++)
    children[i]->PrintNumChildren();
}

//
// Store Symbols
//

void
Tree::StoreSymbols(SymbolTable& tbl)
{
  tbl.Store(symbol);
  for (int i=0; i<numc(); i++)
    children[i]->StoreSymbols(tbl);
}

//
// main
//

int
main(int argc, char* argv[])
{
  SymbolTable SymTbl;
  Params::SetParams(argc,argv);
  Token::SetParens();

  if (Params::ConvFile.size()) ReadConvertSymbols();
  if (Params::WordVocabFile.size())
    Params::wordVocab = new VocabTable(Params::WordVocabFile);
  if (Params::SymbolVocabFile.size())
    Params::symbolVocab = new VocabTable(Params::SymbolVocabFile);

  Tree *tr;
  while(tr=Tree::ReadTree()) {
    Tree *ntr = new Tree(tr);	// copy tree

    // new label from head/symbol with symbol conv table
    ntr->SetLabels();
    if (Params::ConvFile.size()) ntr->ConvertSymbols();

    // modify the tree as directed by the options
    for (int i=0; i<Params::FuncArray.size(); i++) {
      switch (int id=Params::FuncArray[i]) {
      case Params::FLAT: ntr->Flatten(); break;
      case Params::NOFLAT: ntr->NoFlatten(); break;
      case Params::AUX: ntr->FixAux(); break;
      case Params::PAS: ntr->FixPassive(); break;
      case Params::CONV: ntr->ConvertSymbols(); break;
      case Params::UPQPUNC: ntr->RaiseQPunc(ntr); break;
      case Params::RMPUNC: ntr->RemovePunc(); break;
      case Params::RMHEAD: ntr->RemoveHead(); break;
      case Params::LOWER: ntr->LowercaseHead(); break;
      case Params::VOCAB: ntr->ConvertWordVocab(); break;
      case Params::CONVSYM: ntr->ConvertSymbolVocab(); break;
      default: error_exit("unrecognized FuncID" + int(id));
      }
    }

    // expand leaf if specified
    if (Params::ExpandLeaf) {
      tr->ExpandLeaves();
      ntr->ExpandLeaves();
    }

    // print modified tree or child list
    if (Params::ProcessBtx) {
      string line;
      Token::GetLine(line);	// dummy read to EOL
      Token::GetLine(line);	// JSent
      if (ntr->GetMaxChildren() <= Params::LimitMaxc && tr->head != "SentenceTooLong") {
	// print e-tree
	if (ntr->IsLeaf()) {
	  // one-word sentence
	  cout << "(";
	  ntr->PrintPlain();
	  cout <<")\n";
	} else {
	  ntr->PrintPlain(); cout << "\n";
	}
	// print j-sent (and blank)
	cout << line << "\n\n";
      }
      Token::GetLine(line);	// blank line
    } else if (Params::PrintMaxc) {
      cout << "MaxC= " << ntr->GetMaxChildren() << "\n";
    } else if (Params::PrintNumc) {
      ntr->PrintNumChildren();
    } else if (Params::PrintList) {
      if (Params::LimitMaxc==0 || ntr->GetMaxChildren() <= Params::LimitMaxc) {
	ntr->PrintList(); cout << "\n";
      }
    } else if (Params::PickMaxc > 0) {
      // pick tree whose maxc matches the specified number
      if (ntr->GetMaxChildren()==Params::PickMaxc) {
	tr->PrintPlain(); cout << "\n";	// original tree
	ntr->PrintPlain(); cout << "\n"; // modified tree
      }
    } else if (Params::PrintSymbolTable) {
      ntr->StoreSymbols(SymTbl);
    } else {
      // normal output. original tree and modified tree.
      if (!Params::PrintNoOriginal) { tr->PrintPlain(); cout << "\n"; }
      ntr->PrintPlain(); cout << "\n";
    }
    delete tr;
    delete ntr;
  }
  if (Params::PrintSymbolTable) SymTbl.Print();
}

