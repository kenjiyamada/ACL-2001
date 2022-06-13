#include "rtable.h"

// static member definition
bool InsTable::nullShared;
map<string,ProbCnt> InsTable::insword_Shared;

void
InsTable::clear()
{
  noins.clear();
  right_Shared.clear();
  left_Shared.clear();
}

void
InsTable::normalize()
{
  // obtain sum
  double sum = noins.cnt;

  if (nullShared) {
    sum += right_Shared.cnt;
    sum += left_Shared.cnt;
  } else {
    for (InsTblItr p = right.begin(); p != right.end(); p++) {
      const string& insword = p->first;
      const ProbCnt& pc = p->second;
      sum += pc.cnt;
    }
    for (InsTblItr p = left.begin(); p != left.end(); p++) {
      const string& insword = p->first;
      const ProbCnt& pc = p->second;
      sum += pc.cnt;
    }
  }

  // normalize
  noins.prob = (sum>0) ? (noins.cnt/sum) : 0.0;
  noins.cnt = 0.0;

  if (nullShared) {
    right_Shared.prob = (sum>0) ? (right_Shared.cnt/sum) : 0.0;
    right_Shared.cnt = 0.0;
    left_Shared.prob = (sum>0) ? (left_Shared.cnt/sum) : 0.0;
    left_Shared.cnt = 0.0;
  } else {
    for (InsTblItr p = right.begin(); p != right.end(); p++) {
      const string& insword = p->first;
      const ProbCnt& pc = p->second;
      right[insword].prob = (sum>0) ? (pc.cnt/sum) : 0.0;
      right[insword].cnt = 0.0;
    }
    for (InsTblItr p = left.begin(); p != left.end(); p++) {
      const string& insword = p->first;
      const ProbCnt& pc = p->second;
      left[insword].prob = (sum>0) ? (pc.cnt/sum) : 0.0;
      left[insword].cnt = 0.0;
    }
  }
  
  // caller UpdateProbs() must normalize InsTable::insword_Shared separately
}  

void
InsTable::normalize_insword_Shared()
{
  map<string,ProbCnt>& nullT = insword_Shared;

  // normalize nullT
  double sum = 0.0;
  
  // obtain sum
  for (InsTblItr p = nullT.begin(); p != nullT.end(); p++) {
    const string& fwordins = p->first;
    const ProbCnt& pc = p->second;
    sum += pc.cnt;
  }
  
  // normalize
  for (InsTblItr p = nullT.begin(); p != nullT.end(); p++) {
    const string& fwordins = p->first;
    const ProbCnt& pc = p->second;
    nullT[fwordins].prob = (sum>0) ? (pc.cnt / sum) : 0.0;
    nullT[fwordins].cnt = 0.0;
  }
}

void
InsTable::dumpTable(ofstream *dump, int pos) const
{
  // no-insert
  *dump << pos << " 0 0 " << noins.prob << "\n";

  if (nullShared) {
    *dump << pos << " 1 0 " << right_Shared.prob << "\n";
    *dump << pos << " 2 0 " << left_Shared.prob << "\n";

    // caller DumpTables() must dump InsTable::insword_Shared separately

  } else {

    // right
    for (InsTblItr p = right.begin(); p != right.end(); p++) {
      const string& insword = p->first;
      const ProbCnt& pc = p->second;
      if (pc.prob>0)
	*dump << pos << " 1 " << insword << " " << pc.prob << "\n";
    }
    
    // left
    for (InsTblItr p = left.begin(); p != left.end(); p++) {
      const string& insword = p->first;
      const ProbCnt& pc = p->second;
      if (pc.prob>0)
	*dump << pos << " 2 " << insword << " " << pc.prob << "\n";
    }
  }
}

void
InsTable::dump_insword_Shared(ofstream *dump)
{
  // dump NullT
  
  // dummy rule
  // assumy dummyRule == "0 -> 0 0 0"
  const string dummyProbR = "0 0 0 0 0 0";
  *dump << ": " << dummyRule << " | " << dummyProbR << "\n";

  map<string,ProbCnt>& nullT = insword_Shared;
  for (InsTblItr p = nullT.begin(); p != nullT.end(); p++) {
    const string& insword = p->first;
    const ProbCnt& pc = p->second;
    double prob = pc.prob;
    const int dummyPos = 0, dummyIdx = 1;
    if (prob>0) 
      *dump << dummyPos << " " << dummyIdx << " " << insword << " " << prob << "\n";
  }
}

