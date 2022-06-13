
// Graph Server
//   (actually a client to the master)

#include "gserver.h"

// for sleep(), getpid()
extern "C" {
#include <unistd.h>
}

void
Vocab::LoadVocab(string& vocFile, int maxId)
{
  if (vocFile.size()>0) {
    string fname = vocFile;
    ifstream in(fname.c_str());
    if (!in) error_exit("can't open vocab file [" + fname + "]");

    string line;
    while(getline(in,line)) {
      vector<string> wds;
      split(line,wds);		// format: id word freq
      int id = atoi(wds[0].c_str());
      if (id<=maxId) {
	string word = wds[1];
	vocTable[id] = word;
      }
    }
  }
}

void
Vocab::word2str(Word w, string& str)
{
  str = vocTable[w];
  if (str.size()==0) str = "???";
}

void
Vocab::words2strs(vector<Word>& words, vector<string>& strs)
{
  for (int i=0; i<words.size(); i++) {
    string str;
    word2str(words[i],str);
    strs.push_back(str);
  }
}

TableInfo::TableInfo()
{
  // initialize members
  sc.type = TYPE_TABLE;
  //sc.ctx = new Context();   // sc.ctx will be allocated by ProcessTableInfo()
  hostname = "";
  requesting = false;
}

GServer::GServer(Params *param, Socket *master)
  : param(param), master(master)
{
  // not to connect to table servers now
  // tableInfo (tables[]) will be allocated and init'd by processTableInfo()
  numTables = 0;
  tablesConnected = false;

  // if vocab files are specified in params,
  // then set vocE,vocF and symTbl here.
  vocE.LoadVocab(param->VocabEFile,param->MaxVocabId);
  vocF.LoadVocab(param->VocabFFile,param->MaxVocabId);
  symTbl.LoadVocab(param->SymTblFile,param->MaxVocabId);
}

void
GServer::waitAllProbs()
{
  // wait all table server returns prob or ACK
  bool done=false;
  while(!done) {
    done=true;
    // check all requests have done
    for (int i=0; i<numTables; i++)
      if (tables[i].requesting) {
	done=false; break;
      }

    if (!done) {
      // wait packet from table servers

      // set readfds
      fd_set readfds;
      FD_ZERO(&readfds);
      for (int i=0; i<numTables; i++)
	if (tables[i].requesting) FD_SET(tables[i].sc.fd,&readfds);
      
      // select
      int idx = select(FD_SETSIZE, &readfds, NULL, NULL, NULL);
      if (idx<0) perror_exit("select");

      // receive packet
      for (int i=0; i<numTables; i++) {
	Socket& st = tables[i].sc;
	if (FD_ISSET(st.fd, &readfds)) {

	  // buffers for packet unpacking
	  vector<int>& poslenEFArr = st.ctx->poslenEFArr;
	  poslenEFArr.clear();
	  vector<int> numcArr;
	  vector<double> probArr;

	  st.RecvPacket();
	  switch(st.funcid) {
	  case FUNC_NULL:
	    // disconnect
	    error_exit("table server disconnect before return prob ", st.fd);

	  case FUNC_SENDPROB:
#if 0
	    cout << "Tbl-" << i << ":";
#endif	    
	    st.UnpackSendProb(poslenEFArr,probArr);
	    tpair->graph->CacheProbs(poslenEFArr,probArr);
	    break;

	  case FUNC_SENDPROB_RN:
	    st.UnpackSendProbRN(numcArr,probArr);
	    tpair->graph->CacheProbsRN(numcArr,probArr);
	    break;

	  case FUNC_ACK:
	    // ack returned after SENDCOUNT
	    // just need to set tableRequesting flag to off.
	    break;

	  case FUNC_ERROR:
	    cout << "ERROR packet from table " << i << " in GServer::waitAllProbs()\n";
	    cout.flush();
	    break;

	  default:
	    error_exit("unexpected return from table", st.funcid);
	  }

	  // clear requesting flag
	  tables[i].requesting = false;
	}
      }
    }
  }
}

