#ifndef PORT_H
#define PORT_H

#include "misc.h"
#include "context.h"

#include <vector>

extern "C" {
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#include <strings.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/utsname.h>
}

#ifdef sun
#define socklen_t int
#endif

const int TYPE_UNKNOWN = 0;
const int TYPE_MASTER = 1;
const int TYPE_TABLE = 2;
const int TYPE_GRAPH = 3;

const int PORT_MASTER = 7200;
const int PORT_TABLE = 7300;	// (+ ESize, or 0 for RTable)

const int FUNC_NULL = 0;
const int FUNC_SENDID = 1;
const int FUNC_ACK = 2;		// ack for receiving SENDCOUNT,UPDATE,DUMP
const int FUNC_ERROR = 8;
const int FUNC_SHUTDOWN = 9;

// FuncID for table server
const int FUNC_CLEAR = 11;
const int FUNC_UPDATE = 12;
const int FUNC_DUMP = 13;
const int FUNC_REQPROB = 21;
const int FUNC_SENDPROB = 22;
const int FUNC_SENDCOUNT = 23;
const int FUNC_REQPROB_RN = 24;
const int FUNC_SENDPROB_RN = 25;
const int FUNC_SENDCOUNT_RN = 26;

// FuncID for graph server
const int FUNC_TABLEINFO = 31;
const int FUNC_GRAPHREQ = 32;
const int FUNC_GRAPHPROB = 33;

const int SOCK_MAX = 100;	// used by listen()


class Port;			// forward declaration

class Socket {
public:
  int fd;			// socket fd
  int type;			// type of peer (master, table, or graph)
  unsigned int hostaddr;	// client address (filled when calling accept())
  string hostname;		// client hostname (same as above)
  Context *ctx;			// context info

//#ifdef __i386__
#if 0
  Socket(int ty=0);		// g++ problem?
#else
  Socket(int ty=TYPE_UNKNOWN);
#endif

  void SendPacket(int funcId, char *buff, int len);
  void RecvPacket();

  // message buffer (for receive)
  typedef char* BufPtr;
  int funcid;
  char *buf;
  int buflen;
  int alloclen;

  union packBuf {
    char  xchar[8];
    short xshort[4];
    long  xint[2];
    double xdouble;
  };

  inline void ResetBufPtr(BufPtr& pt) { pt = buf; }

  inline void PackChar(BufPtr& pt, char c);
  inline void PackInt(BufPtr& pt, int i);
  inline void PackShort(BufPtr& pt, short i);
  inline void PackDouble(BufPtr& pt, double d);
  inline void PackString(BufPtr& pt, const string& str);

  inline char UnpackChar(BufPtr& pt);
  inline int UnpackInt(BufPtr& pt);
  inline short UnpackShort(BufPtr& pt);
  inline double UnpackDouble(BufPtr& pt);
  inline void UnpackString(BufPtr& pt, string &str);

  // unpack with length check
  inline char UnpackChar(BufPtr& pt, int& remlen);
  inline int UnpackInt(BufPtr& pt, int& remlen);
  inline short UnpackShort(BufPtr& pt, int& remlen);
  inline double UnpackDouble(BufPtr& pt, int& remlen);
  inline void UnpackString(BufPtr& pt, string &str, int& remlen);
  inline void CheckPacketEnd(int remlen);


  // Methods for clients
  void Connect(string host, int portno);	// connect to server
  void SendID(int id);		// send client ID (portno for table server), which
				// implies client type.
  void SendACK();		// send back ACK

  
  // Prob/Count table packing and unpacking (for tserver and gserver)
  BufPtr PackSendProb(vector<int>&,vector<double>&,int&);
  void UnpackSendProb(vector<int>&, vector<double>&);
  BufPtr PackSendProbRN(int,vector<int>&,vector<double>&,int&);
  void UnpackSendProbRN(vector<int>& , vector<double>&);
};

class Port {
  Params *param;
public:
  int s0;			// socket for connection
  struct sockaddr_in saddr;	// server address
  struct sockaddr_in caddr;	// client address
  fd_set readfds;

  vector<Socket*> socks;
  int numTables, numGraphs;	// number of sockets for Tables/Graphs

  bool masterAllConnected();
  void acceptNewConnection();

  Port(Params *prm);
  void AllowIncoming(int portno);       // for master and tables
  void EstablishAllMasterConnections();	// for master only (from tables and graphs)
  Socket* WaitAvailableGraph();	        // for master only
  void unpackGraphProb(Socket *sg);     // for master only
  void SendTableInfo(Socket *sg);	// for master only (send to graphs)
  void SendAll(int funcId, int type=0);
  void WaitAll(int funcId, int type=0);

};

