#include "misc.h"


Word
str2word(string& str)
{
  // convert decimal string to an integer

  Word ret = 0;
  for (int i=0; i<str.size(); i++) {
    ret = ret*10 + (str[i] - '0');
  }
  return ret;
}

void
str2words(string& str, vector<Word>& words)
{
  istrstream ist(str.c_str());
  string w;
  while(ist>>w) words.push_back(str2word(w));
}

void
sout(string &str, Word w)
{
  // append w to str
  int rem = w % 10;
  if (w>=10) sout(str,(w-rem)/10);
  str += ('0' + rem);
}

void
sout(string &str, vector<Word>& w)
{
  // convert sequence of words w to a string

  if (w.size()>0) {
    sout(str,w[0]);
    for (int i=1; i<w.size(); i++) {
      str += " ";
      sout(str,w[i]);
    }
  }
}

int
factorial(int numc)
{
  switch(numc) {
  case 0: return 1;
  case 1: return 1;
  case 2: return 2;
  case 3: return 6;
  case 4: return 24;
    //case 5: return 120;
  default: error_exit("unexpected factorial ", numc);
  }
  return 0;
}

bool ChineseMode = false;	// global var, but used only by misc.cc

void 
getValidFLen(int esize, int& minF, int& maxF)
{
  // due to hardware difference, +0.001 is needed

  if (ChineseMode) {
    minF = int(1.1 * esize - 4 + 0.001);
    maxF = int(1.7 * esize + 7 + 0.001);
  } else {
    minF = int(0.8 * esize - 2 + 0.001);
    maxF = int(1.4 * esize + 2 + 0.001);
  }
  if (minF<1) minF = 1;
}

void
error_exit(string msg)
{
  cerr << msg << "\n";
  cout << msg << "\n";
  exit(1);
}

void
error_exit(string msg, int n)
{
  cerr << msg << n << "\n";
  cout << msg << n << "\n";
  exit(1);
}

Params::Params(int argc, char* argv[])
  : argc(argc), argv(argv)
{
  Set(argc,argv);
}

void
Params::Set(int argc, char* argv[])
{
  // default params come here
  Verbose = 0;
  Sleep = 0;
  ChineseMode = false;
  
  NumIterate = 3;
  MinGraphServers = 2;
  NumTableServers = 21;
  TrainFile = "";
  TestFile = "";

  HostMaster = "localhost";

  TableIdx = 3;
  TableFile = "j4.tbl-";
  DumpFile = ".dmp";		// dump file suffix
  NoTableFile = false;
  NoDumpFile = false;
  NoUpdate = false;

  MaxVocabId = 65500;		// must fit in type Word 

  LambdaPhrase = 0.0;		// -L or -lambda1
  LambdaLeaf = 0.0;		// -M or -lmabda2
  LambdaPhraseConst = false;	// scaling lambda1
  LambdaPhraseVary = false;	// individual lambda1 as variable
  TrainFloorProb = 0.0;
  TestFloorProb = 1e-5;

  UseEqualProb = false;
  UseCutoff = false;
  ShareNullProb = false;
  FertileMode = 0;
  OldMode = OldMode2 = false;

  // parse args
  string arg;
  int i=0;
  while(++i<argc) {
    arg=argv[i];
    if (arg=="-v") Verbose=1;
    else if (arg=="-C") ChineseMode=true;
    else if (arg=="-sleep") Sleep=atoi(argv[++i]);
    else if (arg=="-n") NumIterate=atoi(argv[++i]);
    else if (arg=="-ng") MinGraphServers=atoi(argv[++i]);
    else if (arg=="-nt") NumTableServers=atoi(argv[++i]);
    else if (arg=="-idx") TableIdx=atoi(argv[++i]);
    else if (arg=="-tbl") TableFile=argv[++i];
    else if (arg=="-dmp") DumpFile=argv[++i];
    else if (arg=="-notbl") NoTableFile=true;
    else if (arg=="-nodmp") NoDumpFile=true;
    else if (arg=="-noupdate") NoUpdate=true;
    else if (arg=="-voce") VocabEFile=argv[++i];
    else if (arg=="-vocf") VocabFFile=argv[++i];
    else if (arg=="-sym") SymTblFile=argv[++i];
    else if (arg=="-maxvid") MaxVocabId=atoi(argv[++i]);
    else if (arg=="-lambda1") LambdaPhrase=atof(argv[++i]);
    else if (arg=="-lambda2") LambdaLeaf=atof(argv[++i]);
    else if (arg=="-lambda1const") LambdaPhraseConst=true;
    else if (arg=="-lambda1vary") LambdaPhraseVary=true;
    else if (arg=="-eqlprob") UseEqualProb=true;
    else if (arg=="-cutoff") UseCutoff=true;
    else if (arg=="-sharenull") ShareNullProb=true;
    else if (arg=="-fmode") FertileMode=atoi(argv[++i]);
    else if (arg=="-oldmode") OldMode=ShareNullProb=true;
    else if (arg=="-oldmode2") OldMode=OldMode2=ShareNullProb=true;
    else if (arg=="-tree") ShowTree=true;
    else if (arg=="-uprob") ShowUsedProbs=true;
    else if (arg=="-uprob2") ShowUsedProbs=ShowUsedProbsInPhrase=true;
    else if (arg=="-phprob") ShowPhraseProbs=true;
    else if (arg=="-showf0") ShowFertZero=true;
    else if (arg=="-host") HostMaster=argv[++i];
    else if (arg=="-train") TrainFile=argv[++i];
    else if (arg=="-test") TestFile=argv[++i];
    else if (arg=="-trainfloor") TrainFloorProb=atof(argv[++i]);
    else if (arg=="-testfloor") TestFloorProb=atof(argv[++i]);
    else {
      cerr << "Unrecognized option: " << arg << "\n";
	exit(1);
    }
  }

  // check param validity
  if (NumTableServers < 2) error_exit("more than one tservers needed");

}

void
Params::Print()
{
}