Socket::BufPtr
GServer::packReqProb(Socket *st, vector<int>& poslenArr, int& pktLen)
{
  // allocate REQPROB packet and fill it.
  // REQPROB packet: arrSize(1) eposlenArr(arrSize) 
  //                 elen(1) sentE(elen*2) flen(1) sentF(flen*2)

  int arrSize = poslenArr.size();
  int elen = tpair->wordE.size();
  int flen = tpair->wordF.size();
  pktLen = 1 + arrSize + 1 + elen*2 + 1 + flen*2;

  //cout << "PackReqProb: arrSize=" << arrSize << "\n";

  Socket::BufPtr buff = (Socket::BufPtr)malloc(pktLen); // must be freed by a caller
  Socket::BufPtr pt = buff;

  // pack arrSize
  st->PackChar(pt,arrSize);

  // pack poslenArr
  for (int i=0; i<arrSize; i++)
    st->PackChar(pt,poslenArr[i]);

  // pack sentE
  st->PackChar(pt,elen);
  for (int i=0; i<elen; i++) {
    Word w = tpair->wordE[i];
    st->PackShort(pt,w);
  }

  // pack sentF
  st->PackChar(pt,flen);
  for (int i=0; i<flen; i++) {
    Word w = tpair->wordF[i];
    st->PackShort(pt,w);
  }

  if (buff + pktLen != pt) error_exit("inconsistent pktLen in packReqProb()");
  return buff;
}

Socket::BufPtr
GServer::packReqProbRN(Socket* sr, vector<int>& numcArr, vector<int>& symArr,
		       int& pktLen)
{
  // allocate REQPROB_RN packet and fill it
  // REQPROB_RN packet: 
  //     numNode(1) [numc(1) parent(1) childLabel(numc)](*numNode)
  //     flen(1) sentF(flen*2) <= add

  // obtain pktLen
  int numNode = numcArr.size();
  int flen = tpair->wordF.size();
  pktLen = 1;
  for (int i=0; i<numNode; i++) {
    int numc = numcArr[i];
    pktLen += (1 + 1 + numc);
  }
  pktLen += (1 + flen * 2);

  Socket::BufPtr buff = (Socket::BufPtr)malloc(pktLen); // must be freed by a caller
  Socket::BufPtr pt = buff;

  // pack for each node
  sr->PackChar(pt,numNode);
  int symIdx=0;
  for (int i=0; i<numNode; i++) {
    int numc = numcArr[i];
    sr->PackChar(pt,numc);
    for (int j=0; j<numc+1; j++) { // +1 for parent
      int sym = symArr[symIdx++];
      sr->PackChar(pt,sym);
    }
  }
  if (symIdx != symArr.size()) error_exit("packReqProbRN inconsistent");

  // pack sentF
  sr->PackChar(pt,flen);
  for (int i=0; i<flen; i++) {
    Word w = tpair->wordF[i];
    sr->PackShort(pt,w);
  }

  if (buff + pktLen != pt) error_exit("inconsistent pktLen in packReqProbRN()");
  return buff;
}  

void
GServer::unpackGraphReq(int& pairId, string& lineE, string& lineF, bool& sendCountRequested)
{
  // unpack GRAPHREQ packet, store into lineE and lineF (and sendCountReqested)

  Socket::BufPtr pt=master->buf;
  int remlen = master->buflen;

#if 0
  // unpack lineE
  master->UnpackString(pt,lineE);
  totlen -= (sizeof(short) + lineE.size());

  // check next string size
  int nextlen = master->UnpackShort(pt);
  pt -= sizeof(short);		// to be reread by UnpackString()
  totlen -= (sizeof(short) + nextlen);
  if (totlen<0) error_exit("broken GRAPHREQ packet");

  // unpack lineF
  master->UnpackString(pt,lineF);
  if (totlen>0) error_exit("packet size mismatch in GRAPHREQ");
#else
  //char sendCntReqFlag;
  pairId = master->UnpackInt(pt,remlen);
  master->UnpackString(pt,lineE,remlen);
  master->UnpackString(pt,lineF,remlen);
  sendCountRequested = master->UnpackChar(pt,remlen);
  master->CheckPacketEnd(remlen);
#endif

}

