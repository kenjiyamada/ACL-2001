#include "train.h"

Train::Train(Params *prm, Port *port)
  : param(prm), port(port)
{
}  

void
Train::GetResults(int& totalE, double& sumlogprob, double& perplexity,
		  int& npair, int& ngs)
{
  totalE = this->totalE;
  sumlogprob = this->sumlogprob;
  perplexity = this->perplexity;

  npair = this->numPairs;
  ngs = this->numNGs;
}

void
Train::ShowResults(const string& header)
{
  cout << "\n" << header;
  cout << " #Pairs=" << numPairs << ",#NG=" << numNGs << ",";
  if (numZeroProbs>0) cout << ",#ZeroP=" << numZeroProbs << ",";
  cout << "#EWords=" << totalE << ",sumlogprob=" << sumlogprob
       << ",peplexity=" << perplexity << "\n";
}

void
Train::checkReturnedProbs()
{
  Socket *sg;
  for (int i=0; i<port->socks.size(); i++) {
    sg = port->socks[i];
    if (sg->type == TYPE_GRAPH && sg->ctx->probRcvd) {
      int lenE = sg->ctx->lenE;
      double prob = sg->ctx->prob;

#if 1
      // print info returned back from graph
      cerr << "got pair# " << sg->ctx->pairNo << " prob: " << sg->ctx->prob
	   << " cpu: " << sg->ctx->cpuTime
	   << " mem: " << sg->ctx->memSize << "\n";
      cerr.flush();
#endif      

#if 0
	cout << "Received lenE=" << lenE << ",prob=" << prob << "\n";
#endif
      if (lenE<=0) {
	// lenE==0 indicates some error (maybe too long) in graph
	numNGs++;
      } else if (prob==0 || isnan(prob) || isinf(prob)) {
	numZeroProbs++;
      } else {
	sumlogprob += log(prob)/log(2);
	totalE += lenE;
      }
#if 1
      if (lenE==0) cout << "*";
      else if (prob==0 || isnan(prob) || isinf(prob)) cout << "!";
      else cout << ".";
      cout.flush();
#endif
      sg->ctx->probRcvd = false;
    }
  }
}

void
Train::checkGraphs()
{
  // for debug: show all Graph status
  for (int i=0; i<port->socks.size(); i++) {
    Socket *sg = port->socks[i];
    if (sg->type == TYPE_GRAPH) {
#if DEBUG
      cout << "Socks[" << i << "]=" << ((sg->ctx->busy)?"busy":"idle") << "\n";
#endif
    }
  }
}

