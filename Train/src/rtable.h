#ifndef RTABLE_H
#define RTABLE_H

#include <map>
#include <fstream>

#include "misc.h"

// dummy constant
const string dummyWord = "0";
const string dummyRule = "0 0 0 0";

//
// basic table entry
//

class InsTable {
public:
  ProbCnt noins;
  map<string,ProbCnt> right;
  map<string,ProbCnt> left;
  typedef map<string,ProbCnt>::const_iterator InsTblItr;

  // for NullShared
  static bool nullShared;
  ProbCnt right_Shared, left_Shared;
  static map<string,ProbCnt> insword_Shared;
  static void normalize_insword_Shared();
  static void dump_insword_Shared(ofstream*);

  InsTable() { clear(); }
  void clear();
  void normalize();
  void dumpTable(ofstream *dump, int pos) const;

  void setProb(int insIdx, const string& word, double prob);
  double getProb(int insIdx, const string& word);
  void addCount(int insIdx, const string& word, double val);

  inline double getProb_noins() { return getProb(0,dummyWord); }
  inline double getProb_right(const string& word) { return getProb(1,word); }
  inline double getProb_left(const string& word) { return getProb(2,word); }

  inline void addCount_noins(double val) { addCount(0,dummyWord,val); }
  inline void addCount_right(const string& word, double val) { addCount(1,word,val); }
  inline void addCount_left(const string& word, double val) { addCount(2,word,val); }

  void print();			// for debug
};

class ProbRN {
public:
  static const int maxr = Params::MaxReorder;
  static const int maxc = Params::MaxChildren;
  static const int maxi = 3 * maxc;

  ProbCnt ord[maxr];		// reorder prob
  InsTable insFull[maxc];	// full non-shared version

  int numc;			// Note: numc is introduced at the end of writing code
				// when DumpTables() was added, so other methods don't
				// use this...

  ProbRN();			// need initialize prob/cnt

#if 0
  // it may not be needed
  void setProbN(int pos, int insIdx, const string& word, double prob) {
    insFull[pos].setProb(insIdx,word,prob);
  }
  double getProbN(int pos, int insIdx, string& word) {
    return insFull[pos].getProb(insIdx,word);
  }
  void addCountN(int pos, int insIdx, string& word, double val) {
    insFull[pos].addCount(insIdx,word,val);
  }
#endif
};

class RNTable {
  Params *param;
  ofstream *dump;

  bool useEqualProb;
  double EqualProbVal;
  bool oldMode;

  map<string,ProbRN> probs;	// RN prob for each rule (tree)
  typedef map<string,ProbRN>::const_iterator ProbRNItr;
  typedef map<string,ProbCnt>::const_iterator NullInsItr;

  static const int DUMMY_PARENT = 9999;
  void makeOldRuleStr(string& rstr, vector<Word>& rule);

  // internal access methods
  void getProbR(const string& rule, int numc, vector<double>& ord);
  void setProbR(const string& rule, int numc, vector<double>& ord);
  void addCountR(const string& rule, int numc, vector<double>& ord);

  void getProbN(string& rulestr, int numc,
		vector<string>& sentF, vector<double>& probs);
  void addCountN(string& rulestr, int numc,
		   vector<string>& sentF, vector<double>& cnts);

  void loadTableFile(ifstream&,string&);
  void loadUniformProbRN(const string& rule, int numc);
  void loadDumpFile(ifstream&,string&);

  // old mode 
  void getProbN_oldMode(vector<Word>& rule, vector<string>& sent, vector<double>& ins);
  void addCountN_oldMode(vector<Word>& rule, vector<string>& sent, vector<double>& cnts);

public:
  RNTable(Params *param);
  void LoadFile();

  void GetProbR(vector<Word>& rule, vector<double>& ord);
  void GetProbN(vector<Word>& rule, vector<Word>& sentF, vector<double>& ins);

  void AddCountR(vector<Word>& rule, vector<double>& ord);
  void AddCountN(vector<Word>& rule, vector<Word>& sentF, vector<double>& ins);

  void UpdateProbs();
  void DumpTables();

};

#endif // RTABLE_H


