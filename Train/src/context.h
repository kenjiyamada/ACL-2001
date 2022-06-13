#ifndef CONTEXT_H
#define CONTEXT_H

#include "misc.h"

// context block for each Socket

class Context {
public:
  // context for TTable
  int tableIdx;			// master keeps track which table server is connecting.
				// gserver uses to identify which table server connected.

  // context for Graph (used by master)
  bool tableInfoSent;
  bool busy;			// Graph running
  double prob;			// return from graph server
  int lenE;			// return from graph server
  bool probRcvd;		// mark this prob must be processed.
#if 1
  int pairNo;			// pairID (returned back from graph)
  double cpuTime;		// cputime spent by graph
  int memSize;			// memsize at graph
#endif

  // context for TServer
  vector<Word> sentE, sentF;	// save from REQPROB to use for SENDCOUNT
  vector<int> symArr;		// save from REQPROB_RN to use for SENDOUNT_RN

  // context for GServer (and TServer)
#if 0
  vector<int> posArr;		// save when sending REQPROB to use for SENDPROB
				// also used by TServer for SENDCOUNT
  vector<int> poslenArr;	// save when receiving SENDPROB to use for SENDCOUNT
#else
  vector<int> poslenEFArr;	// saved by g-server upon receiving SENDPROB to use
				// for sending SENDCOUNT
#endif

  Context();

};

#endif // CONTEXT_H
