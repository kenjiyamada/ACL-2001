#include "ttable.h"

FWords::FWords()
{
  // clear fertility (for safety)
  for (int i=0; i<maxFert; i++)
    fertility[i].prob=fertility[i].cnt=0.0;

  // clear lambda1
  lambda1prob = lambda1cnt = lambda1ncnt = 0.0;
}

//
// T-Table
//

TTable::TTable(Params *param)
  : param(param)
{
  // copy flag from param (will be changed later)
  useEqualProb = param->UseEqualProb;
  EqualProbVal = param->EqualProbVal;
  fertileMode = param->FertileMode;
  useCutoff = param->UseCutoff;
  oldMode = param->OldMode;
  oldMode2 = param->OldMode2;

  // open dump file
  if (!param->NoDumpFile && !param->NoTableFile) {
    string fname = param->TableFile;

    // add index
    int idx = param->TableIdx;
    if (idx>=10) fname += ('0' + idx/10);
    fname += ('0' + idx%10);

    // add dump suffix and open
    fname += param->DumpFile;
    dump = new ofstream(fname.c_str());
    if (!*dump) error_exit("can't open dump file [" + fname + "]");
    *dump << "#TableT\n";
  }
}

void
TTable::LoadFile()
{
  // file format
  // : e e
  // F prob prob ...      // for fertility
  // L prob               // for lambda1
  // f f f 
  // f f [| prob]

  // if -eqlprob, no table is needed, but if -cutoff is applied,
  // it need to check the entry existence.
  if (useEqualProb && !useCutoff) return;

  // determine table file name
  string fname;
  if (param->NoTableFile) return;
  else {
    fname = param->TableFile;
    int idx = param->TableIdx;
    if (idx>=10) fname += ('0' + idx/10);
    fname += ('0' + idx%10);
  }

  ifstream in(fname.c_str());
  if (!in) error_exit("can't open table file [" + fname + "]");
  
  bool needNormalize = false;
  string line,wordE,wordF,prob; bool addNullCount=true;
  while(getline(in,line)) {
    if (line == "#TableT") {
      // ignore header
    } else if (line[0]==':') {
      if (wordE.size()>0 && addNullCount) 
	addCount(wordE,"0",1.0);	// "0" for NULL wordF
      wordE = line.substr(2);
    } else if (line[0]=='F') {
      // fertility line
      readFertilityLine(wordE,line);
    } else if (line[0]=='L') {
      // lambda1 line
      readLambda1Line(wordE,line);
    } else {
      string::size_type bpos = line.find('|');
      if (bpos != line.npos) {
	// prob is specified after |
	addNullCount=false;
	wordF = line.substr(0,bpos-1);
	prob = line.substr(bpos+2);
	setProb(wordE,wordF,atof(prob.c_str()));
      } else {
	// no prob specified
	needNormalize = true;
	wordF = line;
	addCount(wordE,wordF,1.0);
      }
    }
  }
  if (wordE.size()>0 && addNullCount) addCount(wordE,"0",1.0);

  if (needNormalize) UpdateProbs(false); // normalize. (not to disable useEqualProb)

  cout << "Loading done.\n";
  cout.flush();
}

//
// internal prob handling methods
//

double
TTable::getProb(const string& eword, const string& fword)
{
#if 0
  // eqlprob handling gone to GetProb()
  if (useEqualProb) return (EqualProbVal);
#endif

  // look for e2f[eword].words[fword].prob
  // use find() to avoid inserting zero-prob entries

  EWordItr p = e2f.find(eword);
  if (p==e2f.end()) return 0.0;
  else {
    const FWords& fwords = p->second;
    FWordItr q = fwords.words.find(fword);
    if (q==fwords.words.end()) return 0.0;
    else return q->second.prob;
  }
}

void
TTable::setProb(const string& eword, const string& fword, double val)
{
  ProbCnt& ent = e2f[eword].words[fword];
  ent.prob = val;
  ent.cnt = 0.0;
}

void
TTable::addCount(const string& eword, const string& fword, double val)
{
  // note: this may be called internally other than public AddCount()

  if (val>0) 
    e2f[eword].words[fword].cnt += val;
}

//
// fertility handling methods
//

void
TTable::readFertilityLine(const string& estr, const string& line)
{
  // line = "F prob prob..."
  vector<string> strArr;
  split(line,strArr);
  for (int i=1; i<strArr.size(); i++) {
    double prob = atof(strArr[i].c_str());
    ProbCnt& pc = e2f[estr].fertility[i-1]; // can be optimized?
    pc.prob = prob;
    pc.cnt = 0.0;
  }
}


