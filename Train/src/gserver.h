#ifndef GSERVER_H
#define GSERVER_h

#include <map>
#include <fstream>

#include "misc.h"
#include "port.h"
#include "tpair.h"
#include "graph.h"

class Vocab {
  // converter for integerized words into string words
  map<Word,string> vocTable;
public:
  void LoadVocab(string&,int);
  void word2str(Word,string&);
  void words2strs(vector<Word>&, vector<string>&);
};


class TableInfo {
public:
  Socket sc;
  string hostname;
  bool requesting;
  TableInfo();
};

class GServer {
  Params *param;
  Socket *master;

  int numTables;
  vector<TableInfo> tables;
  bool tablesConnected;

  TrainPair *tpair;

  // just for debugging, or backward compatibility
  Vocab vocE,vocF,symTbl;

  Socket::BufPtr packReqProb(Socket*,vector<int>&,int&);
  Socket::BufPtr packReqProbRN(Socket*,vector<int>&,vector<int>&,int&);
  void unpackGraphReq(int&,string&,string&,bool&);
  void waitAllProbs();

  // graph processing main methods
  double processGraph(bool);
  void requestProbsForGraph();
  void sendCountsForGraph(double);

 public:
  GServer(Params*, Socket*);
  void ProcessTableInfo();
  void ProcessTrainPair();
};


#endif // GSERVER_H
