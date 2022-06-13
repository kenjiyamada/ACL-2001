#ifndef TPAIR_H
#define TPAIR_H

#include "tree.h"
#include "graph.h"

class TrainPair {
 public:
  int pairId;
  string lineE,lineF;
  vector<Word> wordE,wordF;

  double cpuTime;		// cpuTime when TrainPair is created

  Tree *troot;
  Graph *graph;

  vector<Tree*> nodeList;

  TrainPair(int,string&,string&,Params*);
  ~TrainPair();
  
  void collectEWords(Tree*);
  void collectSymbols(Tree*,vector<Word>&);
  void CollectRules(vector<int>&,vector<int>&,Tree *node=NULL);

};


#endif // TPAIR_H