void
InsTable::setProb(int insIdx, const string& word, double prob)
{
  // here we check nullShared
  
  if (insIdx==0) { noins.prob = prob; noins.cnt = 0.0; }
  else {
    if (nullShared) {
      if (word!=dummyWord) {
	ProbCnt& ent = insword_Shared[word];
	ent.prob = prob; ent.cnt = 0.0;
      } else {
	switch(insIdx) {
	case 1: right_Shared.prob = prob;
	        right_Shared.cnt = 0.0; break;
	case 2: left_Shared.prob = prob;
	        left_Shared.cnt = 0.0; break;
	default: error_exit("unexpected insIdx in InsTable::setProb(): ",insIdx);
	}
      }
    } else {
      // not nullShared
      ProbCnt *ent;
      switch(insIdx) {
      case 1: ent = &(right[word]);
	      ent->prob = prob;
	      ent->cnt = 0.0; break;
      case 2: ent = &(left[word]);
	      ent->prob = prob;
	      ent->cnt = 0.0; break;
      default: error_exit("unexpected insIdx in InsTable::setProb(): ",insIdx);
      }
    }
  }
}

double
InsTable::getProb(int insIdx, const string& word)
{
  // here we check nullShared

  if (insIdx==0) return noins.prob;
  else {
    if (nullShared) {
      switch(insIdx) {
      case 1: return (right_Shared.prob * insword_Shared[word].prob);
      case 2: return (left_Shared.prob * insword_Shared[word].prob);
      default: error_exit("unexpected insIdx in InsTable::getProb(): ",insIdx);
      }
    } else {
      switch(insIdx) {
      case 1: return right[word].prob;
      case 2: return left[word].prob;
      default: error_exit("unexpected insIdx in InsTable::getProb(): ",insIdx);
      }
    }
  }
}

void
InsTable::addCount(int insIdx, const string& word, double val)
{
  // here we check nullShared

  if (insIdx==0) noins.cnt += val;
  else {
    if (nullShared) {
      switch(insIdx) {
      case 1: right_Shared.cnt += val; insword_Shared[word].cnt += val; break;
      case 2: left_Shared.cnt += val; insword_Shared[word].cnt += val; break;
      default: error_exit("unexpected insIdx in InsTable::addCount(): ",insIdx);
      }
    } else {
      switch(insIdx) {
      case 1: right[word].cnt += val; break;
      case 2: left[word].cnt += val; break;
      default: error_exit("unexpected insIdx in InsTable::addCount(): ",insIdx);
      }
    }
  }
}

void
InsTable::print()
{
  // no-insert
  cout << " 0 0 " << noins.prob << "(" << noins.cnt << ")\n";

  // right
  for (InsTblItr p = right.begin(); p != right.end(); p++) {
    const string& insword = p->first;
    const ProbCnt& pc = p->second;
    cout << " 1 " << insword << " " << pc.prob << "(" << pc.cnt << ")\n";
  }

  // left
  for (InsTblItr p = left.begin(); p != left.end(); p++) {
    const string& insword = p->first;
    const ProbCnt& pc = p->second;
    cout << " 1 " << insword << " " << pc.prob << "(" << pc.cnt << ")\n";
  }
}

ProbRN::ProbRN()
{
  // initialize prob/cnt
  for (int i=0; i<maxr; i++) ord[i].clear();
  for (int i=0; i<maxc; i++) insFull[i].clear();
}


//
// RN-Table
//

RNTable::RNTable(Params *param)
  : param(param)
{
  // copy flag from param (will be changed later)
  useEqualProb = param->UseEqualProb;
  EqualProbVal = param->EqualProbVal;
  oldMode = param->OldMode;

  // set InsTable class variable
  InsTable::nullShared = param->ShareNullProb;

  // check flag comibination
#if 0
  if (! shareNullProb() && ! useEqualProb)
    error_exit("-eqlprob needed unless -sharenull");
#endif

  // open dump file
  if (!param->NoDumpFile && !param->NoTableFile) {
    string fname = param->TableFile;
    fname += "0";
    fname += param->DumpFile;
    dump = new ofstream(fname.c_str());
    if (!*dump) error_exit("can't open dump file [" + fname + "]");
    *dump << "#TableRN\n";
  }
}

