
//
// Easy T-Table generator
//

// compile as g++ -DSPARCV9, if needed

// Usage: % co-count -mine 2 -maxe 2 -minf 1 -maxf 4 < x.bxi | gen-ttable > ttbl-2.txt

// Output Format: 
//    : e e e
//    f f
//    f f f 

// This code is the same as gen-ttable.cc, except removing unnecessary prob field
// from ProbCnt.

#include <string>
#include <map>
#include <iostream>

//
// misc function
//

bool isMultipleWord(const string& str)
{
  int spPos = str.find(' ');
  return (0<=spPos && spPos<str.size());
}


//
// Class Header
//

class ProbCnt {
public:
  //double prob,cnt;
  char cnt;
};

class FWords {
public:
  map<string,ProbCnt> words;
};

class TTable {
  map<string,FWords> e2f;
  typedef map<string,FWords>::const_iterator EWordItr;
  typedef map<string,ProbCnt>::const_iterator FWordItr;

public:
  //double GetProb(const string& eword, const string& fword);
  void PutCount(const string& eword, const string& fword, char val);
  void ClearCounts();
  void UpdateProbs();
  void GenerateTableFile(int cutoff, int cutoff2);
};

//double
//TTable::GetProb(const string& eword, const string& fword)
//{
//  return e2f[eword].words[fword].prob;
//}

void
TTable::PutCount(const string& eword, const string& fword, char val)
{
  const int Max = 10;
  char old = e2f[eword].words[fword].cnt;
  if (old<Max) e2f[eword].words[fword].cnt += val;
}

void
TTable::ClearCounts()
{
}

void
TTable::UpdateProbs()
{
}

void
TTable::GenerateTableFile(int cutoff, int cutoff2)
{
  for (EWordItr p = e2f.begin(); p != e2f.end(); p++) {
    const string& eword = p->first;
    const FWords& fwords = p->second;
    bool isSingleE = !isMultipleWord(eword);

    bool ewordPrinted=false;
    for (FWordItr q = fwords.words.begin(); q != fwords.words.end(); q++) {
      const string& fword = q->first;
      const ProbCnt& pc = q->second;
      bool isSingleF = !isMultipleWord(fword);

      // print if it's more than cutoff, or 1word-to-1word translation
      if (pc.cnt > cutoff || (isSingleE && isSingleF)) {
	if (cutoff2>0 && !isSingleE && pc.cnt <= cutoff2) continue;
	if (!ewordPrinted) cout << ": " << eword << "\n";
	ewordPrinted=true;
	cout << fword << "\n";
      }
    }
  }
}

//
// Process Info (CPU & Memory Usage)
//         (taken from Decode3/main.cc)
//     

#include <vector>
#include <fstream>
#include <strstream>

extern "C" {
#include <sys/times.h>
}

double
UTime() {
  struct tms tbuf;
  times(&tbuf);
  return ((double)tbuf.tms_utime/CLK_TCK);
}

#ifdef sun

extern "C" {
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
}

#ifdef SPARCV9

extern "C" {
#include <procfs.h>
#include <fcntl.h>
}

int
MemSize()
{
  struct pstatus pstat;
  int pid,fd;
  char fname[32];
  
  
  pid = getpid();
  sprintf(fname,"/proc/%d/status",pid);
  fd = open(fname,O_RDONLY);
  read(fd,&pstat,sizeof(pstat));
  close(fd);
  printf("getpid=%d,pid=%d,brksize=%d,stksize=%d\n",
	 pid,pstat.pr_pid,pstat.pr_brksize,pstat.pr_stksize);
  return pstat.pr_stksize + pstat.pr_brksize;
}

#else // not __sparcv9

int 
MemSize()
{
  struct stat buf;
  int pid = getpid();
  char fname[32];
  sprintf(fname,"/proc/%d",pid);
  stat(fname,&buf);
  return buf.st_size;
}
#endif // __sparcv9

#else // not sun (Linux)

extern "C" {
#include <stdio.h>
#include <unistd.h>
}

int
MemSize()
{
  int pid = getpid();
  char fname[32];
  sprintf(fname,"/proc/%d/status",pid);
  ifstream in(fname);
  if (!in) {
    cerr << "cannot open [" + string(fname) + "]\n";
    exit(1);
  }

  string VmSize;

  string line,w; vector<string> item;
  while(getline(in,line)) {
    istrstream ist(line.c_str());
    item.clear();
    while(ist>>w) item.push_back(w);
    if (item.size()>0) {
      if (item[0]=="VmSize:") {
	VmSize = item[1]; break;
      }
    }
  }
  int ret = atoi(VmSize.c_str());
  return ret;
}
#endif // sun

void
ShowMemory()
{
  cerr << "MemSize=" << MemSize();
  cerr << ",CPUTime=" << UTime() << "\n";
  cerr.flush();
}

//
// main
//

void
process_line(TTable *tbl, string& line)
{
  //cout << "Line[" << line << "]\n";

  string::size_type colon_pos = line.find(':');
  string eword = line.substr(0,colon_pos-1);
  string fword = line.substr(colon_pos+2);

  //cout << "EWord[" << eword << "], FWord[" << fword << "]\n\n";
  tbl->PutCount(eword,fword,1);

}


main(int argc, char* argv[])
{
  // default params
  int cutoff = 0;
  int cutoff2 = 0;
  
  // parse args
  int i=0;
  while(++i<argc) {
    string arg=argv[i];
    if (arg=="-cutoff") cutoff=atoi(argv[++i]);
    else if (arg=="-cutoff2") cutoff2=atoi(argv[++i]);
    else {
      cerr << "Unrecognized argument: " << arg << "\n";
    }
  }

  TTable *tbl = new TTable();

  // read co-count output from stdin
  string line; int numlines=0;
  while(getline(cin,line)) {
    numlines++;
    process_line(tbl,line);
  }
  
  // generate output
  tbl->GenerateTableFile(cutoff,cutoff2);
  
  // show memory usage to stderr
  ShowMemory();
  
  exit(0);
}