//
// inline functions
//

// Pack functions

inline void
Socket::PackChar(BufPtr& pt, char c)
{
  *pt++ = c;
}

inline void
Socket::PackShort(BufPtr& pt, short s)
{
  union packBuf xbuf;
  short xs = htons(s);
  xbuf.xshort[0] = xs;
  *pt++ = xbuf.xchar[0];
  *pt++ = xbuf.xchar[1];
}

inline void
Socket::PackInt(BufPtr& pt, int i)
{
  union packBuf xbuf;
  int xi = htonl(i);
  xbuf.xint[0] = xi;
  for (int i=0; i<4; i++) *pt++ = xbuf.xchar[i];
}

inline void
Socket::PackDouble(BufPtr& pt, double d)
{
  union packBuf xbuf;
  xbuf.xdouble = d;
  int x0 = xbuf.xint[0];
  int x1 = xbuf.xint[1];
#ifdef __i386__
  xbuf.xint[1] = htonl(x0);
  xbuf.xint[0] = htonl(x1);
#else
  xbuf.xint[0] = htonl(x0);
  xbuf.xint[1] = htonl(x1);
#endif
  for (int i=0; i<8; i++) *pt++ = xbuf.xchar[i];
}

inline void
Socket::PackString(BufPtr& pt, const string& str)
{
  PackShort(pt, str.size());
  strncpy(pt, str.c_str(), str.size());
  pt += str.size();
}


// Unpack functions

inline char
Socket::UnpackChar(BufPtr& pt)
{
  char c = *pt++;
  return c;
}

inline short
Socket::UnpackShort(BufPtr& pt)
{
  union packBuf xbuf;
  xbuf.xchar[0] = *pt++;
  xbuf.xchar[1] = *pt++;
  short s = ntohs(xbuf.xshort[0]);
  return s;
}

inline int
Socket::UnpackInt(BufPtr& pt)
{
  union packBuf xbuf;
  for (int i=0; i<4; i++) xbuf.xchar[i] = *pt++;
  int i = ntohl(xbuf.xint[0]);
  return i;
}

inline double
Socket::UnpackDouble(BufPtr& pt)
{
  union packBuf xbuf;
  for (int i=0; i<8; i++) xbuf.xchar[i] = *pt++;
  int x0 = ntohl(xbuf.xint[0]);
  int x1 = ntohl(xbuf.xint[1]);
#ifdef __i386__
  xbuf.xint[1] = x0;
  xbuf.xint[0] = x1;
#else
  xbuf.xint[0] = x0;
  xbuf.xint[1] = x1;
#endif
  double d = xbuf.xdouble;
  return d;
}

inline void
Socket::UnpackString(BufPtr& pt, string &str)
{
  int len = UnpackShort(pt);
  // copy *pt to str for length len
  str.replace(0,str.size(),pt,len);
  pt += len;
}

//
// unpack with length check
//

inline char 
Socket::UnpackChar(BufPtr& pt, int& remlen)
{
  if (remlen<sizeof(char)) error_exit("packet too short for char");
  remlen--;
  return UnpackChar(pt);
}

inline int 
Socket::UnpackInt(BufPtr& pt, int& remlen)
{
  if (remlen<sizeof(int)) error_exit("packet too short for int");
  remlen -= sizeof(int);
  return UnpackInt(pt);
}

inline short 
Socket::UnpackShort(BufPtr& pt, int& remlen)
{
  if (remlen<sizeof(short)) error_exit("packet too short for short");
  remlen -= sizeof(short);
  return UnpackShort(pt);
}

inline double 
Socket::UnpackDouble(BufPtr& pt, int& remlen)
{
  if (remlen<sizeof(double)) error_exit("packet too short for double");
  remlen -= sizeof(double);
  return UnpackDouble(pt);
}  

inline void 
Socket::UnpackString(BufPtr& pt, string &str, int& remlen)
{
  if (remlen<sizeof(short)) error_exit("packet too short for string header");
  int strlen = UnpackShort(pt,remlen);
  pt -= sizeof(short);		// to reread by UnpackString()
  if (remlen<strlen) error_exit("packet too short for string");
  remlen -= strlen;
  UnpackString(pt,str);
}

inline void 
Socket::CheckPacketEnd(int remlen)
{
  if (remlen!=0) error_exit("packet too big");
}

#endif // PORT_H