void
RNTable::LoadFile()
{
  if (useEqualProb) return;

  string fname = param->TableFile;
  fname += "0";

  ifstream in(fname.c_str());
  if (!in) error_exit("can't open table file [" + fname + "]");

  bool needNormalize;
  string line;
  if (getline(in,line)) {
    if (line == "#TableRN") {
      needNormalize = false;
      loadDumpFile(in,line);
    } else {
#if 1
      // as long as loadTableFile() sets probs directly, we don't need normalize.
      needNormalize = false;
#else
      needNormalize = true;
#endif
      // if it needs to load table for t-table, use -eqlprob just for r-table,
      // even if not -sharenull
      if (!param->ShareNullProb) error_exit("can't load table without -sharenull");
      loadTableFile(in,line);
    }
  }

  if (needNormalize) UpdateProbs();
  cout << "loading done.\n";
}

void
RNTable::loadTableFile(ifstream& in, string& line1)
{
  // read tbl-0 for rules, and vocab.f for nullT map
  // tbl-0 format:   freq parent c c c...

  vector<string> parentList;

  string line = line1;
  do {
    // read one line into string vector
    vector<string> wds;
    split(line,wds);		// freq parent c1 c2...

    // get info
    int numc = wds.size() - 2;
    string parent = wds[1];
    parentList.push_back(parent);

    // make key string
    string rule = parent;
    for (int i=2; i<wds.size(); i++) rule += (" " + wds[i]);

    // add count
    loadUniformProbRN(rule,numc);

  } while(getline(in,line));


  // make TOP->X entries
  for (int i=0; i<parentList.size(); i++) {
    string rule = "0 " + parentList[i];	// TOP=0
    loadUniformProbRN(rule,1);
  }

  // initialize nullT map
  string fname = param->VocabFFile;
  ifstream nin(fname.c_str());
  if (!nin) error_exit("can't open vocab file [" + fname + "]");

  while(getline(nin,line)) {

    // read one line into string vector
    vector<string> wds;
    split(line,wds);		// id word freq

    // no check for id range, since addCountN() uses a decimal string
    // for the key.

    // create entry
    string& word = wds[0];
    InsTable::insword_Shared[word].cnt = 1.0;
  }
  InsTable::normalize_insword_Shared();
}

void
RNTable::loadUniformProbRN(const string& rule, int numc)
{
  // Assume this is called only when shareNullProb()==true.

  // For the old code compatibility,
  // the uniform prob is somewhat strange (use 1/ProbRN::maxr).

  ProbRN& rn = probs[rule];
  rn.numc = numc;		// this var was added later since DumpTables() needs it.
  
  double prob = 1.0/ProbRN::maxr; // old code compatibility
  for (int i=0; i<ProbRN::maxr; i++) { rn.ord[i].prob = prob; rn.ord[i].cnt = 0.0; }

  prob = 1.0/3;
  for (int i=0; i<ProbRN::maxc; i++)
    for (int insIdx=0; insIdx<3; insIdx++)
      rn.insFull[i].setProb(insIdx,dummyWord,prob);
      //ent.setProbN(i,insIdx,dummyWord,prob);
}  

void
RNTable::loadDumpFile(ifstream& in, string& line1)
{
  // read dump file for #TableRN
  // format: 

  // : parent c c ... | probR ...
  // pos 0 0 prob      (no-insert)
  // pos 1 w prob      (right-insert)
  // pos 1 w prob
  // ..
  // pos 2 w prob      (left-insert)
  // pos 2 w prob
  // ..

  // for nullShared,
  // w==0 -> right/left_Shared
  // w!=0 -> insword_Shared

  string line;
  while(getline(in,line)) {
    if (line.size()==0 || line[0] == '#') {
      // ignore null or comment line
    } else {
      vector<string> wds;
      split(line,wds);

      ProbRN *rn;
      if (wds[0] == ":") {
	// rule and probR line
	// similar to loadDumpFileNullShared()
	vector<string> children;
	for (int i=2; i<wds.size(); i++) {
	  string w = wds[i];
	  if (w=="|") break;
	  else children.push_back(w);
	}
	int numc = children.size();
	int numr = factorial(numc);
	if (wds.size() != 1 + 1 + numc + 1 + numr)
	  error_exit("TableRN wrong format (loadDumpFileNullFull), numc: ",numc);

	// make key string
	string rule = wds[1];	// parent
	for (int i=1+1; i<1+numc+1; i++) rule += (" " + wds[i]);
	
	// read probR
	vector<double> ord;
	for (int i=1+1+numc+1; i<1+1+numc+1+numr; i++) // : p <numc> | <numr>
	  ord.push_back(atof(wds[i].c_str()));
	setProbR(rule,numc,ord);

	// save entry for insert table lines
	rn = &(probs[rule]);
	
      } else {
	// insert table line
	int pos = atoi(wds[0].c_str());
	int insIdx = atoi(wds[1].c_str());
	string word = wds[2];
	double prob = atof(wds[3].c_str());

	//ent->setProbN(pos,insIdx,word,prob);
	rn->insFull[pos].setProb(insIdx,word,prob);
      }
    }
  }
}