void
GServer::ProcessTableInfo()
{
  // process FUND_TABLEINFO from master.
  // connect to table servers based on the info.

  Socket::BufPtr pt=master->buf;
  int remlen = master->buflen;
  numTables = master->UnpackShort(pt,remlen);
  tables = vector<TableInfo>(numTables); // allocate tables[numTables]
  for (int i=0; i<numTables; i++) {
    master->UnpackString(pt,tables[i].hostname,remlen);
    tables[i].sc.ctx = new Context();
  }
  master->CheckPacketEnd(remlen);

  // connect to tables
  for (int i=0; i<numTables; i++) {
    Socket& st = tables[i].sc;
    int portno = PORT_TABLE + i;
    if (tables[i].hostname=="") error_exit("tableHost not known: ",i);
#if 1
    cerr << "Connecting to Table: " << tables[i].hostname << ":" << portno << "\n";
    cerr.flush();
#endif
    st.Connect(tables[i].hostname, portno);
    st.ctx->tableIdx = i;	// mark socket which table is connected.
				// needed for unpacking returned probs.
  }
  tablesConnected = true;
}

void
GServer::ProcessTrainPair()
{
  // process train pair

  // ensure connection to tables
  if (!tablesConnected) error_exit("not connected to tables");

  // unpack GRAPHREQ
  int pairId;
  string lineE, lineF;
  bool testMode;
  unpackGraphReq(pairId,lineE,lineF,testMode);

#if 1
  cout << "got pair# " << pairId << "\n"; cout.flush();
#endif

#if 0
  cout << "lineE=[" << lineE << "],lineF=[" << lineF << "]\n"; cout.flush();
#endif

  // create TrainPair (tree and graph)
  tpair = new TrainPair(pairId,lineE,lineF,param);

  // backward compatibility for graph
  vector<string> esent,jsent,labels;
  vocE.words2strs(tpair->wordE,esent);
  vocF.words2strs(tpair->wordF,jsent);

  vector<Word> symbols;
  tpair->collectSymbols(tpair->troot,symbols);
  symTbl.words2strs(symbols,labels);
  tpair->graph->SetStringInfo(esent,jsent,labels);

  int lenE = tpair->wordE.size();
  double prob;

#if 0
  cout << "GServer::ProcessTrainPair()\n";
  cout << "lenE=" << lenE << ",lenF=" << tpair->wordF.size() << "\n";
#endif

  // skip the pair if lineF begins with '0' (manual mark for skipping)
  bool skipIt = (lineF.substr(0,1) == "0");

  if (skipIt) {
    cout << "skipping pair# " << pairId << "\n"; cout.flush();

    // real skipping is done below, when checking SentLengthOK
  }


  // print tree header (E/J sents) if necessary
  if (param->ShowTree) tpair->graph->ShowSents();

  // process graph if possible
  if (!skipIt && tpair->graph->SentLengthOK()) {

    // process graph and send counts
    prob=0;
    try {
      prob = processGraph(testMode);
    } catch(bad_alloc) {
      cout << "bad_alloc for pair# " << pairId << "\n"; cout.flush();
    } catch(...) {
      cout << "unknown error for pair# " << pairId << "\n"; cout.flush();
    }

    if (prob==0 || isnan(prob) || isinf(prob)) {
      // something strange happend
      int elen = tpair->wordE.size();
      int flen = tpair->wordF.size();

      cout << "Prob==" << prob << " !!"
	   << "  elen=" << elen << ",flen=" << flen << "\n";
      cout.flush();
    }
  } else {
    lenE=0; prob=0.0;
    
    // print dummy alignment, if necessary
    if (param->ShowTree) cout << "A: n/a\n\n\n";
  }

  // obtain memory/cpu info
  int memSize = MemSize();
  double cpuTime = UTime() - tpair->cpuTime;

#if 1
  cout << "done pair" << (testMode?"*":"#") << " " << tpair->pairId
       << " prob: " << prob 
       << " cpu: " << cpuTime << " mem: " << memSize << "\n";
  cout.flush();
#endif

#if DEBUG
  cout << "Sending: lenE=" << lenE << ",prob=" << prob << "\n";
#endif

  // send FUNC_GRAPHPROB to master
  // packet format: id(4) lenE(1) logprob(4) cpu(8) mem(4)

#if 1
  int pktLen = sizeof(int) + sizeof(char) + sizeof(double)
    + sizeof(double) + sizeof(int);
#else
  int pktLen = sizeof(char) + sizeof(double);
#endif
  Socket::BufPtr buff = (Socket::BufPtr)malloc(pktLen);
  Socket::BufPtr pt=buff;

  master->PackInt(pt,tpair->pairId);
  master->PackChar(pt,lenE);	// send lenE=0 if graph->SentLengthOK() returned false
  master->PackDouble(pt,prob);
  master->PackDouble(pt,cpuTime);
  master->PackInt(pt,memSize);
  master->SendPacket(FUNC_GRAPHPROB, buff, pktLen);
  free(buff);

  // delete TrainPair object (tree and graph)
  delete tpair;

  // debug
  cout.flush();
}

