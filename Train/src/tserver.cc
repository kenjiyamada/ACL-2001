#include "tserver.h"

//
// TServer
//

TServer::TServer(Params *prm)
  : param(prm)
{

  // alocate Table (not loading files)
  int idx = prm->TableIdx;
  if (idx>0)
    ttbl = new TTable(prm);
  else
    rtbl = new RNTable(prm);
}

void
TServer::ConnectMaster()
{
  master = new Socket(TYPE_MASTER);
  master->Connect(param->HostMaster, PORT_MASTER);
}  

void
TServer::AllowIncoming()
{
  port = new Port(param);
  int portno = PORT_TABLE + param->TableIdx;
  port->AllowIncoming(portno);
}  

void
TServer::LoadFile()
{
  isTTable() ? ttbl->LoadFile() : rtbl->LoadFile();
}

void
TServer::UpdateProbs()
{
  cout << "update requested\n";
  cout.flush();

  isTTable() ? ttbl->UpdateProbs() : rtbl->UpdateProbs();
  master->SendACK();
}

void
TServer::DumpTables()
{
  cout << "dump requested\n";

  isTTable() ? ttbl->DumpTables() : rtbl->DumpTables();
  master->SendACK();
}

void
TServer::SendID()
{
  int portno = PORT_TABLE + param->TableIdx;
  master->SendID(portno);
}

void
TServer::ClearCounts()
{
  // clear counts is automatically done when loading or updating.
  cout << "TServer::ClearCount() is not implemented";
}  

//
// T-Table REQPROB/SENDCOUNT handling
//

void
TServer::processRequestProbs(Socket *sg)
{
#if DEBUG
  cout << "prob request from " << sg->fd << "\n";
#endif

  // unpack REQPROB

  // unpacked sentE and sentF will be stored in sg->ctx.
  Context *ctx = sg->ctx;
  ctx->sentE.clear(); ctx->sentF.clear();
  vector<int> eposlenArr;
  unpackReqProb(sg,eposlenArr);

  // return probs
  vector<double> probArr;
  vector<int> poslenEFArr;	// not saved in ctx. (gserver will save it, though)

  // fill arrays (set poslenEFArr and probArr)
  fillSendProb(eposlenArr,poslenEFArr,probArr,ctx->sentE,ctx->sentF);

  // pack SENDPROB packet
  int pktLen;
  Socket::BufPtr buff = sg->PackSendProb(poslenEFArr,probArr,pktLen);
  sg->SendPacket(FUNC_SENDPROB, buff, pktLen);
  free(buff);
}

void
TServer::unpackReqProb(Socket *sg, vector<int>& poslenArr)
{
  // use socket context to store the unpacked info
  vector<Word>& sentE = sg->ctx->sentE;
  vector<Word>& sentF = sg->ctx->sentF;

  // unpack REQPROB
  Socket::BufPtr pt = sg->buf;
  int remlen = sg->buflen;

  // unpack arrSize
  int arrSize = sg->UnpackChar(pt,remlen);

  // unpack poslenArr
  for (int i=0; i<arrSize; i++)
    poslenArr.push_back(sg->UnpackChar(pt,remlen));

  // unpack sentE, sentF
  int sentlenE = sg->UnpackChar(pt,remlen);
  for (int i=0; i<sentlenE; i++)
    sentE.push_back(sg->UnpackShort(pt,remlen));
  int sentlenF = sg->UnpackChar(pt,remlen);
  for (int i=0; i<sentlenF; i++)
    sentF.push_back(sg->UnpackShort(pt,remlen));

  // check all packet is read
  sg->CheckPacketEnd(remlen);
}