void
RNTable::getProbR(const string& rule, int numc, vector<double>& ord)
{
  ProbRN& ent = probs[rule];
  ent.numc = numc;		// this var was added later since DumpTables() needs it,
				// and if -eqlprob, numc is unknown until now.

  // fill ord[]
  if (numc==0) ord.push_back(1.0);// for leaf node
  else {
    int numr = factorial(numc);
    for (int i=0; i<numr; i++)
      ord.push_back(useEqualProb ? EqualProbVal : ent.ord[i].prob);
  }
}  

void
RNTable::setProbR(const string& rule, int numc, vector<double>& ord)
{
  ProbRN& ent = probs[rule];
  ent.numc = numc;		// for safety

  // set probR from ord[]
  for (int i=0; i<ord.size(); i++) {
    ent.ord[i].prob = ord[i]; ent.ord[i].cnt = 0.0;
  }
}

void
RNTable::addCountR(const string& rule, int numc, vector<double>& ord)
{
  ProbRN& ent = probs[rule];
  ent.numc = numc;		// for safety

  // set countR from ord[]
  for (int i=0; i<ord.size(); i++) ent.ord[i].cnt += ord[i];
}

//
// internal access methods for Null-FULL
//

void
RNTable::getProbN(string& rulestr, int numc,
		  vector<string>& sentF, vector<double>& prbs)
{
  // set numc, since it may be unaccessed data
  ProbRN& ent = probs[rulestr];
  ent.numc = numc;

  // fill probs for [numc * (1 + flen*2)]
  if (useEqualProb) {
    int inslen = numc * (1 + sentF.size() * 2);
    for (int i=0; i<inslen; i++) prbs.push_back(EqualProbVal);
  } else {
    for (int i=0; i<numc; i++) {
      InsTable& instbl = ent.insFull[i];
      prbs.push_back(instbl.getProb_noins());
      for (int j=0; j<sentF.size(); j++) {
	string word = sentF[j];
	prbs.push_back(instbl.getProb_right(word));
	prbs.push_back(instbl.getProb_left(word));
      }
    }
  }
}

void
RNTable::addCountN(string& rulestr, int numc,
		   vector<string>& sentF, vector<double>& cnts)
{
  // add counts for [numc * (1 + flen*2)] in cnts[]
  ProbRN& ent = probs[rulestr];
  int idx=0;
  for (int i=0; i<numc; i++) {
    InsTable& instbl = ent.insFull[i];
    instbl.addCount_noins(cnts[idx++]);
    for (int j=0; j<sentF.size(); j++) {
      string word = sentF[j];
      instbl.addCount_right(word,cnts[idx++]);
      instbl.addCount_left(word,cnts[idx++]);
    }
  }
}

//
// public methods
//

void
RNTable::makeOldRuleStr(string& rstr, vector<Word>& rule)
{
  // replace parent symbol with DUMMY
  vector<Word> rule1 = rule;
  rule1[0] = DUMMY_PARENT;
  sout(rstr,rule1);
}

void
RNTable::GetProbR(vector<Word>& rule, vector<double>& ord)
{
  string rstr;

  if (oldMode) makeOldRuleStr(rstr,rule);
  else sout(rstr,rule);		// rule -> rstr
  
  int numc = rule.size() - 1;	// -1 for parent
  getProbR(rstr,numc,ord);
}

void
RNTable::AddCountR(vector<Word>& rule, vector<double>& ord)
{
  string rstr;

  if (oldMode) makeOldRuleStr(rstr,rule);
  else sout(rstr,rule);		// rule -> rstr

  int numc = rule.size() - 1;	// -1 for parent
  addCountR(rstr,numc,ord);
}