double
GServer::processGraph(bool testMode)
{
  double prob;			// return value

#if 0
  static int cnt;
  if (testMode) cnt=0;
  else {
    param->Verbose=1;
    cout << "[" << ++cnt << "]"; cout.flush();
    cout << "==========================\n";
  }
#endif

  // obtain probs from t-servers
  requestProbsForGraph();	// prob set in TNode/Graph

  // setup graph and set prob
  Graph *graph = tpair->graph;
  graph->AllocGraphNodes();	// generate graph structure
  graph->SetFloorProb(testMode); // add floor prob in TNode/Graph
  graph->SetArcProb();		// cached prob in TNode/Graph -> graph nodes
  graph->SetAlphaBeta();	// obtain probs
  prob = graph->GetTotalProb();
  graph->GetArcCount();		// event count -> cache
  if (param->ShowTree) graph->ShowBest();
  if (param->ShowPhraseProbs) graph->ShowPhraseProbs();
  if (param->ShowFertZero) graph->ShowFertZero();
  graph->DeallocGraphNodes();	// erase graph structure

  // send counts to t-servers
  if (!testMode) sendCountsForGraph(prob);

  // sleep if needed
  if (param->Sleep>0) sleep(param->Sleep);
  return prob;
}

void
GServer::requestProbsForGraph()
{
  // contact table servers to obtain probs
  vector<int> poslenAll;
  tpair->graph->CollectPhraseE(poslenAll); // collect all pos-len pair
  int numttbl = numTables-1;
  //cout << "numTTable=" << numttbl << "\n";
  for (int tidx=0; tidx<numttbl; tidx++) {

    // find pos-len which match this table index (tidx)
    vector<int> poslenArr;
    int idx2=0;
    for (int idx=0; idx<poslenAll.size(); idx+=2) {
      int epos = poslenAll[idx2++];
      int elen = poslenAll[idx2++];
      //cout << "mod=" << (tpair->wordE[epos] % numttbl) << ",tidx=" << tidx << "\n";
      if (tpair->wordE[epos] % numttbl == tidx) {
	//cout << "poslenArr.push: epos=" << epos << ",elen=" << elen << "\n";
	poslenArr.push_back(epos);
	poslenArr.push_back(elen);
      }
    }
    
    // need to clear poslenEFArr in ALL context
    // (note: since FlushCounts() decides which pos/len to send counts, and
    //  some tables might not be used)
    Socket& st = tables[tidx+1].sc;
    st.ctx->poslenEFArr.clear();

    if (poslenArr.size()>0) {
      // make REQPROB packet and send it
      // note: tidx must be offset by 1 (use tbl-1 for tidx==0)
      int pktLen;
      Socket::BufPtr buff = packReqProb(&st,poslenArr,pktLen);
      st.SendPacket(FUNC_REQPROB, buff, pktLen);
      free(buff);
      tables[tidx+1].requesting = true;
    }
  }

  // ProbRN
  {
    Socket& sr = tables[0].sc;
    vector<int> numcArr, symArr; int pktLen;
    tpair->CollectRules(numcArr,symArr);
    Socket::BufPtr buff = packReqProbRN(&sr,numcArr,symArr,pktLen);
    sr.SendPacket(FUNC_REQPROB_RN, buff, pktLen);
    free(buff);
    tables[0].requesting = true;
  }

  // wait all table servers responded
  waitAllProbs();		// also unpack and CacheProbs()
  
}

