#include "tpair.h"

TrainPair::TrainPair(int pairId, string& lineE, string& lineF, Params *param)
  : pairId(pairId), lineE(lineE), lineF(lineF)
{
  // create time
  cpuTime = UTime();

  // create a parse tree
  troot = new Tree(lineE);

  // create a node list
  nodeList.clear();
  troot->AddNodeList(nodeList);

  // convert line into words
  collectEWords(troot);		// set wordE
  str2words(lineF,wordF);

#if DEBUG
  cout << "wordE.size()=" << wordE.size()
       << ",wordF.size()=" << wordF.size() << "\n";
#endif

  // create a graph
  graph = new Graph(troot,wordE,wordF,param);

  // need to set graph->esent, graph->fsent
}

TrainPair::~TrainPair()
{
  delete troot;
  delete graph;
}

void
TrainPair::collectEWords(Tree *node)
{
  if (node->child.size()==0)
    wordE.push_back(node->head);
  else
    for (int i=0; i<node->child.size(); i++)
      collectEWords(node->child[i]);
}

void
TrainPair::collectSymbols(Tree *node, vector<Word>& words)
{
  words.push_back(node->symbol);
  for (int i=0; i<node->child.size(); i++)
    collectSymbols(node->child[i],words);
}

void
TrainPair::CollectRules(vector<int>& numcArr, vector<int>& symArr, Tree *node=NULL)
{
  // pack CFG rules (for ProbRN)

  if (node==NULL) node=troot;

  // push numc
  int numc = node->child.size();
  numcArr.push_back(numc);

  // push sym
  symArr.push_back(node->symbol);
  for (int i=0; i<numc; i++)
    symArr.push_back(node->child[i]->symbol);

  // recursively call children
  for (int i=0; i<numc; i++)
    CollectRules(numcArr,symArr,node->child[i]);
}
    
