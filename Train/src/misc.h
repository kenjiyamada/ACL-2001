#ifndef MISC_H
#define MISC_H

#include <string>
#include <vector>
#include <strstream>

extern "C" {
#include <stdio.h>
}

// for isnan(), isinf(), which are actually macros
extern "C" {
#include <math.h>
#ifdef sun
  /* sunmath.h is missing */
#define isinf(x) (0)
#endif
}

#ifndef DEBUG
#define DEBUG 0
#endif

// forward declaration
class Model;
class Train;

// definition for Word

typedef unsigned short Word;
static const Word WordIdMax = 65535;

Word str2word(string& str);
void str2words(string& str, vector<Word>& words);
void sout(string& str, Word w);
void sout(string& str, vector<Word>& words);


// used only by ttable.cc and rtable.cc
class ProbCnt {
public:
  double prob,cnt;
  ProbCnt() { prob=cnt=0.0; }
  void clear() { prob=cnt=0.0; }
};

// misc functions

inline void
split(const string& line, vector<string>& strs)
{ 
  // split a string into a vector of string
  istrstream ist(line.c_str());
  string w;
  while(ist>>w) strs.push_back(w);
}
  
// functions in memory.cc
void ShowMemory();
double UTime();
int MemSize();

int factorial(int n);

void getValidFLen(int esize, int& minF, int& maxF);

void error_exit(string msg);
void error_exit(string msg,int n);

inline void
perror_exit(char *msg)
{
  perror(msg);
  exit(1);
}

inline void
warn(string msg)
{
  cerr << "Warning: " << msg << "\n";
  cerr.flush();
}


// Global constants

static const int FPOS_LAMBDA1 = 127;
static const int FLEN_L1 = 127;
static const int FLEN_L1N = 126;


// Parameter Block

class Params {
 public:
  static const int MaxSentELen = 20;
  static const int MaxSentFLen = 30;
  static const int MaxChildren = 4;
  static const int MaxReorder = 24;		// factrial of MaxChildren

  int Verbose;
  int Sleep;

  int NumIterate;
  int MinGraphServers;
  int NumTableServers;

  string TrainFile;
  string TestFile;
  string VocabEFile;
  string VocabFFile;
  string SymTblFile;
  int MaxVocabId;

  bool ShowTree;
  bool ShowUsedProbs;		// show probs used in the best alignment
  bool ShowUsedProbsInPhrase;	// show probs used (even in phrase)
  bool ShowPhraseProbs;		// show phrase probs in the best alignment
  bool ShowFertZero;		// show fertility zero context

  // flags for Table server
  string HostMaster;

  int TableIdx;
  string TableFile;
  bool NoTableFile;

  string DumpFile;		// dump file suffix
  bool NoDumpFile;

  bool NoUpdate;		// not update table

  double LambdaPhrase;		// -L or -lambda1
  double LambdaLeaf;		// -M or -lmabda2
  bool LambdaPhraseConst;	// not scaling -lambda1 by elen
  bool LambdaPhraseVary;	// individual lambda1 as variable
  double TrainFloorProb;	// floor prob for training (used for run2.sh for alignment)
  double TestFloorProb;		// floor prob for test perplexity

  bool UseEqualProb;		// not uniform, but equal probs for init table
  static const double EqualProbVal = 0.1;

  bool UseCutoff;		// for t-table only. meaningful when UseEqualProb
				// is applied. use table entry to allow such probs.


  bool ShareNullProb;		// old compatibility

  int FertileMode;		// 0: original, 1: IBM style, 2: shared fertility

  bool OldMode;			// only 1-to-1 t-table, ignore parent in r-table,
				// and use label/parent pair in n-table.
				// note: -oldmode will set ShareNullProb too

  bool OldMode2;		// same as oldmode, except allowing non 1-to-1 t-table.

  Params(int argc, char* argv[]);
  void Set(int argc, char* argv[]);
  void Print();

  // for debug
  int argc;
  char **argv;

};

#endif // MISC_H