void
TServer::fillSendProb(vector<int>& eposlenArr, vector<int>& poslenEFArr,
		      vector<double>& probArr, vector<Word>& sentE, vector<Word>& sentF)
{
  // input: eposlenArr, sentE, sentF
  // output: poslenEFAr, probArr

  int arrSize = eposlenArr.size();
  //cout << "FillSendProb: arrSize=" << arrSize << "\n";
  int idx2=0;
  for (int idx=0; idx<arrSize; idx+=2) {

    // set epos,elen,minF,maxF
    int epos = eposlenArr[idx2++];
    int elen = eposlenArr[idx2++];
    int minF,maxF;
    getValidFLen(elen,minF,maxF);
    if (elen==1) minF=0;	// allow NULL wordF
    if (maxF>sentF.size()) maxF = sentF.size();

    // make eword
    vector<Word> eword;
    for (int j=0; j<elen; j++) eword.push_back(sentE[epos+j]);

    // for each possible fword
    for (int fpos=0; fpos<sentF.size(); fpos++) {
      for (int flen=minF; flen<=maxF && fpos+flen<=sentF.size(); flen++) {
	// make fword
	vector<Word> fword;
	for (int j=0; j<flen; j++) fword.push_back(sentF[fpos+j]);
	// get prob
	double prob = ttbl->GetProb(eword,fword);

	// push poslenEFArr and probArr
	if (prob>0) {
	  poslenEFArr.push_back(epos);
	  poslenEFArr.push_back(elen);
	  poslenEFArr.push_back(fpos);
	  poslenEFArr.push_back(flen);
	  probArr.push_back(prob);
	}
      }
    }

    // set lambda1 probs
    if (elen>1 && param->LambdaPhraseVary) {
      double l1prob = ttbl->GetLambda1Prob(eword);
      if (l1prob>0) {
	poslenEFArr.push_back(epos);
	poslenEFArr.push_back(elen);
	poslenEFArr.push_back(FPOS_LAMBDA1);
	poslenEFArr.push_back(FLEN_L1);
	probArr.push_back(l1prob);

	// need to send 1-lambda1, since g-server returns count for this
	poslenEFArr.push_back(epos);
	poslenEFArr.push_back(elen);
	poslenEFArr.push_back(FPOS_LAMBDA1);
	poslenEFArr.push_back(FLEN_L1N);
	probArr.push_back(1-l1prob);
      }
    }

  } // end of for (int idx=0...)
  //cout << "FillSendProb: nprob=" << probArr.size() << "\n";
}

//
// RN-Table REQPROB/SENDCOUNT handling
//

void
TServer::processRequestProbsRN(Socket *sg)
{
#if DEBUG
  cout << "probRN request from " << sg->fd << "\n";
#endif
  
  // unpack REQPROB_RN

  // unpacked numcArr,symArr,sentF will be stored in sg->ctx
  vector<int> numcArr;
  sg->ctx->symArr.clear();
  sg->ctx->sentF.clear();
  unpackReqProbRN(sg,numcArr);

  // return probs
  vector<double> probArr;

  // fill arrays
  fillSendProbRN(numcArr,sg->ctx->symArr,probArr,sg->ctx->sentF);

  // pack SENDPROB_RN packet
  int pktLen;
  int flen = sg->ctx->sentF.size();
  Socket::BufPtr buff = sg->PackSendProbRN(flen,numcArr,probArr,pktLen);
  sg->SendPacket(FUNC_SENDPROB_RN, buff, pktLen);
  free(buff);
}

void
TServer::unpackReqProbRN(Socket* sg, vector<int>& numcArr)
{
  // use socket context to store the unpacked info
  vector<int>& symArr = sg->ctx->symArr;
  vector<Word>& sentF = sg->ctx->sentF;

  // unpack REQPROB_RN
  Socket::BufPtr pt = sg->buf;
  int remlen = sg->buflen;

  // unpack numNode
  int numNode = sg->UnpackChar(pt,remlen);

  // unpack each rule
  for (int i=0; i<numNode; i++) {
    int numc = sg->UnpackChar(pt,remlen);
    numcArr.push_back(numc);
    for (int j=0; j<numc+1; j++) // +1 for parent
      symArr.push_back(sg->UnpackChar(pt,remlen));
  }

  // unpack sentF
  int sentlenF = sg->UnpackChar(pt,remlen);
  for (int i=0; i<sentlenF; i++)
    sentF.push_back(sg->UnpackShort(pt,remlen));

  // check all packet is read
  sg->CheckPacketEnd(remlen);
}
      
