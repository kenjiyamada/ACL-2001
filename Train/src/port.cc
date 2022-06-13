#include "port.h"

// for sleep(), getpid()
extern "C" {
#include <unistd.h>
}

Socket::Socket(int ty=TYPE_UNKNOWN)
  : type(ty)
{
  // init receive buffer
  buf=0;
  buflen=alloclen=0;

  // init context (allocated if needed)
  ctx=NULL;

}  

// Packet format: FuncID(2) BodyLen(2) Body(nn)

void
Socket::SendPacket(int funcId, char *buff, int len)
{
  const int MaxLength = 65000;
  int s;

  // length must be shown in 2-bytes
  if (len>MaxLength) error_exit("packet too long",len);
  
#if DEBUG
  cout << "Send(" << getpid() << "): FuncID=" << funcId << ", BodyLen=" << len << "\n";
  cout.flush();
#endif

  // FuncID
  short header = htons(funcId);
  s = write(fd, (char *)&header, sizeof(short));
  if (s<0) perror("SendPacket: FuncID");

  // BodyLen
  header = htons(len);
  s = write(fd, (char *)&header, sizeof(short));
  if (s<0) perror("SendPacket: BodyLen");

  // Body
  if (len>0) {
    s = write(fd, buff, len);
    if (s<0) perror("SendPacket: Body");
  }

}

void
Socket::RecvPacket()
{
  // read FuncID
  short fid;
  int len = read(fd, &fid, sizeof(short));
  if (len<0) {
    perror("RecvPacket: FuncID");
    cout << "RecvPacket: FuncID\n"; cout.flush();
    funcid = FUNC_ERROR;
    return;
  }
  
  if (len==0) {
    funcid = 0;
    buflen = 0;
  } else {
    funcid = ntohs(fid);

    // read BodyLen
    short blen;
    len = read(fd, &blen, sizeof(short));
    if (len<0) perror("RecvPacket: BodyLen");
    buflen = len ? ntohs(blen) : 0;
#if DEBUG
    cout << "Recv(" << getpid() << "): FuncId=" << funcid << ",BodyLen=" << buflen << " ";
    char *ct = (char *)&blen;
    //cout << "[" << int(*ct) << "][" << int(*(ct+1)) << "]";
    cout << "\n";
#endif

    // alloc buffer
    if (buflen>alloclen) {
      if (buf) free(buf);
      buf = (char *)malloc(buflen);
      alloclen = buflen;
    }      

    // fill buffer 
    char *pt = buf;
    int nbytes = buflen;
    while(nbytes>0) {
      len = read(fd, pt, nbytes);
      if (len==0) {
	warn("broken packet");
	return;
      }
      pt += len;
      nbytes -= len;
    }
    //cout << "Recv: Body = ";
    //for (int i=0; i<buflen; i++) cout << "[" << int(buf[i]) << "]";
    //cout << "\n";
#if DEBUG
    if (pt - buf > alloclen) error_exit("RecvPacket overflow");
#endif
  }
  //return buflen;
}

// Methods for clients

void
Socket::Connect(string host, int portno)
{
  // connect to server on portno

  // create socket
  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd<0) perror_exit("socket");

  // initialize addr
  struct sockaddr_in addr;
  bzero((char *)&addr, sizeof(addr));

  // get hostname
  struct hostent *hp;
  hp = gethostbyname(host.c_str());
  if (!hp) perror_exit("no such host");

  // set addr
#if 0
  bcopy(hp->h_addr, &addr.sin_addr, hp->h_length);
#else
  // gethostbyname() returns 4-byte address in hp, which equals sin_addr.s_addr
  addr.sin_addr.s_addr = *(unsigned long *)hp->h_addr_list[0]; // after pppctl.c
#endif
  addr.sin_family = AF_INET;
  addr.sin_port = htons(portno);

  // connect
