#ifndef GRAPH_H
#define GRAPH_H

#include <string>
#include <vector>
#include <algorithm>

#include "misc.h"
#include "tree.h"

// originally written for dep-em3.cc

//
// TNode
//

class TNode {
public:
  int id,numr,epos,maxjlen;

  int elen;			// new: used by Graph::CollectPhraseE()
  string label;			// new: used by TNode::ShowNodeLabel(),
				//      and set by Graph::SetStringInfo()
#if 1
  double lambda1prob,lambda1cnt,lambda1ncnt; // lambda1 can be trained.
#else
  double lambda1;		// new: scaled lambda1 by elen
#endif

  Tree *node;
  vector<TNode*> child;

  // new: probT is moved from Graph
  static const int MaxFLen = Params::MaxSentFLen;
  double probT[MaxFLen][MaxFLen];	// probT[posF][lenF]
  double countT[MaxFLen][MaxFLen];

  // copy of prob table (tableT is in Graph)
  double probR[Params::MaxReorder];

  // count used for prob table update
  double countR[Params::MaxReorder];

  // new: non-shared NULL-insert probs (NULL -> F)
  double probInsNone;
  double probInsRight[MaxFLen];
  double probInsLeft[MaxFLen];

  double countInsNone;
  double countInsRight[MaxFLen];
  double countInsLeft[MaxFLen];

  inline double getInsProb(int insIdx, int inspos);
  inline void addInsCount(int insIdx, int inspos, double val);

  inline double getInsNoneCount(double prob=1.0);
  inline double getInsRightCount(int inspos, double prob=1.0);
  inline double getInsLeftCount(int inspos, double prob=1.0);

  TNode(Tree*);
  ~TNode();

  void Setup(double lambda1, bool lambda1const, bool lambda1vary, int epos=0);
				// set epos,elen,numr,maxjlen (id must be set beforehand)
				// new: scaled lambda1 is set here
				// new: varying lambda1 is set default here
				// (obtain real value from t-server)
  
  int numNodes();

  int numc() { return child.size(); }
  bool isLeaf() { return (child.size()==0); }
  void reorder(int xr, TNode *cn[]);
  int reorder(int xr, int i);

  void cacheProbRN();
  void flushCountRN();
  void showReorderIdx(int);

  // new methods
  void CollectTNodes(vector<TNode*>&);
  void showNodeLabel();		// debug or backward compatibility
  void setFloorProb(double);	// add floor probs
};

//
// Graph
//

class Graph;
class GraphNode;
class GraphArc;
class Partition;

//
// class PartitionVec allows capacity adjastable vector, see GraphNode::PrunePrevs().
// used instead of vector<Partition *>, and have reassign() method.
//

class PartitionVec {
  typedef Partition * PartitionPt;
public:
  vector<Partition *>* vec;

  PartitionVec() { vec = new vector<Partition *>; }
  ~PartitionVec() { delete vec; }
  PartitionPt& operator[](int idx) { return (*vec)[idx]; }
  PartitionPt& operator[](double idx) { return (*vec)[int(idx)]; }
  int size() { return (*vec).size(); }
  int capacity() { return (*vec).capacity(); }
  void push_back(Partition *pt) { (*vec).push_back(pt); }
  void reassign(PartitionVec& newvec) {
    delete vec;
    vec = new vector<Partition *>;
    *vec = *(newvec.vec);
  }
};

// 
// Graph main classes
//

class Graph {
  static const int MaxELen = Params::MaxSentELen;
  static const int MaxFLen = Params::MaxSentFLen;
  typedef GraphNode* GraphNodePt;
  static const GraphNodePt notYetCreated = (GraphNode *)(-1);
  friend class GraphArc;	// allow GraphArc to access Graph's private member
  friend class GraphNode;	// allow GraphNode::setArcProb() access esent
  friend class Partition;	// allow Partition::Partition() access notYetCreated

  Params *param;
  vector<string> jsent;
  vector<string> esent;
  TNode *troot;
  int numtnodes;
  vector<TNode*> tnodeList;	// new: used by SetTablesRN()

  GraphNode *groot;		// root of the graph

  static const int maxId = Params::MaxSentELen*2;
  static const int maxFPos = Params::MaxSentFLen+1;
  static const int maxFLen = Params::MaxSentFLen+1;
  GraphNode *gn[maxId][maxFPos][maxFLen];

  double probNT[MaxFLen];		// NULL -> F probs
  double countNT[MaxFLen];

public:
  Graph(Tree *tree, vector<Word>& wordsE, vector<Word>& wordsF, Params*);
  ~Graph();
  
  void AllocGraphNodes();
  void DeallocGraphNodes();

  void SetAlphaBeta();
  double GetTotalProb();
  double GetBestProb();
  void ShowBest();
  void ShowSents();
  void showBestAlignment();
  void showTransIns(TNode*,int,int);
  void showTrans(int,int,int,int);
  void ShowPhraseProbs();

#if 0
  void ShowAlignment();
  void ShowAlignmentList();
#endif

  int ESentSize() { return esent.size(); }
  int JSentSize() { return jsent.size(); }
  bool SentLengthOK();
  void Print();

private:
  void allocGraphNode(TNode *node, int len);

  void setAlpha(TNode *node);
  void setBeta(TNode *node);