void
TServer::fillSendProbRN(vector<int>& numcArr, vector<int>& symArr,
			vector<double>& probArr, vector<Word>& sentF)
{
  // fill probArr

  // push probR for each nodes
  int numNodes = numcArr.size();
  int symIdx = 0;
  for (int i=0; i<numNodes; i++) {
    int numc = numcArr[i];
    // make rule
    vector<Word> rule;
    for (int j=0; j<numc+1; j++) { // +1 for parent
      int sym = symArr[symIdx++];
      rule.push_back(sym);
    }
    // get probs
    rtbl->GetProbR(rule,probArr);	// append probArr
  }

  // get ProbN
    
  // make TOP->X rule
  int topSym = symArr[0];
  vector<Word> toprule;
  toprule.push_back(0);		// TOP
  toprule.push_back(topSym);
    
  // get topIns prob
  rtbl->GetProbN(toprule,sentF,probArr);
  
  // get insProb for each node
  symIdx = 0;
  for (int i=0; i<numNodes; i++) {
    int numc = numcArr[i];
    // make rule
    vector<Word> rule;
    for (int j=0; j<numc+1; j++) { // +1 for parent
      int sym = symArr[symIdx++];
      rule.push_back(sym);
    }
    // get probs
    rtbl->GetProbN(rule,sentF,probArr);
  }
}

void
TServer::processSendCounts(Socket *sg)
{
#if DEBUG
  cout << "count sent from " << sg->fd << "\n";
#endif

  // unpack SENDCOUNT

  // buffer for unpacking
  vector<int> poslenEFArr;	// not use in context (only g-server uses context)
  vector<double> cntArr;
  vector<Word>& sentE = sg->ctx->sentE;
  vector<Word>& sentF = sg->ctx->sentF;

  sg->UnpackSendProb(poslenEFArr,cntArr);
  addCounts(poslenEFArr,cntArr,sentE,sentF);

  // send ACK to gserver
  // not neccessary needed, but required before master sends UPDATE
  sg->SendPacket(FUNC_ACK, NULL, 0);
}

void
TServer::addCounts(vector<int>& poslenEFArr, vector<double>& cntArr,
		   vector<Word>& sentE, vector<Word>& sentF)
{
  // reverse of fillSendProb()
  // similar to Graph::CacheProbs() for unpacking

  int ncnt = cntArr.size();
  int idx4=0;
  for (int idx=0; idx<ncnt; idx++) {
    int epos = poslenEFArr[idx4++];
    int elen = poslenEFArr[idx4++];
    int fpos = poslenEFArr[idx4++];
    int flen = poslenEFArr[idx4++];
    double count = cntArr[idx];

    // make eword
    vector<Word> eword;
    for (int j=0; j<elen; j++) eword.push_back(sentE[epos+j]);

    if (fpos==FPOS_LAMBDA1) {
      if (flen==FLEN_L1) ttbl->AddLambda1Count(eword,count);
      if (flen==FLEN_L1N) ttbl->AddLambda1nCount(eword,count);
    } else {      

      // make fword
      vector<Word> fword;
      for (int j=0; j<flen; j++) fword.push_back(sentF[fpos+j]);
      
      // add count
      ttbl->AddCount(eword,fword,count);
    }
  }
}

void
TServer::processSendCountsRN(Socket *sg)
{
#if DEBUG
  cout << "count sent from " << sg->fd << "\n";
#endif

  // unpack SENDCOUNT_RN

  // buffer for unpacking
  vector<int> numcArr;
  vector<int>& symArr = sg->ctx->symArr;
  vector<double> cntArr;

  sg->UnpackSendProbRN(numcArr, cntArr);
  addCountsRN(numcArr,symArr,cntArr,sg->ctx->sentF);

  // send ACK to gserver
  // not neccessary needed, but required before master sends UPDATE
  sg->SendPacket(FUNC_ACK, NULL, 0);
}

void
TServer::addCountsRN(vector<int>& numcArr, vector<int>& symArr,
		     vector<double>& cntArr, vector<Word>& sentF)
{
  // reverse of fillSendProbRN()
  // similar to Graph::CacheProbsRN() for unpacking

  int numNodes = numcArr.size();
  int symIdx = 0, cntIdx = 0;

  // for each node
  for (int i=0; i<numNodes; i++) {
    int numc = numcArr[i];
    int numr = factorial(numc);

    // make rule
    vector<Word> rule;
    for (int j=0; j<numc+1; j++) { // +1 for parent
      int sym = symArr[symIdx++];
      rule.push_back(sym);
    }

    // CountR
    vector<double> ord;
    for (int j=0; j<numr; j++) {
      double cnt = cntArr[cntIdx++];
      ord.push_back(cnt);
    }

    // addCountR
    rtbl->AddCountR(rule,ord);
  }

  // topIns and NULL-F counts

  // make TOP->X rule
  int topSym = symArr[0];
  vector<Word> toprule;
  toprule.push_back(0);		// 0=TOP
  toprule.push_back(topSym);

  int numcnt = 1 + sentF.size() * 2;

  // put topIns counts
  vector<double> topcnt;
  for (int i=0; i<numcnt; i++)
    topcnt.push_back(cntArr[cntIdx++]);
  rtbl->AddCountN(toprule,sentF,topcnt);
  
  // for each node
  symIdx=0;			// rewind symArr[]
  for (int i=0; i<numNodes; i++) {
    int numc = numcArr[i];
    
    // make rule
    vector<Word> rule;
    for (int j=0; j<numc+1; j++) { // +1 for parent
      int sym = symArr[symIdx++];
      rule.push_back(sym);
    }
    
    // put counts
    vector<double> cnts;
    for (int j=0; j<numcnt*numc; j++)
      cnts.push_back(cntArr[cntIdx++]);
    rtbl->AddCountN(rule,sentF,cnts);
  }
}