void
GServer::sendCountsForGraph(double prob)
{  
  // send counts
  int pktLen;
  Socket::BufPtr buff;
  Graph *graph = tpair->graph;
  
  // countT
  for (int i=1; i<numTables; i++) {
    Socket& st = tables[i].sc;
    
    // buffers for packing packets
    vector<int>& poslenEFArr = st.ctx->poslenEFArr; // use seved one
    vector<double> cntArr;

    if (poslenEFArr.size()>0) {
#if 0
      cout << "Tbl-" << i << ":";
#endif
      graph->FlushCounts(prob,poslenEFArr,cntArr);
      buff = st.PackSendProb(poslenEFArr,cntArr,pktLen);
      st.SendPacket(FUNC_SENDCOUNT,buff,pktLen);
      free(buff);
      tables[i].requesting = true;
    }
  }
  
  // countRN
  Socket& st = tables[0].sc;
  vector<double> cntArr; vector<int> numcArr; int flen;
  graph->FlushCountsRN(prob,numcArr,cntArr,flen);
  buff = st.PackSendProbRN(flen,numcArr,cntArr,pktLen);
  st.SendPacket(FUNC_SENDCOUNT_RN,buff,pktLen);
  free(buff);
  tables[0].requesting = true;
  
  // wait ACK
  // (not necessarry needed, but required before master sends UPDATE)
  waitAllProbs();
  
}


int
main(int argc, char* argv[])
{
  Params *prm = new Params(argc,argv);

  // connect to master
  Socket *master = new Socket(TYPE_MASTER);
  master->Connect(prm->HostMaster, PORT_MASTER);

  // sendID (ID=1) to master
  master->SendID(1);		// 1 for graph

  // create GServer (not to connect to table servers now)
  GServer *gtbl = new GServer(prm,master);

  // process each training pair given from master
  bool done = false;
  while(!done) {
    master->RecvPacket();
    switch(master->funcid) {
    case FUNC_NULL:
      cout << "disconnected from master\n";
    case FUNC_SHUTDOWN:
      cout << "shutdown from master\n";
      done=true; break;
    case FUNC_TABLEINFO:
      gtbl->ProcessTableInfo();	// connect to table servers
      break;
    case FUNC_GRAPHREQ:
      gtbl->ProcessTrainPair();	// create graph, contact table servers and
				// send FUNC_GRAPHPROB to master
      break;
    default:
      cerr << "unexpected packet from master: " << int(master->funcid) << "\n";
    }
  }
      
  // close all sockets (by process rundown)

  // exit
  ShowMemory();
  exit(0);
}