  void setArcProb(TNode *node);
  void getArcCount(TNode *node);

  void setBestBeta(TNode *node);

public:
  // new methods
  void SetStringInfo(vector<string>&,vector<string>&,vector<string>&);
  void CollectPhraseE(vector<int>&, TNode *node=NULL);
  void FindTNodes(int posE, int lenE, vector<TNode*>&);

  void CacheProbs(vector<int>&, vector<double>&);
  void FlushCounts(double,vector<int>&,vector<double>&);
  void CacheProbsRN(vector<int>&, vector<double>&);
  void FlushCountsRN(double,vector<int>&,vector<double>&,int&);

  void SetArcProb() { setArcProb(troot); }
  void GetArcCount() { getArcCount(troot); }
  void SetFloorProb(bool);

  // show f0
  void ShowFertZero();
};

class GraphNode {
  friend class Graph;		// class Graph can access GraphNode's alpha/beta.
  friend class GraphArc;	// class GraphArc can access GraphNode's beta.
  friend class Partition;	// class Partition can access GraphNode's prevs.
  int nid,pos,len;		// nodeId,posJ,lenJ
  double alpha,beta;
  vector<GraphArc *> arcs;	// indexed by idxReorder*3+idxNull (idxReorder=0 for leaf).
  PartitionVec prevs;
public:
  GraphNode(Graph *graph, TNode *node, int pos, int len);
  ~GraphNode();

  inline void PrunePrevs();
  inline void PruneArcs();

  inline void setAlpha();
  // new: to handle extra phrase arcs
  inline void setBeta(TNode *tnode);
  void setArcProb(Graph *graph, TNode *tnode);
  void getArcCount(Graph *graph, TNode *tnode);

  inline void setBestBeta(TNode *tnode);
  void showBest(Graph *graph, bool showReord, TNode *tnode, int indent);
  void showRNProb(TNode *tnode);
  void showReorderIdx(TNode *tnode);
#if 0
  void showAlignment(Graph *graph, TNode *tnode);
  void showAlignmentList(Graph *graph, TNode *tnode, bool showSrc);
#endif
  void showBestAlignment(Graph *graph, TNode *tnode);
  void showUsedProbs(Graph *graph, TNode *tnode);
  void showUsedProbT(Graph*,TNode*);
  void showUsedProbT(Graph*,TNode*,GraphArc*);
  void showUsedProbPhraseT(Graph*,TNode*,GraphArc*);

  void showPhraseProbs(Graph*,TNode*);
  void showPhraseProbT(Graph*,TNode*);
  void showPhraseProbT(Graph*,TNode*,GraphArc*);

  void Print();

  // new (for phrase level translation)
  bool hasPhraseArcs(TNode *tnode, int& offset);
  inline bool isPhraseArc(TNode *tnode, int idx) { return (idx >= tnode->numr*3); }
  GraphArc *FindBestPhraseArc(int offset, int& ixBest, double& maxP);
  GraphArc *FindBestRegularArc(int offset, TNode *tnode, double& maxR);
#if 1
  GraphArc *FindBestRegularArc(TNode *tnode, int& rIdx, int& insIdx);
#else
  GraphArc *FindBestRegularArc(TNode *tnode, int& rIdx);
#endif

  inline void setArcProbPhrase(Graph *graph, TNode *tnode);
  inline void getArcCountPhrase(Graph *graph, TNode *tnode);
  void showBestPhrase(Graph *graph, TNode *tnode, int indent);
  inline void showBestIndent(int);

  // show f0
  bool nonUnaryAllFertZeroExists(Graph *graph, TNode *tnode);
  bool allFertZero(Graph *graph, TNode *tnode);
  void showFertZero(Graph *graph, TNode *tnode);
  void showFertZeroRule(Graph *graph, TNode *tnode, vector<bool>&, vector<GraphNode*>&);
  void showFertZeroWord(Graph *graph, TNode *tnode);
};

class GraphArc {
  friend class GraphNode;	// GraphNode can access GraphArc's from;
  GraphNode *from;		// backlink to node
  int idxNull, idxReorder;	// used for handling prob table??
  int inspos;			// posJ for inserted word
  double prob;			// P(ins) [* P(insWord)] * (P(reorder) or P(word))
  double rBeta;			// raw beta (Node's beta is weighted sum of this).
  int npos,nlen;		// posJ/lenJ for children or leaf excl. insertion
  PartitionVec pttn;
public:
  GraphArc(Graph *graph, GraphNode *frm, TNode *node, int pos, int len, int xi, int xr);
  ~GraphArc();
private:
  void allocPartitions(Graph *graph, TNode *node, int xr);
  inline void PrunePartitions();

  inline void setAlpha();
  inline void setBeta(bool isTerminal);

  void Print();
};

class Partition {
  friend class GraphNode;	// GraphNode can access Partition's arc.
  friend class GraphArc;	// GraphArc can access Partition's child.
  GraphArc *arc;		// backlink to arc
  int numc;
  GraphNode *child[Params::MaxChildren];	// link to next graph node
public:
  Partition(Graph *graph, GraphArc *arc0, GraphNode* gn[], TNode* cn[], int pos[], int len[], int clen);
  inline double getSideBeta(GraphNode *node);
  void Print();
};

#endif // GRAPH_H