void
Train::Iterate(bool testMode=false)
{
  // perform one iteration

  // read training file
  // dispatch to graph servers (they return prob)
  // note: don't forget to send TABLEINFO if not sent

  // initialize perplexity
  perplexity = 0.0;
  sumlogprob = 0.0;
  totalE = 0;

  numPairs=numNGs=numZeroProbs=0;

  string& fname = (testMode ? param->TestFile : param->TrainFile);
  ifstream in(fname.c_str());
  if (!in) error_exit("can't open file [" + fname + "]");

  string lineE, lineF;
  string line; int lineno = 0;
  while(getline(in,line)) {
    int mod = lineno++ % 3;
    if (mod==0) {
      lineE = line; //cout << "lineE[" << lineE << "]\n";
    } else if (mod==1) {
      lineF = line; //cout << "lineF[" << lineF << "]\n";
    } else {
      numPairs++;
      if (numPairs % 100 == 0) {
	cout << "[" << numPairs << "]";
	cout.flush();
      }

      //cout << "line2[" << line << "]\n";
      if (line.size()>0) error_exit("broken train file at line ", lineno);
      cout.flush();

      // find available graph
      Socket *sg;
      bool found=false;
      for (int i=0; i<port->socks.size(); i++) {
	sg = port->socks[i];
	if (sg->type == TYPE_GRAPH && !sg->ctx->busy) {
	  found=true; break;
	}
      }
      if (!found) {
	sg = port->WaitAvailableGraph();
	checkReturnedProbs();	// process returned prob into sumprob
      }

      // use the found graph
      sg->ctx->busy = true;
      
      // send TABLEINFO if necessary
      if (!sg->ctx->tableInfoSent) port->SendTableInfo(sg);
      
      // pack lineE and lineF (and testMode flag)
#if 1
      int totlen = sizeof(int) + sizeof(short)*2 + lineE.size() + lineF.size() + 1;
#else
      int totlen = sizeof(short)*2 + lineE.size() + lineF.size() + 1;
#endif
      char *buff = (char *)malloc(totlen);
      char *pt = buff;
      sg->PackInt(pt,numPairs);
      sg->PackString(pt,lineE);
      sg->PackString(pt,lineF);
      sg->PackChar(pt,testMode);
      
      // send GRAPHREQ
#if DEBUG
      cerr << "Sending " << (testMode?"*":"#")
	   << numPairs << " [" << lineE << "][" << lineF << "]\n";
      cerr.flush();
#endif
      sg->SendPacket(FUNC_GRAPHREQ, buff, totlen);
      free(buff);
      
      // proceed to next training pair...
    }
  } // end of while

#if 0
    sleep(3);
#endif

  // wait all graphs are done
  cout << "Waiting all graphs are done...\n";
  cerr << "Waiting all graphs are done...\n"; cerr.flush();
  bool done = false;
  while(!done) {
    checkGraphs();
    Socket *sg;
    done=true;
    for (int i=0; i<port->socks.size(); i++) {
      sg = port->socks[i];
      if (sg->type == TYPE_GRAPH && sg->ctx->busy) {
	done=false; break;
      }
    }
    if (!done) {
      sg = port->WaitAvailableGraph();
      checkReturnedProbs();
    }
  }

  // set perplexity
  double avelogprob = sumlogprob / totalE;
  perplexity = pow(2.0,-avelogprob);

#if 1
  cout << "All graphs are finished!\n";
  cerr << "All graphs are finished!\n"; cerr.flush();
#endif

  // update model
  if (!testMode) {
    port->SendAll(FUNC_UPDATE, TYPE_TABLE);// it will clear count too
    port->WaitAll(FUNC_ACK, TYPE_TABLE); // wait all table returns ACK
  }
}

int
main(int argc, char* argv[])
{
  Params *prm = new Params(argc,argv);

#if DEBUG
  cout << "debug option enabled\n";
#endif  

  // open master port
  cout << "master port\n";
  Port *port = new Port(prm);
  port->AllowIncoming(PORT_MASTER);
  cout << "opened \n";
  cout.flush();

  // wait all tables and graphs are connected
  port->EstablishAllMasterConnections();
  cout << "all connection established\n";

  Train *trn = new Train(prm,port);

  for (int i=0; i<prm->NumIterate; i++) {
    cout << "***** start itereration " << i << " *****\n";
#if 0
    trn->Iterate();
    double sumlogprob,perplexity;
    int totalE,npairs,ngs;
    trn->GetResults(totalE,sumlogprob,perplexity,npairs,ngs);
    cout << "\n#Pairs=" << npairs << ",#NG=" << ngs << ",";
    cout << "#EWords=" << totalE << ",sumlogprob=" << sumlogprob
	 << ",peplexity=" << perplexity << "\n";
    //mdl->Update();		// it will clear count too.
    //sleep(10);
#else
    const bool doTrain = false;
    const bool doTest = true;

    // obtain test perplexity
    if (prm->TestFile.size()>0) {
      trn->Iterate(doTest);
      trn->ShowResults("Test:");
    }
    // run iteration on training corpus
    trn->Iterate(doTrain);
    trn->ShowResults("Train:");
#endif
  }

  // request table dump
  port->SendAll(FUNC_DUMP, TYPE_TABLE);
  port->WaitAll(FUNC_ACK, TYPE_TABLE); // wait all tables finish table dump

  // shutdown servers
  port->SendAll(FUNC_SHUTDOWN);
  ShowMemory();
}

