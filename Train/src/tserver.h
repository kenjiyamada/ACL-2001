#ifndef TSERVER_H
#define TSERVER_H

#include "misc.h"
#include "port.h"
#include "ttable.h"
#include "rtable.h"


class TServer {
  Params *param;
  Socket *master;
  Port *port;

  TTable *ttbl;
  RNTable *rtbl;
public:
  TServer(Params *prm);

  void ConnectMaster();
  void AllowIncoming();

private:
  bool isTTable() { return (param->TableIdx>0); }

  void processRequestProbs(Socket*);
  void processSendCounts(Socket*);
  void unpackReqProb(Socket *sg, vector<int>& posArr);
  void fillSendProb(vector<int>&,vector<int>&,vector<double>&,
		    vector<Word>&,vector<Word>&);
  void addCounts(vector<int>&,vector<double>&,vector<Word>&,vector<Word>&);

  void processRequestProbsRN(Socket*);
  void processSendCountsRN(Socket*);
  void unpackReqProbRN(Socket *sg,vector<int>&);
  void fillSendProbRN(vector<int>&,vector<int>&,vector<double>&,vector<Word>&);
  void addCountsRN(vector<int>&,vector<int>&,vector<double>&,vector<Word>&);

public:
  void LoadFile();
  void SendID();
  void MainLoop();

  void ClearCounts();
  void UpdateProbs();
  void DumpTables();

};

#endif // TSERVER_H