#if 1
  int stat = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
  if (stat<0) {
    for (int i=0; i<10; i++) {
      cerr << "Retrying (" << i << ")\n"; cerr.flush();
      sleep(1);
      stat = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
      if (stat>=0) break;
    }
    if (stat<0) perror_exit("connect");
  }
#else
  int stat = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
  if (stat<0) perror_exit("connect");
#endif

}

void
Socket::SendID(int id)
{
  // send client ID 
  // for table server: portno (>1000)
  // for graph server: 1-1000

#if 0
  short idbuf = htons(id);
  SendPacket(FUNC_SENDID, (char *)&idbuf, sizeof(idbuf));
#else
  // test for PackShort()
  BufPtr pt0 = (BufPtr)malloc(sizeof(short));
  BufPtr pt = pt0;

  PackShort(pt,id);
  SendPacket(FUNC_SENDID, pt0, sizeof(short));
  free(pt0);
#endif
  
}

void
Socket::SendACK()
{
  // send back ACK
  // (used by t-server to send ACK after doing Update()

  SendPacket(FUNC_ACK, NULL, 0);
}

//
// Prob/Count table packing and unpacking (for tserver and gserver)
//

Socket::BufPtr 
Socket::PackSendProb(vector<int>& poslenEFArr, vector<double>& probArr, int& pktLen)
{
  // allocate SENDPROB packet and fill it.
  // SENDPROB packet: nprob(2) [posE(1) lenE(1) posF(1) lenF(1) prob(8)] * nprob

  // Input: poslenEFArr,probArr
  // Output: pktLen, buffptr(return value)

  int nprob = probArr.size();
  pktLen = 2 + (4 + sizeof(double)) * nprob;

  BufPtr buff = (BufPtr)malloc(pktLen); // must be freed by a caller
  BufPtr pt = buff;

  // pack nprob
  PackShort(pt,nprob);

  // pack body
  int idx4=0;
  for (int idx=0; idx<nprob; idx++) {
    for (int j=0; j<4; j++)  PackChar(pt,poslenEFArr[idx4++]);
    PackDouble(pt,probArr[idx]);
  }
  if (idx4 != nprob*4) error_exit("packSendProb inconsistent");

  if (buff + pktLen != pt) error_exit("inconsistent pktLen in PackSendProb()");
  return buff;
}

void
Socket::UnpackSendProb(vector<int>& poslenEFArr, vector<double>& probArr)
{
  // unpack SENDPROB into poslenEFArr and probArr
  //   Inupt:  none
  //   Output: poslenEFArr, probArr (poslenEFArr will be saved if it's g-server)
  //                                (need to be cleared before calling this)

  // unpack SENDPROB packet
  BufPtr pt = buf;
  int remlen = buflen;

  // unpack nprob
  int nprob = UnpackShort(pt,remlen);
  //cout << "UnpackSendProb: nprob=" << nprob << "\n";

  // unpack body
  for (int i=0; i<nprob; i++) {
    for (int j=0; j<4; j++) 
      poslenEFArr.push_back(UnpackChar(pt,remlen));
    probArr.push_back(UnpackDouble(pt,remlen));
  }

  CheckPacketEnd(remlen);
}