void
TServer::MainLoop()
{
  // wait for request coming to port (or from master)
  // and process them on ttable.
  // will exit on close from master.

  bool done = false;
  while(!done) {
    // set readfds
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(master->fd, &readfds);
    FD_SET(port->s0, &readfds);
    for (int i=0; i<port->socks.size(); i++)
      FD_SET(port->socks[i]->fd, &readfds);

    // select
    int idx = select(FD_SETSIZE, &readfds, NULL, NULL, NULL);
    if (idx<0) perror_exit("select");

    // check msg from master
    if (FD_ISSET(master->fd, &readfds)) {
      master->RecvPacket();
      switch (master->funcid) {
      case FUNC_NULL:
	cout << "disconnected from master\n";
	done=true; break;
      case FUNC_SHUTDOWN:
	cout << "shutdown from master\n";
	done=true; break;
      case FUNC_CLEAR:
	ClearCounts(); break;
      case FUNC_UPDATE:
	UpdateProbs(); break;
      case FUNC_DUMP:
	DumpTables(); break;
	done=true; break;
      default:
	cerr << "unexpected packet from master: " << int(master->funcid) << "\n";
      }
    }
    if (done) break;

    // check already connected sockets (from graph)
    for (int i=0; i<port->socks.size(); i++) {
      if (FD_ISSET(port->socks[i]->fd, &readfds)) {
	Socket *sg = port->socks[i];
	sg->RecvPacket();
	switch (sg->funcid) {
	case FUNC_NULL:
	  // disconnect
	  cout << "graph(" << sg->fd << ") disconnected\n";
	  // close(sg->fd);
	  delete port->socks[i];
	  port->socks.erase(port->socks.begin()+i);
	  i--;
	  break;
	case FUNC_REQPROB:
	  processRequestProbs(sg);
	  break;
	case FUNC_SENDCOUNT:
	  processSendCounts(sg);
	  break;
	case FUNC_REQPROB_RN:
	  processRequestProbsRN(sg);
	  break;
	case FUNC_SENDCOUNT_RN:
	  processSendCountsRN(sg);
	  break;
	case FUNC_ERROR:
	  cout << "ERROR packet from graph(" << sg->fd << ")\n";
	  cout.flush();
	  break;
	default:
	  cerr << "unexpected packet from graph(" << sg->fd << ")\n";
	  cout << "unexpected packet from graph(" << sg->fd << ")\n"; cout.flush();
	}
      }
    }

    // check new connection (from graph)
    if (FD_ISSET(port->s0, &readfds)) {
      Socket *sn = new Socket(TYPE_GRAPH);
      sn->ctx = new Context();	// will use sentE,numcArr, etc
      socklen_t len = sizeof(port->caddr);
      sn->fd = accept(port->s0, (struct sockaddr *)&(port->caddr), &len);
      port->socks.push_back(sn);
      cout << "new graph(" << sn->fd << ") connected\n";
    }
  }

  // shutdown server

  // close() will be done by process terminater automatically...
}

int
main(int argc, char* argv[])
{
  Params *prm = new Params(argc,argv);
  TServer *srv = new TServer(prm);

  // connect to master
  srv->ConnectMaster();

  // Identify itself
#if 0
  // not sure if it really works
  sprintf(argv[0], "tbl-%d", prm->TableIdx);
#endif

  // create incoming Port
  srv->AllowIncoming();

  // read data file
  srv->LoadFile();			// also set probs to uniform (if necessary)

  // declare ready for serving
  srv->SendID();

  // mainloop
  srv->MainLoop();

  ShowMemory();
  exit(0);
}
