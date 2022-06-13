#ifndef TRAIN_H
#define TRAIN_H

#include <fstream>
#include <cmath>

#include "misc.h"
#include "port.h"

class Train {
  Params *param;
  Port *port;

  double sumlogprob;
  double perplexity;
  int totalE;
  int numPairs, numNGs, numZeroProbs;

  void sendTableInfo();
  void checkReturnedProbs();
  void checkGraphs();		// for debug

 public:
  Train(Params *prm, Port *port);

  void Iterate(bool testMode=false);
  void GetResults(int&,double&,double&,int&,int&);
  void ShowResults(const string&);
};


#endif // TRAIN_H