double
TTable::getProb_Fertile1(const string& estr, vector<Word>& fword)
{
  EWordItr p = e2f.find(estr);
  if (p==e2f.end()) return 0.0;
  else {
    int flen = fword.size();
    const FWords& fwords = p->second;
    double transP = 1.0;
    double fertP = fwords.fertility[flen].prob;
    for (int i=0; i<flen; i++) {
      string fstr;
      sout(fstr,fword[i]);
      FWordItr q = fwords.words.find(fstr);
      if (q==fwords.words.end()) {
	transP = 0.0; break;
      } else {
	transP *= q->second.prob;
      }
    }
    return fertP * transP;
  }
}

void
TTable::addCount_Fertile1(const string& estr, vector<Word>& fword, double val)
{
  if (val>0) {
    int flen = fword.size();
    e2f[estr].fertility[flen].cnt += val;
    for (int i=0; i<flen; i++) {
      string fstr;
      sout(fstr,fword[i]);
      e2f[estr].words[fstr].cnt += val;
    }
  }
}

//
// public methods
//

double 
TTable::GetProb(vector<Word>& eword, vector<Word>& fword)
{
  // if this is a dummy t-table
  if (param->NoTableFile) return 0.0;

  // handle old mode (-cutoff is not supported)
  if (oldMode && !oldMode2) {
    if (eword.size()!=1 || fword.size()>1) return 0.0;
    if (useEqualProb) return EqualProbVal;
    // else go through
  }

  // eword -> estr
  string estr;
  sout(estr,eword);

  // fword -> fstr
  string fstr;
  if (fword.size()==0) fstr="0"; // "0" for NULL
  else sout(fstr,fword);
  
  // handle -eqlprob
  if (useEqualProb) {
    double prob = EqualProbVal;

    // check table entry if cutoff is used.
    // Note: it should not applied to 1-to-1 entries
    if (useCutoff && (eword.size() > 1 || fword.size() > 1)) {
      if (getProb(estr,fstr) == 0.0) return 0.0;
    }

    // simulate all component of fertile word prob is equal
    if (fertileMode>0) {
      for (int i=0; i<fword.size(); i++) prob *= EqualProbVal;
    }
    return prob;
  }

  if (fertileMode>0) {
    return getProb_Fertile1(estr,fword);
  } else {
    // normal fertile mode.
    // get prob
    return getProb(estr,fstr);
  }
}

void
TTable::AddCount(vector<Word>& eword, vector<Word>& fword, double val)
{
  if (param->NoTableFile || param->NoUpdate) return;

  // eword -> estr
  string estr;
  sout(estr,eword);

  if (fertileMode>0) {
    addCount_Fertile1(estr,fword,val);

  } else {
    // normal fertile mode

    // fword -> fstr
    string fstr;
    if (fword.size()==0) fstr="0"; // "0" for NULL
    else sout(fstr,fword);
    
    // add count
    addCount(estr,fstr,val);
  }
}

void
TTable::UpdateProbs(bool disableEqlFlag=true)
{
  // normalize table (also clear counts)
  if (param->NoTableFile) return;

  if (param->NoUpdate) {
    cout << "update ignored by -noupdate\n";
    return;
  }

  for (EWordItr p = e2f.begin(); p != e2f.end(); p++) {
    const string& eword = p->first;
    const FWords& fwords = p->second;

    // obtain sum
    double sum = 0.0;
    for (FWordItr q = fwords.words.begin(); q != fwords.words.end(); q++) {
      //const string& fword = q->first;
      const double cnt = q->second.cnt;
      sum += cnt;
    }

    // normalize
    for (FWordItr q = fwords.words.begin(); q != fwords.words.end(); q++) {
      const string& fword = q->first;
      const double cnt = q->second.cnt;
      ProbCnt& pc = e2f[eword].words[fword];
      pc.prob = (sum>0) ? (cnt/sum) : 0.0;
      pc.cnt = 0.0;		// clear count
    }

    // normalize fertility here
    if (fertileMode>0) {
      // obtain sum
      double fsum = 0.0;
      for (int i=0; i<FWords::maxFert; i++) fsum += fwords.fertility[i].cnt;
      // normalize
      for (int i=0; i<FWords::maxFert; i++) {
	const double cnt = fwords.fertility[i].cnt;
	ProbCnt& pc = e2f[eword].fertility[i];    // can be optimized ??
	pc.prob = (fsum>0) ? (cnt/fsum) : 0.0;
	pc.cnt = 0.0;
      }
    }

    // normalize lambda1
    double l1sum = fwords.lambda1cnt + fwords.lambda1ncnt;
    double l1prob = (l1sum>0) ? (fwords.lambda1cnt/l1sum) : 0.0;
    e2f[eword].lambda1prob = l1prob;

  } // end of for (EWordItr p=...)

  if (disableEqlFlag && useEqualProb) {
    // disable useEqualProb once updated
    // note: this should not be done on loading table
    cout << "disabling useEqualProb\n"; cout.flush();
    useEqualProb = false;
  }
}