void 
RNTable::GetProbN(vector<Word>& rule, vector<Word>& sentF, vector<double>& ins)
{
  // make string vector
  vector<string> sent;
  for (int i=0; i<sentF.size(); i++) {
    string str;
    sout(str, sentF[i]);
    sent.push_back(str);
  }
    
  if (oldMode) getProbN_oldMode(rule,sent,ins);
  else {
    
    // make rule string
    string rstr;
    sout(rstr,rule);		// rule -> rstr
    int numc = rule.size() - 1;	// -1 for parent
    
    // fill prob vector
    getProbN(rstr,numc,sent,ins);
  }
}

void
RNTable::getProbN_oldMode(vector<Word>& rule, vector<string>& sent, vector<double>& ins)
{
  // fake to be a set of unary rules

  // parent 
  string pstr;
  sout(pstr,rule[0]);
  pstr += " ";
  
  // for each children
  for (int i=1; i<rule.size(); i++) {
    string rstr,cstr;
    sout(cstr,rule[i]);
    rstr = pstr + cstr;

    getProbN(rstr,1,sent,ins);
  }
}  

void 
RNTable::AddCountN(vector<Word>& rule, vector<Word>& sentF, vector<double>& cnts)
{
  // make string vector
  vector<string> sent;
  for (int i=0; i<sentF.size(); i++) {
    string str;
    sout(str, sentF[i]);
    sent.push_back(str);
  }
    
  if (oldMode) addCountN_oldMode(rule,sent,cnts);
  else {

    // make rule string
    string rstr;
    sout(rstr,rule);		// rule -> rstr
    int numc = rule.size() - 1;	// -1 for parent
    
    // set counts
    addCountN(rstr,numc,sent,cnts);
  }
}  

void
RNTable::addCountN_oldMode(vector<Word>& rule, vector<string>& sent, vector<double>& cnts)
{
  // fake to be a set of unary rules

  // parent 
  string pstr;
  sout(pstr,rule[0]);
  pstr += " ";
  
  // cnts must be split for each unary rule
  int numc = rule.size()-1;	// -1 for parent
  int ncnt = (numc>0) ? (cnts.size() / numc) : 0;
  int cidx=0;

  // for each children
  for (int i=1; i<rule.size(); i++) {
    string rstr,cstr;
    sout(cstr,rule[i]);
    rstr = pstr + cstr;

    // set counts
    vector<double> cnt1;
    for (int j=0; j<ncnt; j++)	cnt1.push_back(cnts[cidx++]);
    addCountN(rstr,1,sent,cnt1);
  }
}

//
// Update
//

void
RNTable::UpdateProbs()
{
  // normalize probR (also clear counts)
  // also normalize probN if not shareNullProb()

  for (ProbRNItr p = probs.begin(); p != probs.end(); p++) {
    const string& rule = p->first;
    const ProbRN& ent = p->second;

    // normalize ProbR
    double sum = 0.0;
    for (int i=0; i<ProbRN::maxr; i++)
      sum += ent.ord[i].cnt;
    for (int i=0; i<ProbRN::maxr; i++) {
      probs[rule].ord[i].prob = (sum>0) ? (ent.ord[i].cnt / sum) : 0.0;
      probs[rule].ord[i].cnt = 0.0;
    }
    
    // normalize ProbN

    // normalize insFull[] (shareNull will be taken take of)
    for (int i=0; i<ProbRN::maxc; i++) {
      probs[rule].insFull[i].normalize();
    }
  }    

  // normalize NullT if necessary
  if (InsTable::nullShared) InsTable::normalize_insword_Shared();

  // disable useEqualProb once updated
  useEqualProb = false;

}

//
// Dump Tables
//

void
RNTable::DumpTables()
{
  // dump table
  if (param->NoDumpFile || param->NoTableFile) return;

  // dump ProbRN, using insFull[]
  for (ProbRNItr p = probs.begin(); p != probs.end(); p++) {
    const string& rule = p->first;
    const ProbRN& ent = p->second;

    if (rule == dummyRule) continue;

    // ProbR part is similar to dumpTablesNullShared()
    int numc = ent.numc;
    int numr = factorial(numc);

    if (numc>0) {
      *dump << ": " << rule << " |";

      // ProbR
      for (int i=0; i<numr; i++) {
	double prob = ent.ord[i].prob;
	*dump << " " << prob;
      }
      *dump << "\n";

      // ProbN (insFull[])
      for (int i=0; i<numc; i++)
	ent.insFull[i].dumpTable(dump,i);
    }
  }

  // dump NullT if necessary
  if (InsTable::nullShared) InsTable::dump_insword_Shared(dump);

  // close dump file
  delete dump;
}

