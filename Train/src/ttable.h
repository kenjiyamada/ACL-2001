#ifndef TTABLE_H
#define TTABLE_H

#include <map>
#include <fstream>

#include "misc.h"

class FWords {
public:
  static const int maxFert = Params::MaxSentFLen+1;
  map<string,ProbCnt> words;
  ProbCnt fertility[maxFert];	// fertility for each EWord
  double lambda1prob,lambda1cnt,lambda1ncnt; // used if -lambda1vary
  FWords();
};

//
// T-Table
//

class TTable {
  Params *param;
  ofstream *dump;

  bool useEqualProb,useCutoff;
  double EqualProbVal;
  int fertileMode;
  bool oldMode,oldMode2;

  map<string,FWords> e2f;
  typedef map<string,FWords>::const_iterator EWordItr;
  typedef map<string,ProbCnt>::const_iterator FWordItr;

  // internal access methods
  double getProb(const string& eword, const string& fword);
  void setProb(const string& eword, const string& fword, double val);
  void addCount(const string& eword, const string& fword, double val);

  // handling fertility
  double getProb_Fertile1(const string& estr, vector<Word>& fword);
  void addCount_Fertile1(const string& estr, vector<Word>& fword, double val);
  void readFertilityLine(const string& eword, const string& line);

  // lambda1
  double getLambda1Prob(const string& eword);
  void addLambda1Count(const string& eword, double val);
  void addLambda1nCount(const string& eword, double val);
  void readLambda1Line(const string& estr, const string& line);

public:
  TTable(Params *param);
  void LoadFile();

  double GetProb(vector<Word>& eword, vector<Word>& fword);
  void AddCount(vector<Word>& eword, vector<Word>& fword, double val);
  void UpdateProbs(bool disableEqlFlag=true);
  void DumpTables();

  // lambda1
  double GetLambda1Prob(vector<Word>& eword);
  void AddLambda1Count(vector<Word>& eword, double val);
  void AddLambda1nCount(vector<Word>& eword, double val);

};

#endif // TTABLE_H