void
TTable::DumpTables()
{
  // do nothing if no table file was given
  if (param->NoDumpFile || param->NoTableFile) return;

  for (EWordItr p = e2f.begin(); p != e2f.end(); p++) {
    const string eword = p->first;
    const FWords fwords = p->second;
    *dump << ": " << eword << "\n";

    // dump fertility 

    if (fertileMode>0) {
      // find max non-zero prob
      int maxIdx=-1;
      for (int i=0; i<FWords::maxFert; i++)
	if (fwords.fertility[i].prob>0) maxIdx=i;
      if (maxIdx>=0) {
	*dump << "F ";
	for (int i=0; i<=maxIdx; i++) {
	  double fertP = fwords.fertility[i].prob;
	  *dump << " " << fertP;
	}
	*dump << "\n";
      }
    }

    // dump lambda1 if -lambda1vary
    if (param->LambdaPhraseVary) {
      double l1prob = fwords.lambda1prob;
      if (l1prob > 0)
	*dump << "L " << l1prob << "\n";
    }

    // dump each fword and prob

    for (FWordItr q = fwords.words.begin(); q != fwords.words.end(); q++) {
      const string fword = q->first;
      const double prob = q->second.prob;
      if (prob>0)
	*dump << fword << " | " << prob << "\n";
    }
  }

  // close dump file
  delete dump;
}

//
// Lambda1 handling
//

//
// public methods
//

double
TTable::GetLambda1Prob(vector<Word>& eword)
{
  // if dummy table
  if (param->NoTableFile) return 0.0;

  // handle oldmode ?? (see TTable::GetProb())
  // ???

  // handle only elen>1
  if (eword.size()==1) return 0.0;

  // eword -> estr
  string estr;
  sout(estr,eword);

  // handle -eqlprob: assuming this value is used only when -lambda1vary
  if (useEqualProb) {
    if (useCutoff) {
      // need to call getLambda1Prob() just to check if estr exists
      if (getLambda1Prob(estr) == 0.0) return 0.0;
    }
    return 0.5;
  }
  
  return getLambda1Prob(estr);
}

void
TTable::AddLambda1Count(vector<Word>& eword, double val)
{
  if (param->NoTableFile || param->NoUpdate) return;

  if (eword.size()>1) {
    // eword -> estr
    string estr;
    sout(estr,eword);

    addLambda1Count(estr,val);
  }
}

void
TTable::AddLambda1nCount(vector<Word>& eword, double val)
{
  if (param->NoTableFile || param->NoUpdate) return;

  if (eword.size()>1) {
    // eword -> estr
    string estr;
    sout(estr,eword);

    addLambda1nCount(estr,val);
  }
}

//
// private methods
//

void
TTable::readLambda1Line(const string& estr, const string& line)
{
  // line = "L prob"

  double prob = atof(line.substr(2).c_str());
  FWords& fwords = e2f[estr];
  fwords.lambda1prob = prob;
  fwords.lambda1cnt = fwords.lambda1ncnt = 0.0;
}

double
TTable::getLambda1Prob(const string& eword)
{
  // use find() to avoid zero-prob entries
  EWordItr p = e2f.find(eword);
  if (p==e2f.end()) return 0.0;
  else {
    // retruns non-zero just to check if eword exists.
    if (useEqualProb && useCutoff) return 0.5;

    const FWords& fwords = p->second;
    return fwords.lambda1prob;
  }
}

void
TTable::addLambda1Count(const string& eword, double val)
{
  if (val>0) e2f[eword].lambda1cnt += val;
}

void
TTable::addLambda1nCount(const string& eword, double val)
{
  if (val>0) e2f[eword].lambda1ncnt += val;
}