Socket::BufPtr
Socket::PackSendProbRN(int flen, vector<int>& numcArr,
		       vector<double>& probArr, int& pktLen)
{
  // allocate SENDPROB_RN packet and fill it
  // SENDPROB_RN packet:  (numr = numc!)
  //       numNode(1) [ numc(1) prob(numr*8) ] (* numNode)
  //       flen(1) prob(inslen*8)
  //             where inslen = (1+flen*2) * [1+sum(numc)]
  //                             ^            ^
  //                             for no-ins   for top-node

  //    Input:  flen, numcArr (numcArr is usually stored in context)
  //    Output: probArr, pktLen

  // obtain pktLen
  int numNode = numcArr.size();
  pktLen = 1;
  int sumNumc=0;
  for (int i=0; i<numNode; i++) {
    int numc = numcArr[i]; sumNumc += numc;
    int numr = factorial(numc);
    pktLen += (1 + numr * sizeof(double));
  }
  int inslen = (1+flen*2)*(1+sumNumc);
  pktLen += (1 + inslen * sizeof(double));

  Socket::BufPtr buff = (Socket::BufPtr)malloc(pktLen); // must be freed by a caller
  Socket::BufPtr pt = buff;

  // pack numNode
  PackChar(pt,numNode);
  
  // pack for each node
  int probIdx = 0;
  for (int i=0; i<numNode; i++) {
    int numc = numcArr[i];
    PackChar(pt,numc);
    int numr = factorial(numc);
    for (int j=0; j<numr; j++) {
      double prob = probArr[probIdx++];
      PackDouble(pt,prob);
    }
  }
  
  // pack sentF prob
  PackChar(pt,flen);
  for (int i=0; i<inslen; i++) {
    double prob = probArr[probIdx++];
    PackDouble(pt,prob);
  }
  if (probIdx != probArr.size()) 
    error_exit("packSendProbRN probArr wrong length");

  if (buff + pktLen != pt) error_exit("inconsistent pktLen in packReqProbRN()");
  return buff;
}

void
Socket::UnpackSendProbRN(vector<int>& numcArr, vector<double>& probArr)
{
  // unpack SENDPROB_RN into numcArr and probArr
  //    Input:  none
  //    Output: numcArr, probArr (must be cleared before calling this)

  // unpack SENDPROB_RN packet
  BufPtr pt = buf;
  int remlen = buflen;

  // unpack numNode
  int numNode = UnpackChar(pt,remlen);

  // unpack each rule
  int sumNumc=0;
  for (int i=0; i<numNode; i++) {
    int numc = UnpackChar(pt,remlen); sumNumc += numc;
    numcArr.push_back(numc);
    int numr = factorial(numc);
    for (int j=0; j<numr; j++) {
      double prob = UnpackDouble(pt,remlen);
      probArr.push_back(prob);
    }
  }

  // unpack NULL prob
  int flen = UnpackChar(pt,remlen);
  int inslen = (1+flen*2)*(1+sumNumc);
  for (int i=0; i<inslen; i++)
    probArr.push_back(UnpackDouble(pt,remlen));

  // check all packet is read
  CheckPacketEnd(remlen);
}


//
// Port methods
// 


Port::Port(Params *prm)
  : param(prm)
{
  numTables=numGraphs=0;
}

void
Port::AllowIncoming(int portno)
{
  // create socket
  s0 = socket(AF_INET, SOCK_STREAM, 0);
  if (s0 < 0) {
    perror("socket");
    exit(1);
  }

  // initialize saddr
  bzero((char *)&saddr, sizeof(saddr));

  // bind
  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr = INADDR_ANY;
  saddr.sin_port = htons(portno);
  setsockopt(s0, SOL_SOCKET, SO_REUSEADDR, (const char *)&s0, sizeof(s0));
  if (bind(s0, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
    perror("bind");
    exit(1);
  }

  // listen
  if (listen(s0, SOCK_MAX) != 0) {
    perror("listen");
    exit(1);
  }
  
}

bool
Port::masterAllConnected()
{
  int xt = param->NumTableServers - numTables;
  int xg = param->MinGraphServers - numGraphs;

#if 1
  if (xt<5 && xg<5) {
    cout << "Waiting " << xt << " tservers and " << xg << " gservers...\n";
    cout.flush();
  }
#endif

  return (numTables == param->NumTableServers &&
	  numGraphs >= param->MinGraphServers);
}

void
Port::acceptNewConnection()
{
  // create new socket
  Socket *sn = new Socket();
  sn->ctx = new Context();

  // accept 
  socklen_t len = sizeof(caddr);
  sn->fd = accept(s0, (struct sockaddr *)&caddr, &len);

  // get client info
  sn->hostaddr = *(unsigned long *)&(caddr.sin_addr);
  struct hostent *hp = 
    gethostbyaddr((char *)&(caddr.sin_addr), sizeof(caddr.sin_addr), AF_INET);

  // get hostname 
  char *pt = (char *)&(caddr.sin_addr);
  if (sizeof(caddr.sin_addr)==4 &&
      *pt++ == 127 && *pt++ == 0 && *pt++ == 0 && *pt++ == 1) {
    // connected from localhost
    struct utsname hname;
    uname(&hname);		// gethostname()
    sn->hostname = hname.nodename; 
  } else {
    sn->hostname = hp->h_name;
  }  
#if 1
  // force hostname to be 6-chars (remove -m0)
  //sn->hostname.replace(sn->hostname.find("-m0"),3,"");
  if (sn->hostname.substr(6,3) == "-m0")
    sn->hostname = sn->hostname.substr(0,6);
  cout << "hostname=" << sn->hostname << "\n"; cout.flush();
#endif

  socks.push_back(sn);	// assume sn is copied...

#if 1
  cout << "socket " << sn->fd << " connected";
  
  // test obtaining client address and hostname
  pt = (char *)&(caddr.sin_addr);
  //cout << "sizeof(sin_addr)=" << sizeof(caddr.sin_addr) << "\n";
  cout << " from ";
  for (int i=0; i<sizeof(caddr.sin_addr); i++)
    cout << "(" << int(*pt++) << ")";
  cout << ", hostname=[" << sn->hostname << "(" << hp->h_name << ")]";
  int portno=ntohs(caddr.sin_port);
  cout << ", port=" << portno;
  cout << "\n";
  cout.flush();
#endif
}


void
Port::EstablishAllMasterConnections()
{
  // wait all tables and min graphs are up and connected

  while (!masterAllConnected()) {
    // set readfds
    FD_ZERO(&readfds);
    FD_SET(s0, &readfds);
    for (int i=0; i<socks.size(); i++) FD_SET(socks[i]->fd, &readfds);
      
    // select
    int idx = select(FD_SETSIZE, &readfds, NULL, NULL, NULL);
    if (idx<0) {
      perror("select");
      exit(1);
    }

    // check already connected sockets
    for (int i=0; i<socks.size(); i++) {
      // assume only in-use socks are pushd to socks[]
      if (FD_ISSET(socks[i]->fd, &readfds)) {
	Socket *sr = socks[i];
	sr->RecvPacket();
	if (sr->funcid == 0) {
	  // connection closed
	  cout << "socket " << socks[i]->fd << " disconnected\n";
	  // remove from socks[]
	  delete socks[i];
	  socks.erase(socks.begin()+i);
	  i--;
	} else if (sr->funcid == FUNC_SENDID) {
	  // determine peer type from ID
	  Socket::BufPtr pt=sr->buf;
	  short cid = sr->UnpackShort(pt);
	  sr->type = (cid >= PORT_TABLE) ? TYPE_TABLE : TYPE_GRAPH;
#if 1
	  cout << "socket " << sr->fd << " is set to type " << sr->type << "\n";
	  cout << "socket " << sr->fd << " received cid=" << cid << "\n";
#endif
	  // increment server count
	  if (sr->type == TYPE_TABLE) {
#if 0
	    // check ESize of the table
	    int tblESize = cid - PORT_TABLE;
	    if (tblESize <= Params::MaxSentELen) {
	      sr->ctx->tableIdx = tblESize;
#else
	    int tidx = cid - PORT_TABLE;
	    if (tidx < param->NumTableServers) {
	      sr->ctx->tableIdx = tidx;
#endif
	      numTables++;
	    } else {
	      cerr << "needless table connected\n";
	    }
	  } else {
	    // type == TYPE_GRAPH
	    numGraphs++;

	  }
	  cout.flush();
	} else {
	  cerr << "Unexpected packet: " << sr->funcid << "\n";
	  cout << "Unexpected packet: " << sr->funcid << "\n"; cout.flush();
	}
      }
    }

    // check new connection
    if (FD_ISSET(s0, &readfds)) acceptNewConnection();
  }
}

Socket *
Port::WaitAvailableGraph()
{
  // wait GRAPHPROB packet from graphs.
  // set busy flag off.
  // also accept new graph connection and disconnect

  //fd_set readfds;   // use in-class var (not a good idea)
  Socket::BufPtr pt;
  FD_ZERO(&readfds);
  FD_SET(s0, &readfds);
  for (int i=0; i<socks.size(); i++)
    FD_SET(socks[i]->fd, &readfds);

  // select
  int idx = select(FD_SETSIZE, &readfds, NULL, NULL, NULL);
  if (idx<0) perror_exit("select");

  // check msg from table or graph
  Socket *ret;
  bool done = false;
  while (!done) {
    for (int i=0; i<socks.size(); i++) {
      if (FD_ISSET(socks[i]->fd, &readfds)) {
	Socket *sc = socks[i];
	sc->RecvPacket();
	switch (sc->funcid) {
	case FUNC_NULL:
	  // disconnect
	  cout << "server (type=" << sc->type << ", fd=" << sc->fd << ") disconnected\n";
#if 0
	  // ignore disconnected servers (it may work for some special case,
	  // such as -tree)
	  if (sc->type == TYPE_TABLE) cout << "table server dead\n";
	  else cout << "graph server dead before returning prob\n";
	  cout << "ignoring disconnected servers...\n";
	  break;
#else
	  // close(sc->fd);
	  if (sc->type == TYPE_TABLE) error_exit("table server dead");
	  else numGraphs--;
	  if (sc->ctx->busy) error_exit("graph server dead before returning prob");
	  delete socks[i];
	  socks.erase(socks.begin()+i);
	  i--;
	  break;
#endif
	case FUNC_GRAPHPROB:
	  // must be from graph
	  if (sc->type == TYPE_TABLE) error_exit("GRAPHPROB from t-server");
#if 1
	  unpackGraphProb(sc);	// sets id/prob/len/mem/cpu in context
#else
	  pt=sc->buf;
	  sc->ctx->lenE = sc->UnpackChar(pt);
	  sc->ctx->prob = sc->UnpackDouble(pt);
#endif
	  sc->ctx->probRcvd = true;
	  sc->ctx->busy = false;
	  ret = sc;		// return this socket
	  done=true;
	  break;
	case FUNC_SENDID:
	  // must be from newly connected server
	  if (sc->type != TYPE_UNKNOWN) error_exit("SENDID from non-newconnection");
	  pt = sc->buf;
	  short cid; cid = sc->UnpackShort(pt);
	  if (cid >= PORT_TABLE) error_exit("additional t-table connection");
	  sc->type = TYPE_GRAPH;
	  numGraphs++;
	  cout << "socket " << sc->fd << " is set to type " << sc->type << "\n";
	  break;
	default:
	  cerr << "unexpected packet from server "
	       << "(type=" << sc->type << ", fd=" << sc->fd << ")\n";
	  break;
	}
	cout.flush();
      } // end of if (FD_ISSET)
    } // end of for all socks[i]

    // check new connection (from graph)
    if (FD_ISSET(s0, &readfds))
      acceptNewConnection();

  } // end of while
  return ret;
}


void
Port::unpackGraphProb(Socket *sg)
{
  // unpack GRAPHPROB

  Socket::BufPtr pt = sg->buf;
  int remlen = sg->buflen;

  sg->ctx->pairNo = sg->UnpackInt(pt,remlen);
  sg->ctx->lenE = sg->UnpackChar(pt,remlen);
  sg->ctx->prob = sg->UnpackDouble(pt,remlen);
  sg->ctx->cpuTime = sg->UnpackDouble(pt,remlen);
  sg->ctx->memSize = sg->UnpackInt(pt,remlen);
  sg->CheckPacketEnd(remlen);
}

void
Port::SendTableInfo(Socket *sg)
{
  // send hostname for tables to graphs
  // note: more graph servers may be connected later.

  // obtain hostname, for each table server.
  // send TABLEINFO to the graph (sg).

#if 0
  // find table orders (0..MaxSentELen)
  Socket *slis[Params::MaxSentELen+1]; // ==numTables
  int totlen = sizeof(short) * numTables;
  for (int i=0; i<socks.size(); i++) {
    Socket *sc = socks[i];
    if (sc->type == TYPE_TABLE) {
      slis[sc->ctx->tableIdx] = sc;
      totlen += sc->hostname.size();
    }
  }
  
  // create a packet for TABLEINFO
  char *buff = (char *)malloc(totlen);
  char *pt = buff;
  for (int i=0; i<numTables; i++)
    sg->PackString(pt,slis[i]->hostname);

  // send TABLE_INFO
  sg->SendPacket(FUNC_TABLEINFO, buff, totlen);
  free(buff);
#else
  // TABELINFO packet: 
  //     numTables(2) hostname(str)*numT

  // sort table order (and obtain pktlen)
  vector<Socket *> slis(numTables);  // -> slis.size()=numTables
  int pktlen = 2;		// for numTables
  for (int i=0; i<socks.size(); i++) {
    Socket *sc = socks[i];
    if (sc->type == TYPE_TABLE) {
      slis[sc->ctx->tableIdx] = sc;
      pktlen += (2 + sc->hostname.size()); // 2 for strlen
    }
  }

  // create TABLEINFO packet
  char *buff = (char *)malloc(pktlen);
  char *pt = buff;
  //cout << "numTables=" << numTables << ",pktlen=" << pktlen << "\n";
  sg->PackShort(pt,numTables);
  for (int i=0; i<numTables; i++)
    sg->PackString(pt,slis[i]->hostname);

  // send TABLEINFO
  sg->SendPacket(FUNC_TABLEINFO, buff, pktlen);
  free(buff);
#endif

  sg->ctx->tableInfoSent = true;
}

void
Port::SendAll(int funcId, int ty=0)
{
  // send to all servers
  for (int i=0; i<socks.size(); i++) {
    Socket *st = socks[i];
    if (ty == 0 || st->type == ty) 
      st->SendPacket(funcId, NULL, 0);
  }
}
  
void
Port::WaitAll(int funcId, int ty=0)
{
  // wait all servers for funcId

  // create flag array
  vector<bool> waiting;
  for (int i=0; i<socks.size(); i++) {
    Socket *st = socks[i];
    waiting.push_back((ty == 0 || st->type == ty));
  }

  bool done=false;
  while (!done) {

    // see any waiting flag is still on
    done=true;
    for (int i=0; i<socks.size(); i++)
      if (waiting[i]) {
	done=false; break;
      }

    if (!done) {
      // wait ACK packet

      // set readfds
      fd_set readfds;
      FD_ZERO(&readfds);
      for (int i=0; i<socks.size(); i++)
	if (waiting[i]) FD_SET(socks[i]->fd, &readfds);

      // select
      int idx = select(FD_SETSIZE, &readfds, NULL, NULL, NULL);
      if (idx<0) perror_exit("select");

      // receive packet
      for (int i=0; i<socks.size(); i++) {
	Socket *st = socks[i];
	if (FD_ISSET(st->fd, &readfds)) {
	  st->RecvPacket();
	  switch (st->funcid) {
	  case FUNC_NULL:
	    // disconnect
	    error_exit("disconnect before returning ACK ", st->fd);
	    break;
	  case FUNC_ACK:
	    waiting[i] = false;
	    break;
	  case FUNC_ERROR:
	    cout << "ERROR packet from " << i << " when waiting ACK\n";
	    cout.flush();
	    break;
	  default:
	    error_exit("unexpected packet when waiting ACK ", st->funcid);
	  }
	}
      }
    }
  }
}
