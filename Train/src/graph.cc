#include "graph.h"

// Note for -lambda1vary:
// Varying prob and count for lambda1 are handled only when -lambda1vary is set.
// t-server must be supplied -lambda1vary too, and t-server will send lambda1 prob
// as flen=FLEN_LAMBDA1(=127) and fpos=FPOS_L1(=127) for specific (epos,elen).
// When returning count for arc with (1-lambda1), fpos=FPOS_L1N(=126) is used.
// g-server will return counts for lambda1 and (1-lambda1) only when t-server
// returns prob with lambda1.


//
// TNode
//

TNode::TNode(Tree *tree)
  : node(tree)
{
  // copy whole tree from node
  for (int i=0; i<tree->child.size(); i++) {
    TNode *nd = new TNode(tree->child[i]);
    child.push_back(nd);
  }

  // clear prob tables (table server may not return any value)
  for (int i=0; i<MaxFLen; i++)
    for (int j=0; j<MaxFLen; j++)
      probT[i][j]=countT[i][j]=0.0;
  probInsNone=countInsNone=0.0;
  for (int i=0; i<MaxFLen; i++) {
    probInsRight[i]=countInsRight[i]=0.0;
    probInsLeft[i]=countInsLeft[i]=0.0;
  }
  for (int i=0; i<Params::MaxReorder; i++)
    probR[i]=countR[i]=0.0;

  // lambda1
  lambda1prob = lambda1cnt = lambda1ncnt = 0.0;

}

TNode::~TNode()
{
  // delete children before delete myself
  for (int i=0; i<child.size(); i++)
    delete child[i];
}

void
TNode::Setup(double lx1, bool lx1const, bool lx1vary, int epos=0)
{
  // set epos,elen,numr,maxjlen  
  // note: id must be set already
  // new: scaled lambda1 is set here (from lx1)

  // new: varying lambda1 is set default here
  // (obtain real value from t-server)

  // set numr
  numr = factorial(numc());

  // set epos,elen,maxjlen

  if (isLeaf()) {
    this->epos=epos;
    this->elen=1;
    //this->maxjlen=2;
  } else {
    int sumelen=0, pos1=epos, sumjlen=0;
    for (int i=0; i<child.size(); i++) {
      TNode *cp = child[i];
      cp->Setup(lx1,lx1const,lx1vary,pos1);
      sumelen += cp->elen;
      //sumjlen += cp->maxjlen;
      pos1 += cp->elen;
    }
    this->epos = epos;
    this->elen = sumelen;
    //this->maxjlen = sumjlen+1;	// nonterminal can add extra jword
  }      

  // now maxjlen is determined by getValidFLen()
  int minF,maxF;
  getValidFLen(elen,minF,maxF);
  this->maxjlen = maxF;

  // set scaled or varying lambda1 (lx1 should be params->lambda1)
  if (lx1const) {
    lambda1prob = lx1;
  } else if (lx1vary) {
    // sets default here. real value will be obtained from t-server
    lambda1prob=0.0;
  } else {
    lambda1prob=1.0;			
    for (int i=0; i<elen; i++) lambda1prob *= lx1;
  }
}

int
TNode::numNodes()
{
  int n=1;
  for (int i=0; i<numc(); i++)
    n += child[i]->numNodes();
  return n;
}

void
TNode::reorder(int xr, TNode *cn[])
{
  // sets reordered children in cn, according to xr
  for (int i=0; i<numc(); i++) {
    cn[i] = child[reorder(xr,i)];
  }
}

inline int
TNode::reorder(int xr, int i)
{
  // returns new position for i-th child, according to xr.
  // same as Node::reorder(int)

  static const int mtx1[1][1] = {{0}};
  static const int mtx2[2][2] = {{0,1},{1,0}};
#if 0
  static const int mtx3[6][3] = {
    {0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
  static const int mtx4[24][4] = {
    {0,1,2,3},{0,1,3,2},{0,2,1,3},{0,2,3,1},{0,3,1,2},{0,3,2,1},
    {1,0,2,3},{1,0,3,2},{1,2,0,3},{1,2,3,0},{1,3,0,2},{1,3,2,0},
    {2,0,1,3},{2,0,3,1},{2,1,0,3},{2,1,3,0},{2,3,0,1},{2,3,1,0},
    {3,0,1,2},{3,0,2,1},{3,1,0,2},{3,1,2,0},{3,2,0,1},{3,2,1,0}};
#else
  static int mtx3[6][3],mtx4[24][4],mtx5[120][5];
  static bool need_init = true;

  if (need_init) {
    need_init = false;
    // set mtx3
    for (int i=0; i<3; i++) mtx3[0][i]=i;
    for (int j=1; j<6; j++) {
      for (int i=0; i<3; i++) mtx3[j][i]=mtx3[j-1][i];
      int *pt = &(mtx3[j][0]);
      next_permutation(pt,pt+3);
    }
    // set mtx4
    for (int i=0; i<4; i++) mtx4[0][i]=i;
    for (int j=1; j<24; j++) {
      for (int i=0; i<4; i++) mtx4[j][i]=mtx4[j-1][i];
      int *pt = &(mtx4[j][0]);
      next_permutation(pt,pt+4);
    }
    // set mtx5
    for (int i=0; i<5; i++) mtx5[0][i]=i;
    for (int j=1; j<120; j++) {
      for (int i=0; i<5; i++) mtx5[j][i]=mtx5[j-1][i];
      int *pt = &(mtx5[j][0]);
      next_permutation(pt,pt+5);
    }
  }
#endif

  switch (numc()) {
  case 1: return mtx1[xr][i];
  case 2: return mtx2[xr][i];
  case 3: return mtx3[xr][i];
  case 4: return mtx4[xr][i];
#if 0
  case 5: return mtx5[xr][i];
#endif
  default: error_exit("unexpected TNode::reorder(xr,i)");
  }
}

void
TNode::showNodeLabel()
{
  cout << label;
}

//
// Graph
//

Partition::Partition(Graph *graph, GraphArc *arc0, GraphNode* gn[], TNode* cn[], int pos[], int len[], int clen)
  : arc(arc0), numc(clen)
{
  for (int i=0; i<clen; i++) {
    GraphNode *gnode = gn[i];
    // create gnode on-demand (also set passed array, gn[])
    if (gnode==graph->notYetCreated) gnode = gn[i] = new GraphNode(graph,cn[i],pos[i],len[i]);
    // set this
    child[i]=gnode;
    gnode->prevs.push_back(this);
  }
}

Graph::Graph(Tree *tree, vector<Word>& ewords, vector<Word>& jwords, Params *param)
  : param(param)
{
  // copy Nodes to TNodes. 
  // set tree root and numtnodes
  troot = new TNode(tree);
  numtnodes = troot->numNodes();
  
  // new: collect all TNodes, and set id
  troot->CollectTNodes(tnodeList);
  for (int i=0; i<tnodeList.size(); i++)
    tnodeList[i]->id = i;

  // set numr,epos,elen,maxjlen and lambda1
  troot->Setup(param->LambdaPhrase,param->LambdaPhraseConst,param->LambdaPhraseVary);

  // new: SentLengthOK() must be called after SetStringinfo().

  // AllocGraphNodes() must be called later.
}

bool
Graph::SentLengthOK()
{
  // first check if it can fit to Graph data structure, esp. gn[id][pos][len]
  // and probT[posE][posJ].

#if 0
  cout << "numtnodes=" << numtnodes << ",jsent.size()=" << jsent.size()
       << ",esent.size()=" << esent.size() << ",maxflen=" << MaxFLen
       << ",maxjlen=" << troot->maxjlen << "\n";
#endif

  // ignore if jsent's length=0
  if (jsent.size()==0) return false;

  if (numtnodes > maxId || jsent.size()+1 > MaxFLen || esent.size()+1 > MaxELen)
    return false;

  // next check if jsent length is short enoght to cover by sent length.
  if (troot->maxjlen < jsent.size())
    return false;

  // if no phrase-translation nor 1->many t-table are possible, max insertion
  // is determined by the number of tnodes
  if (param->LambdaPhrase==0 && param->LambdaLeaf==0 &&
      esent.size() + numtnodes < jsent.size())
    return false;

  // if only phrase-translation is enabled, but Elen==1, i.e. all unary nodes,
  // then FLen must be less than Elen + numtnodes (since no phrasal at unary
  // nodes and leaf node)
  if (param->LambdaPhrase!=0 && param->LambdaLeaf==0 && esent.size() == 1 &&
      esent.size() + numtnodes < jsent.size())
    return false;

  // otherwise OK.
  return true;
}

void
Graph::AllocGraphNodes()
{
  // allocate GraphNodes from top-down.
  // create it on-demand, in GraphArc::allocPartitions()

  // reset all gn[] (must be zero'd later for unused one)
  int numid=numtnodes, maxpos=jsent.size(), maxlen=jsent.size();

  for (int id=0; id<numid; id++)        // not inclusive
    for (int pos=0; pos<=maxpos; pos++)   // max inclusive
      for (int len=0; len<=maxlen; len++)	// max inclusive
	gn[id][pos][len]=this->notYetCreated;

  // create top node (lower node created in GraphArc::allocPartitions()
  // and set groot
  int id=troot->id, pos=0, len=jsent.size();
  groot = new GraphNode(this,troot,pos,len); // gn[id][pos][len] is set
				// in GraphNode::GraphNode()

  // zero gn[] for unused one
  for (int id=0; id<numid; id++)        // not inclusive
    for (int pos=0; pos<=maxpos; pos++)   // max inclusive
      for (int len=0; len<=maxlen; len++) {	// max inclusive
	GraphNode *gnode = gn[id][pos][len];
	if (gnode == this->notYetCreated)
	  gn[id][pos][len]=0;
#if DEBUG
	if (id==0 && pos+len<=6 && gn[id][pos][len]!=0)
	  cout << "GraphNode[" << id << "][" << pos << "][" << len << "] allocated\n";
#endif
      }
}

void
Graph::DeallocGraphNodes()
{
  // delete all gn[]
  int numid=numtnodes, maxpos=jsent.size(), maxlen=jsent.size();

  for (int id=0; id<numid; id++)        // not inclusive
    for (int pos=0; pos<=maxpos; pos++)   // max inclusive
      for (int len=0; len<=maxlen; len++) {	// max inclusive
	GraphNode *gnode = gn[id][pos][len];
	if (gnode) {
	  delete gnode;
	  gn[id][pos][len] = 0;
	}
      }
}

GraphNode::GraphNode(Graph *graph, TNode *node, int pos0, int len0)
  : nid(node->id), pos(pos0), len(len0)
{
  alpha=beta=0.0;
  graph->gn[node->id][pos0][len0] = this;
  int numR = node->numr;

  // new: handle extra phrase arcs
  if (node->numc() > 1) numR++;// not allow phrase for unary node

  for (int xr=0; xr<numR; xr++) 
    for (int xi=0; xi<3; xi++) {
      // if len==0, then only xi==1 is allowed
      GraphArc *ga = (len==0 && xi!=0) ? NULL :
	new GraphArc(graph,this,node,pos,len,xi,xr);
      arcs.push_back(ga);
    }
}

GraphArc::GraphArc(Graph *graph, GraphNode *frm, TNode *node, int pos0, int len0, int xi, int xr)
  : from(frm), idxNull(xi), idxReorder(xr)
{
  // set npos,nlen,inspos
  switch (xi) {
  case 0: npos=pos0; nlen=len0; inspos=-1; break; // NONE
  case 1: npos=pos0; nlen=len0-1; inspos=pos0+len0-1; break; // RIGHT
  case 2: npos=pos0+1; nlen=len0-1; inspos=pos0; break; // LEFT
  default: error_exit("invalid idxNull");
  }

  // alloc and push_back pttn
  // new: handle extra phrase arcs
  if (!node->isLeaf() && xr != node->numr)
    allocPartitions(graph,node,xr);

  // new: for extra phrase arcs (for safety)
  prob = 0.0;			// dummy
}

void
GraphArc::allocPartitions(Graph *graph, TNode *node, int xr)
{
  static const int maxc = Params::MaxChildren;
  TNode *cn[maxc]; GraphNode* gn[maxc]; Partition *npt;
  int pos[maxc],len[maxc],id[maxc],max[maxc];
  const int numc=node->numc(); 

  // need special code for numc==1
  if (numc==1) {
    cn[0]=node->child[0];
    if (0<=nlen && nlen<=cn[0]->maxjlen) {
      id[0]=cn[0]->id; pos[0]=this->npos; len[0]=this->nlen;
      gn[0]=graph->gn[id[0]][pos[0]][len[0]];
      npt = new Partition(graph,this,gn,cn,pos,len,1);
      this->pttn.push_back(npt);
    }
    return;
  }

  // set reordered children in cn[], according to xr
  node->reorder(xr,cn);		

  // set id[] and max[]
  for (int i=0; i<numc; i++) {
    id[i] = cn[i]->id; max[i] = cn[i]->maxjlen;
  }
  
  pos[0] = this->npos;
  int carryPos=0, slen=0;	// slen == sum(0..carryPos-1) of len[]
  do {
    // reset to min length for carryPos to end-2
    const int lastPos = numc-1;
    for (int i=carryPos; i<lastPos-1; i++) {
      pos[i]=pos[0]+slen; len[i]=0;
      gn[i]=graph->gn[id[i]][pos[i]][len[i]];
    }
    int sslen = slen;		// sum(0..lastPos-2) of len[]
    int rlen = this->nlen - sslen;     // len[lastPos-1] + len[lastPos]

    // set for lastPos-1 and lastPos
    int min2 = rlen - max[lastPos]; if (min2<0) min2=0;
    int max2 = max[lastPos-1]; if (max2>rlen) max2=rlen;

    int &len2 = len[lastPos-1], &pos2 = pos[lastPos-1], &id2 = id[lastPos-1];
    int &len1 = len[lastPos], &pos1 = pos[lastPos], &id1 = id[lastPos];
    pos2 = pos[0]+sslen; len2 = min2; 
    pos1 = pos2 + min2; len1 = rlen - len2; 
    for (; len2<=max2; len2++,pos1++,len1--) {
      gn[lastPos-1]=graph->gn[id2][pos2][len2];
      gn[lastPos]=graph->gn[id1][pos1][len1];
      npt = new Partition(graph,this,gn,cn,pos,len,numc);
      this->pttn.push_back(npt);
    }

    // next partition
    int cpos=lastPos-2;
    while (cpos>=0 && len[cpos]==max[cpos]) cpos--; // find carry propagation limit
    if (cpos<0) break;		// exit do-loop
    len[cpos]++; gn[cpos]=graph->gn[id[cpos]][pos[cpos]][len[cpos]];
    slen = pos[cpos]+len[cpos]-pos[0];
    carryPos = cpos+1;
  } while (true);
}

//
// Interface to/from Prob Tables
//

// new: use new CacheProbs() and CacheProbsRN()

void
Graph::setArcProb(TNode *node)
{
  // set all GraphNode for this TNode
  int id=node->id, max=jsent.size();

  for (int pos=0; pos<=max; pos++) // max inclusive
    for (int len=0; pos+len<=max; len++) {
      GraphNode *gnode = gn[id][pos][len];
      if (gnode) gnode->setArcProb(this,node);
    }
  
  // set for children
  for (int i=0; i<node->numc(); i++) setArcProb(node->child[i]);
}

void
GraphNode::setArcProb(Graph *graph, TNode *tnode)
{
  int posE = tnode->epos;
  int nullEpos=graph->esent.size(), nullJpos=graph->jsent.size();
  // new: handle extra phrase arcs
  const double lambda1 = tnode->lambda1prob;
  const double lambda2 = graph->param->LambdaLeaf;
  setArcProbPhrase(graph,tnode);

  for (int rx=0; rx<tnode->numr; rx++) {
    double reorderP = tnode->probR[rx];
    for (int ix=0; ix<3; ix++) {
      GraphArc *ap = arcs[rx*3+ix];
      if (ap) {
	double insP = tnode->getInsProb(ix,ap->inspos);
	if (tnode->isLeaf()) {
	  double wordP = 0.0;
	  if (ap->nlen==0) {
	    // insertion allowed only nlen!=1
	    // [ap->npos][0] is bad, since ap->npos can be jsent.size()
	    if (ix==0) wordP = tnode->probT[0][0];// E -> NULL
	  } else {
	    wordP = tnode->probT[ap->npos][ap->nlen];
	  }	    
	  if (ap->nlen>1) wordP *= lambda2;
	  ap->prob = insP * wordP;
	} else {
	  ap->prob = insP * reorderP;
	  ap->prob *= (1.0 - lambda1);
	}
#if 0
	cout << "id=" << tnode->id << ",rx=" << rx << ",ix=" << ix
	     << ",epos=" << tnode->epos << ",elen=" << tnode->elen
	     << ",npos=" << ap->npos << ",nlen=" << ap->nlen
	     << ",prob=" << ap->prob << "\n";
#endif
      }
    }
  }
}

// new: use new FlushCounts() and FlushCountsRN()

void
Graph::getArcCount(TNode *node)
{
  // similar to Graph::setArcCount()
  // get count and put into local count for this TNode
  int id=node->id, max=jsent.size();

  for (int pos=0; pos<=max; pos++) // max inclusive
    for (int len=0; pos+len<=max; len++) {
      GraphNode *gnode = gn[id][pos][len];
      if (gnode) gnode->getArcCount(this,node);
    }

  // get count for children
  for (int i=0; i<node->numc(); i++) getArcCount(node->child[i]);
}

void
GraphNode::getArcCount(Graph *graph, TNode *tnode)
{
  // similar to GraphNode::setArcProb()
  int posE = tnode->epos;
  int nullEpos=graph->esent.size(), nullJpos=graph->jsent.size();

  // new: handle extra phase arcs
  getArcCountPhrase(graph,tnode);

  for (int rx=0; rx<tnode->numr; rx++)
    for (int ix=0; ix<3; ix++) {
      GraphArc *ap = arcs[rx*3+ix];
      if (ap) {
	double cnt = alpha * ap->prob * ap->rBeta;	// ok??
	if (cnt>0) {
	  tnode->addInsCount(ix,ap->inspos,cnt);
	  if (tnode->isLeaf()) {
	    if (ap->nlen==0) {
	      // see setArcProb() for why [0][0] is used.
	      tnode->countT[0][0] += cnt;
	    } else {
	      tnode->countT[ap->npos][ap->nlen] += cnt;
	    }
	  } else {
	    tnode->countR[rx] += cnt;
	    tnode->lambda1ncnt += cnt;
	  }
	}
      }
    }
}

//
// Alpha/Beta prob settings
//

void
Graph::SetAlphaBeta()
{
  // use TNode's hiearchy to ensure setting order (similar to Graph::allocGraphNodes()).
  // all beta must be set before setting alpha.
  // children beta must be set first
  // parent alpha must be set first

  setBeta(troot);
  setAlpha(troot);
}  
  
void
Graph::setAlpha(TNode *node)
{
    // this must be set before children
    int id=node->id, max=jsent.size();
    
    for (int pos=0; pos<=max; pos++)      // max inclusive
      for (int len=0; pos+len<=max; len++) {
	GraphNode *gnode = gn[id][pos][len];
	if (gnode) {
	  if (node==troot)
	    gnode->alpha = (pos==0 && len==max) ? 1.0 : 0.0;
	  else
	    gnode->setAlpha();
	}
      }
    // set for children
    for (int i=0; i<node->numc(); i++) setAlpha(node->child[i]);
}

inline void
GraphNode::setAlpha()
{
  // assume parent alpha and all beta are set already
  int npt = prevs.size();
  if (npt==0) alpha = 0.0;	// root is set by Graph::setAlpha()
  else {
    // alpha = Sum(pttn)[alpha(parent)*P(i/r)*Prod(sibling beta)]
    double sum = 0.0;
    for (int i=0; i<npt; i++) {
      Partition *pt = prevs[i];
      GraphArc *ac = pt->arc;
      if (ac) {
	GraphNode *parent = ac->from;
	double sib = pt->getSideBeta(this);
	double prod = parent->alpha * ac->prob * sib;
	sum += prod;
      }
    }
    alpha = sum;
  }
}

inline double
Partition::getSideBeta(GraphNode *node)
{
  double prod = 1.0;
  for (int i=0; i<numc; i++) {
    GraphNode *nd = child[i];
    if (nd && nd!=node) prod *= nd->beta;
  }
  return prod;
}

void
Graph::setBeta(TNode *node)
{
  // children must be set first
  for (int i=0; i<node->numc(); i++) setBeta(node->child[i]);

  // set this beta
  int id = node->id;
  int max=jsent.size();

  for (int pos=0; pos<=max; pos++)       // max inclusive
    for (int len=0; pos+len<=max; len++) {
      GraphNode *gnode = gn[id][pos][len];

      // new: handle extra phrase arcs
      if (gnode) gnode->setBeta(node);
    }
}

inline void
GraphNode::setBeta(TNode *tnode)
{
  // new: handle extra phrase arcs

  // assume children beta are set by Graph::setBeta()
  // first, set beta for arcs, then get weighted sum.
  double sum=0.0;
  for (int i=0; i<arcs.size(); i++) {
    GraphArc *ap = arcs[i];
    if (ap) {
      // new: handle extra phrase arcs
      ap->setBeta(tnode->isLeaf() || isPhraseArc(tnode,i));

      sum += (ap->rBeta * ap->prob); // ap->prob = P(i/r) for the arc
    }
  }
  beta = sum;
}

inline void
GraphArc::setBeta(bool isTerminal)
{
  // obtain intermediate beta (i.e. sum of beta under pttn)
  if (isTerminal) rBeta = 1.0;
  else {
    double sum=0.0;
    for (int i=0; i<pttn.size(); i++) {
      Partition *pt = pttn[i];
      double prod=1.0;
      for (int j=0; j<pt->numc; j++) {
	GraphNode *nd = pt->child[j];
	if (nd) prod *= nd->beta;
      }
      sum += prod;
    }
    rBeta = sum;
  }
}

double
Graph::GetTotalProb()
{
  return groot->beta;
}

//
// Show Best prob/alignment
//

double
Graph::GetBestProb()
{
  // destroy beta probs on the graph
  // assume SetAlphaBeta() is already called

  this->setBestBeta(troot);
  return(groot->beta);
}

#if 1
#define NEW_BEST_FMT 1
#endif

void
Graph::ShowBest()
{
#if NEW_BEST_FMT
  // tree header is now separately printed by calling ShowSents()
#if 0
  ShowSents();
#endif
#endif // NEW_BEST_FMT

  // show total prob and best prob
  double totProb = groot->beta;	// save before used in setBestBeta()
  this->setBestBeta(troot);
  double bestProb = groot->beta;

#if NEW_BEST_FMT
  // print alignment data (to be used by xalign2.pl)
#if 1
  if (totProb>0) showBestAlignment();
  else cout << "A: n/a\n\n";
#else
  cout << "A: ";
  groot->showBestAlignment(this,troot);
  cout << "\n";
#endif
#endif // NEW_BEST_FMT

  cout << "(Prob=" << bestProb << " [" << bestProb/totProb << "])\n\n";
#if DEBUG
  cout << "totProb=" << totProb << ",bestProb=" << bestProb << "\n";
#endif

  if (totProb>0) {		// prob may be zero!
    // show probs used in the best alignment
    if (param->ShowUsedProbs) groot->showUsedProbs(this,troot);
    
    // show trees
    groot->showBest(this,false,troot,0);
    cout << "\n";
    groot->showBest(this,true,troot,0); // showReordered
#if NEW_BEST_FMT
    cout << "\n";
#endif
  }
}

void
Graph::ShowSents()
{
  // print E/F sent
  cout << "E:";
  for (int i=0; i<esent.size(); i++) cout << " " << esent[i];
  cout << "\nF:";
  for (int i=0; i<jsent.size(); i++) cout << " " << jsent[i];
  cout << "\n";
}

void
GraphNode::showBest(Graph *graph, bool showReord, TNode *tnode, int indent)
{
  for (int i=0; i<indent; i++) cout << "  ";
  tnode->showNodeLabel();
  if (!tnode->isLeaf()) showReorderIdx(tnode);
  if (true/*Params::showProb*/) showRNProb(tnode);
  cout << "\n";
  
#if DEBUG
  cout << "[nid=" << tnode->id << ",beta=" << beta << "]";
#endif

  // handle phrase here
  if (tnode->numc()>1) showBestPhrase(graph,tnode,indent);

  for (int xr=0; xr<tnode->numr; xr++)
    for (int xi=0; xi<3; xi++) {
      GraphArc *ap = arcs[xr*3+xi];
      if (ap && ap->rBeta>=0) {	// rBeta is used for best path marker
	// print Left inserted word
	if (xi==2) {		// insert left
	  for (int i=0; i<indent; i++) cout << "  ";
	  graph->showTransIns(tnode,xi,ap->inspos); // NULL -> F
	}
	// print Leaf or Children
	if (tnode->isLeaf()) {
	  for (int i=0; i<indent; i++) cout << "  ";
	  graph->showTrans(tnode->epos, tnode->elen, ap->npos, ap->nlen);
	} else {
	  // best partition is ap->rBeta
	  Partition *pt = ap->pttn[ap->rBeta];
	  static const int maxc = Params::MaxChildren;
	  TNode *cn[maxc];
	  // set reordered children
	  tnode->reorder(xr,cn);
	  for (int i=0; i<tnode->numc(); i++) {
	    if (showReord)
	      pt->child[i]->showBest(graph,showReord,cn[i],indent+1);
	    else {
	      // find idx
	      int idx=0;
	      while(cn[idx]!=tnode->child[i]) idx++;
	      pt->child[idx]->showBest(graph,showReord,cn[idx],indent+1);
	    }
	  }
	}
	// print Right inserted word
	if (xi==1) {
	  for (int i=0; i<indent; i++) cout << "  ";
	  graph->showTransIns(tnode,xi,ap->inspos); // NULL -> F
	}
      }
    }
}

void
GraphNode::showReorderIdx(TNode *tnode)
{
  bool found=false;
  for (int xr=0; xr<tnode->numr; xr++)
    for (int xi=0; xi<3; xi++) {
      GraphArc *ap = arcs[xr*3+xi];
      if (ap && ap->rBeta >= 0) {
	if (found) error_exit("duplicated best path mark");
	found=true;
	cout << " (R=" << xr << ", ";
	tnode->showReorderIdx(xr);
	cout << ")";
      }
    }
}

void
TNode::showReorderIdx(int xr)
{
  // similar to Train::showReorder() in dep-em2.cc
  static const char *mtx1[1] = {"0"};
  static const char *mtx2[2] = {"01","10"};
#if 0
  static const char *mtx3[6] = {"012","021","102","120","201","210"};
  static const char *mtx4[24] = {
    "0123","0132","0213","0231","0312","0321",
    "1023","1032","1203","1230","1302","1320",
    "2013","2031","2103","2130","2301","2310",
    "3012","3021","3102","3120","3201","3210"};
#else
  static char mtx3[6][4],mtx4[24][5],mtx5[120][6]; // need null-terminator
  static bool need_init = true;

  if (need_init) {
    need_init = false;
    // set mtx3
    for (int j=0; j<6; j++) mtx3[j][3]=0; // null terminator
    for (int i=0; i<3; i++) mtx3[0][i] = '0' + i;
    for (int j=1; j<6; j++) {
      for (int i=0; i<3; i++) mtx3[j][i]=mtx3[j-1][i];
      char *pt = &(mtx3[j][0]);
      next_permutation(pt,pt+3);
    }
    // set mtx4
    for (int j=0; j<24; j++) mtx4[j][4]=0; // null terminator
    for (int i=0; i<4; i++) mtx4[0][i] = '0' + i;
    for (int j=1; j<24; j++) {
      for (int i=0; i<4; i++) mtx4[j][i]=mtx4[j-1][i];
      char *pt = &(mtx4[j][0]);
      next_permutation(pt,pt+4);
    }
    // set mtx5
    for (int j=0; j<120; j++) mtx5[j][5]=0; // null terminator
    for (int i=0; i<5; i++) mtx5[0][i] = '0' + i;
    for (int j=1; j<120; j++) {
      for (int i=0; i<5; i++) mtx5[j][i]=mtx5[j-1][i];
      char *pt = &(mtx5[j][0]);
      next_permutation(pt,pt+5);
    }
  }
#endif
  switch (numc()) {
  case 1: cout << "0->0"; break;
  case 2: cout << "01->" << mtx2[xr]; break;
  case 3: cout << "012->" << mtx3[xr]; break;
  case 4: cout << "0123->" << mtx4[xr]; break;
#if 1
  case 5: cout << "01234->" << mtx5[xr]; break;
#endif
  default: error_exit("unexpected showReorder");
  }
}

void
GraphNode::showRNProb(TNode *tnode)
{
  for (int xr=0; xr<tnode->numr; xr++)
    for (int xi=0; xi<3; xi++) {
      GraphArc *ap = arcs[xr*3+xi];
      if (ap && ap->rBeta>=0) {	// rBeta is used for best path marker
	cout << " \t["; 
#if NEW_BEST_FMT
	// new: print beta-prob here
	cout << beta << "] (";
#endif
	if (tnode->numr>1) cout << "r" << xr << "=" << tnode->probR[xr] << ", ";
	string nstr = (xi==0)?"none":((xi==1)?"right":"left");
	cout << nstr << "=" << tnode->getInsProb(xi,ap->inspos);
#if NEW_BEST_FMT
	cout << ")";
#else
	cout << "]";
#endif
	return;
      }
    }      
}

void
Graph::showTransIns(TNode *tnode, int insIdx, int inspos)
{
  // print "NULL => jword [prob]"

  string& jword = jsent[inspos];
  double prob = tnode->getInsProb(insIdx,inspos);
  cout << "  NULL => " << jword << " \t[" << prob << "]\n";
}

void
Graph::showTrans(int epos, int elen, int fpos, int flen)
{
  // print "eword => jword [prob]"  (jword may be null)

  if (elen==0) error_exit("showTrans() can't handle elen==0");

  // make eword
  string eword = esent[epos];
  for (int i=1; i<elen; i++) eword += (" " + esent[epos+i]);
  if (elen>1) eword = "<" + eword + ">";

  // find TNode to access probT
  vector<TNode*> tnodes;
  FindTNodes(epos,elen,tnodes);

  string jword; double prob; 
  if (flen==0) {
    jword = "NULL";
    prob = tnodes[0]->probT[0][0]; // E -> NULL
  } else {
    // make jword
    jword = jsent[fpos];
    for (int i=1; i<flen; i++) jword += (" " + jsent[fpos+i]);
    if (flen>1) jword = "<" + jword + ">";

    prob = tnodes[0]->probT[fpos][flen];
  }

  // print it
  cout << "  " << eword << " => " << jword << " \t[" << prob << "]";

  // print lambda1
  if (elen>1 && param->LambdaPhraseVary) cout << " L=" << tnodes[0]->lambda1prob;

  cout << "\n";
}

#if 0
//
// Show Egypt style alignment
//

void
Graph::ShowAlignment()
{
  this->setBestBeta(troot);
  groot->showAlignment(this,troot);
}


void
GraphNode::showAlignment(Graph *graph, TNode *tnode)
{
  // similar to GraphNode::showBest()

  for (int xr=0; xr<tnode->numr; xr++)
    for (int xi=0; xi<3; xi++) {
      GraphArc *ap = arcs[xr*3+xi];
      if (ap && ap->rBeta>=0) {	// rBeta is used for best path marker
	// print Left inserted word
	if (xi==2) {
	  cout << "* ({ " << ap->inspos + 1 << " }) ";
	}
	// print Leaf or Children
	if (tnode->isLeaf()) {
	  cout << graph->esent[tnode->epos] << " ({ ";
	  if (ap->nlen) cout << ap->npos + 1 << " ";
	  cout << "}) ";
	} else {
	  // best partition is ap->rBeta
	  Partition *pt = ap->pttn[ap->rBeta];
	  static const int maxc = Params::MaxChildren;
	  TNode *cn[maxc];
	  // set reordered children
	  tnode->reorder(xr,cn);
	  // find idx
	  for (int i=0; i<tnode->numc(); i++) {
	    int idx=0;
	    while(cn[idx]!=tnode->child[i]) idx++;
	    pt->child[idx]->showAlignment(graph,cn[idx]);
	  }
	}
	// print Right inserted word
	if (xi==1) {
	  cout << "* ({ " << ap->inspos + 1 << " }) ";
	}
      }
    }
}
#endif

//
// Set Best Beta (for Show Best alignment)
//

void
Graph::setBestBeta(TNode *tnode)
{
  // set best beta probs from bottom-up.
  // (similar to Graph::setBeta())
  // use GraphNode::beta for this best beta, and
  // use GraphArc::rbeta as best arc marker.

  // children must be set first
  for (int i=0; i<tnode->numc(); i++) setBestBeta(tnode->child[i]);

  // set this beta
  int id = tnode->id;
  int max = jsent.size();

  for (int pos=0; pos<=max; pos++) // max inclusive
    for (int len=0; len<=max; len++) {
      GraphNode *gnode = gn[id][pos][len];
      if (gnode) gnode->setBestBeta(tnode);
    }
}


inline void
GraphNode::setBestBeta(TNode *tnode)
{
  // use alpha as best node marker

  // Note: bestBeta must be set, including phrase arcs, to obtain a real beta
  // value used by upper nodes. However, it sets bestArc mark only for non-phrase
  // arcs. Checking phrase arcs will done at the end of this method.

  int numr = tnode->numr*3;	// exclude extra phrase arcs
  if (tnode->isLeaf()) {
    double max=0.0; int bestarc=-1;
    for (int i=0; i<numr; i++) {
      GraphArc *ap = arcs[i];
      if (ap) {
	ap->rBeta = -1;		// clear best arc mark
	if (ap->prob > max) {
	  max = ap->prob; bestarc = i;
	}
      }
    }
    beta = max;
    if (bestarc>=0) arcs[bestarc]->rBeta = 0;

  } else {			// non-leaf
    double max=0.0; int bestarc=-1,bestpttn=-1;
    for (int i=0; i<numr; i++) {
      GraphArc *ap = arcs[i];
      if (ap) {
	ap->rBeta = -1;		// clear best arc mark
	for (int j=0; j<ap->pttn.size(); j++) {
	  Partition *pt = ap->pttn[j];
	  double prod=1.0;
	  for (int k=0; k<pt->numc; k++) prod *= pt->child[k]->beta;
	  double pr = ap->prob * prod;
	  if (pr > max) {
	    max = pr;
	    bestarc = i; bestpttn = j;
	  }
	}
      }
    }
    beta = max;
    if (bestarc>=0 && bestpttn>=0) arcs[bestarc]->rBeta = bestpttn;
  }

  // check phrase arcs (overrides only beta value, not for best markers)
  int offset;
  if (hasPhraseArcs(tnode,offset)) {
    for (int ix=0; ix<3; ix++) {
      GraphArc *ap = arcs[offset+ix];
      if (ap && ap->prob > beta) beta = ap->prob;
    }
  }
}

#if 0
//
// Show Alignment in List format (to be used for list2tree|xtree16)
//

void
Graph::ShowAlignmentList()
{
  // similar to Graph::ShowBest()

  this->setBestBeta(troot);
  groot->showAlignmentList(this,troot,true); cout << "\n";
  groot->showAlignmentList(this,troot,false); cout << "\n";
}

void
GraphNode::showAlignmentList(Graph *graph, TNode *tnode, bool showSrc)
{
  // similar to GraphNode::showBest()

  cout << "(" ;
  tnode->showNodeLabel();
  cout << " ";

  for (int xr=0; xr<tnode->numr; xr++)
    for (int xi=0; xi<3; xi++) {
      GraphArc *ap = arcs[xr*3+xi];
      if (ap && ap->rBeta>=0) {	// rBeta is used for best path marker
	// print Left inserted word
	if (xi==2) {		// insert left
	  cout << " " << (showSrc ? "*" : graph->jsent[ap->inspos]);
	}
	// print Leaf of Children
	if (tnode->isLeaf()) {
	  cout << " " << (showSrc ? graph->esent[tnode->epos] :
			  (ap->nlen ? graph->jsent[ap->npos] : "*"));
	} else {
	  // best partition is ap->rBeta
	  Partition *pt = ap->pttn[ap->rBeta];
	  static const int maxc = Params::MaxChildren;
	  TNode *cn[maxc];
	  // set reorderd children
	  tnode->reorder(xr,cn);
	  for (int i=0; i<tnode->numc(); i++) {
	    if (!showSrc)
	      pt->child[i]->showAlignmentList(graph,cn[i],showSrc);
	    else {
	      // find idx
	      int idx=0;
	      while(cn[idx]!=tnode->child[i]) idx++;
	      pt->child[idx]->showAlignmentList(graph,cn[idx],showSrc);
	    }
	  }
	}
	// print Right inserted word
	if (xi==1) {
	  cout << " " << (showSrc ? "*" : graph->jsent[ap->inspos]);
	}
      }
    }
  cout << ")";
}
#endif

//
// Print
//

void
Graph::Print()
{
  // print all nodes
  int max=jsent.size();
  for (int id=0; id<numtnodes; id++)
    for (int pos=0; pos<=max; pos++)
      for (int len=0; len<=max; len++) {
	GraphNode *gnode = gn[id][pos][len];
	if (gnode) gnode->Print();
      }
}

void
GraphNode::Print()
{
  cout << "Node[" << nid << "][" << pos << "][" << len << "]: ";
  cout << "alpha=" << alpha << ", beta=" << beta;
  cout << ", a*b=" << alpha*beta << "\n";
  for (int i=0; i<arcs.size(); i++)
    if (arcs[i]) arcs[i]->Print();
  cout << "(in Partitions: ";
  for (int i=0; i<prevs.size(); i++)
    prevs[i]->Print();
  cout << ")\n";
}

void
GraphArc::Print()
{
  cout << "  Arc[" << idxNull << "][" << idxReorder << "]:";
  cout << " prob=" << prob << ", beta=" << rBeta << "\n";
  cout << "    npos=" << npos << ", nlen=" << nlen
       << ",inspos=" << inspos << "\n";
  cout << "    Partition= ";
  for (int i=0; i<pttn.size(); i++)
    pttn[i]->Print();
  cout << "\n";
}

void
Partition::Print()
{
  cout << "(";
  for (int i=0; i<numc; i++) {
    GraphNode *gn = child[i];
    cout << "[" << gn->nid << ":" << gn->pos << ":" << gn->len << "]";
  }
  cout << ")";
}

//
// Deconstructors
//

GraphNode::~GraphNode()
{
  for (int i=0; i<arcs.size(); i++) delete arcs[i];
  // PartitionVec prevs are deleted through ~PartitionVec()
}

GraphArc::~GraphArc()
{
  for (int i=0; i<pttn.size(); i++) delete pttn[i];
}

//
// new methods
//

Graph::~Graph()
{
  delete troot;
}

inline double 
TNode::getInsProb(int insIdx, int inspos)
{
  switch(insIdx) {
  case 0: return probInsNone;
  case 1: return probInsRight[inspos];
  case 2: return probInsLeft[inspos];
  }
}

inline void 
TNode::addInsCount(int insIdx, int inspos, double val)
{
  switch(insIdx) {
  case 0: countInsNone += val; break;
  case 1: countInsRight[inspos] += val; break;
  case 2: countInsLeft[inspos] += val; break;
  }
}

inline double
TNode::getInsNoneCount(double prob=1.0)
{
  return ((prob>0) ? (countInsNone/prob) : 0.0);
}  

inline double
TNode::getInsRightCount(int inspos, double prob=1.0)
{
  return ((prob>0) ? (countInsRight[inspos]/prob) : 0.0);
}  

inline double
TNode::getInsLeftCount(int inspos, double prob=1.0)
{
  return ((prob>0) ? (countInsLeft[inspos]/prob) : 0.0);
}  

void
Graph::SetFloorProb(bool testMode)
{
  double floor = testMode ? param->TestFloorProb : param->TrainFloorProb;

  // probNT
  for (int i=0; i<MaxFLen; i++)
    probNT[i] += floor;

  // probR,probN,probT
  troot->setFloorProb(floor);
}

void
TNode::setFloorProb(double floor)
{
  // add floor prob for obtaining test perplexity.
  // This has to be called just before Graph::SetArcProb()

  // probT
  for (int i=0; i<MaxFLen; i++)
    for (int j=0; j<MaxFLen; j++)
      probT[i][j] += floor;

  // probR
  for (int i=0; i<Params::MaxReorder; i++)
    probR[i] += floor;
  
  // probN
  probInsNone += floor;
  for (int i=0; i<MaxFLen; i++) {
    probInsRight[i] += floor;
    probInsLeft[i] += floor;
  }

  // recursively set for children
  for (int i=0; i<numc(); i++)
    child[i]->setFloorProb(floor);
}

void
Graph::FindTNodes(int posE, int lenE, vector<TNode*>& list)
{
  // note: there may be more than one node, because of unary rule

  for (int i=0; i<tnodeList.size(); i++) {
    TNode *pt = tnodeList[i];
    if (pt->epos==posE && pt->elen==lenE)
      list.push_back(pt);
  }
  if (list.size()==0)  error_exit("TNode not found!");
}

void
Graph::SetStringInfo(vector<string>& esent, vector<string>& jsent, vector<string>& labels)
{
  // copy string sentences
  this->esent = esent;
  this->jsent = jsent;

  if (tnodeList.size() != labels.size()) error_exit("inconsistent label size");

  for (int i=0; i<labels.size(); i++)
    tnodeList[i]->label = labels[i];
}

void
Graph::CollectPhraseE(vector<int>& poslenArr, TNode *node=NULL)
{
  // collect all epos/elen pairs
  // return array of epos/elen (of those TNodes)

  if (node==NULL) node=troot;

  // collect epos/elen, unless this is an unary node.
  // (to avoid duplicates by unary nodes)
  if (node->child.size() != 1) {
    poslenArr.push_back(node->epos);
    poslenArr.push_back(node->elen);
  }

  // recursive call itself
  for (int i=0; i<node->numc(); i++)
    CollectPhraseE(poslenArr,node->child[i]);
}

void
TNode::CollectTNodes(vector<TNode*>& list)
{
  list.push_back(this);
  for (int i=0; i<child.size(); i++)
    child[i]->CollectTNodes(list);
}

void
Graph::CacheProbs(vector<int>& poslenEFArr, vector<double>& probArr)
{
  // see Socket::PackSendProb() for packet format

#if 0
  if (param->Verbose)
    cout << "Cache: poslenEFArr.size()=" << poslenEFArr.size() << "\n";
#endif
  int nprob=probArr.size();
  //cout << "CacheProbs: nprob=" << nprob << "\n";
  int idx4=0;
  for (int idx=0; idx<nprob; idx++) {
    int epos = poslenEFArr[idx4++];
    int elen = poslenEFArr[idx4++];
    int fpos = poslenEFArr[idx4++];
    int flen = poslenEFArr[idx4++];
    double prob = probArr[idx];
    vector<TNode*> tnodes;
    FindTNodes(epos,elen,tnodes);
    // there may be more than one node, due to unary rule
    for (int i=0; i<tnodes.size(); i++) {
      TNode *tnode = tnodes[i];
      //cout << "epos=" << epos << ",elen=" << elen << ",fpos=" << fpos << ",flen=" << flen;
      //cout << ",nodeid=" << tnode->id << ",prob=" << prob << "\n";

      if (fpos==FPOS_LAMBDA1) {
	if (flen==FLEN_L1) tnode->lambda1prob = prob;
      } else {
	tnode->probT[fpos][flen] = prob;
	tnode->countT[fpos][flen] = 0.0;
      }
    }
  }
}

void
Graph::CacheProbsRN(vector<int>& numcArr, vector<double>& probArr)
{
  // see Socket::PackSendProbRN for packet format

#if 0
  cout << "numc[]=";
  for (int i=0; i<numcArr.size(); i++)
    cout << " " << numcArr[i];
  cout << "\n";
  for (int i=0; i<probArr.size(); i++)
    cout << "CacheRN:probArr[" << i << "]=" << probArr[i] << "\n";
#endif

  int numNode = numcArr.size();
  if (numNode != tnodeList.size()) error_exit("numNode mismatch ",numNode);
  
  // for each node
  int idxProb = 0;
  for (int i=0; i<numNode; i++) {
    int numc = numcArr[i];
    if (numc != tnodeList[i]->numc()) error_exit("numc mismatch ", i);
    int numr = factorial(numc);

    // set TableR
    TNode *tn = tnodeList[i];
    for (int j=0; j<numr; j++) {
      double prob = probArr[idxProb++];
      tn->probR[j] = prob;
      tn->countR[j] = 0.0;
    }
  }    

  // set insProbs
  int flen = jsent.size();

  // for topnode
  TNode *tn = tnodeList[0];	// or troot
  tn->probInsNone = probArr[idxProb++];
  for (int posF=0; posF<flen; posF++) {
    tn->probInsRight[posF] = probArr[idxProb++];
    tn->probInsLeft[posF] = probArr[idxProb++];
  }

  // for each node
  for (int i=0; i<tnodeList.size(); i++) {
    TNode *tn = tnodeList[i];
    for (int j=0; j<tn->child.size(); j++) {
      tn->child[j]->probInsNone = probArr[idxProb++];
      for (int posF=0; posF<flen; posF++) {
	tn->child[j]->probInsRight[posF] = probArr[idxProb++];
	tn->child[j]->probInsLeft[posF] = probArr[idxProb++];
      }
    }
  }

  if (idxProb != probArr.size()) error_exit("inconsistent probArr in SetTablesRN");
}

void
Graph::FlushCounts(double prob, vector<int>& poslenEFArr, vector<double>& cntArr)
{
  // reverse of CacheProbs()
  // poslenEFArr must be given (saved in context)

  // note: it's possible to modify poslenEFArr to eliminate entries if cnt=0,
  //       but not yet implemented.

#if 0
  cout << "Flush: poslenEFArr.size()=" << poslenEFArr.size() << "\n";
#endif
  int nprob = poslenEFArr.size()/4;
  int idx4=0;
  for (int idx=0; idx<nprob; idx++) {
    int epos = poslenEFArr[idx4++];
    int elen = poslenEFArr[idx4++];
    int fpos = poslenEFArr[idx4++];
    int flen = poslenEFArr[idx4++];
    double count = 0.0;
    vector<TNode*> tnodes;
    FindTNodes(epos,elen,tnodes);
    // there may be more than one node, due to unary rule
    for (int i=0; i<tnodes.size(); i++) {
      TNode *tnode = tnodes[i];

      if (fpos==FPOS_LAMBDA1) {
	if (flen==FLEN_L1) count += tnode->lambda1cnt;
	if (flen==FLEN_L1N) count += tnode->lambda1ncnt;
      } else {
	count += tnode->countT[fpos][flen];
      }
    }
    count = (prob>0) ? count/prob : 0.0; // normalize
    cntArr.push_back(count);
  }
}

void
Graph::FlushCountsRN(double prob,
		     vector<int>& numcArr, vector<double>& cntArr, int& flen)
{
  // reverse of CacheProbsRN()
  // numcArr must be cleared.

  // set flen
  flen = jsent.size();

  // for each node
  int numNode = tnodeList.size();
  for (int i=0; i<numNode; i++) {
    int numc = tnodeList[i]->numc();
    numcArr.push_back(numc);
    int numr = factorial(numc);

    // set CountR
    TNode *tn = tnodeList[i];
    for (int j=0; j<numr; j++) {
      double count = tn->countR[j];
      count = (prob>0) ? count/prob : 0.0;
      cntArr.push_back(count);
    }
  }
  
  // set insProbs

  // for topnode
  TNode *tn = tnodeList[0];	// or troot
  cntArr.push_back(tn->getInsNoneCount(prob)); // returns normalized count
  for (int posF=0; posF<flen; posF++) {
    cntArr.push_back(tn->getInsRightCount(posF,prob));
    cntArr.push_back(tn->getInsLeftCount(posF,prob));
  }
		     
  // for each node
  for (int i=0; i<tnodeList.size(); i++) {
    TNode *tn = tnodeList[i];
    for (int j=0; j<tn->child.size(); j++) {
      cntArr.push_back(tn->child[j]->getInsNoneCount(prob));
      for (int posF=0; posF<flen; posF++) {
	cntArr.push_back(tn->child[j]->getInsRightCount(posF,prob));
	cntArr.push_back(tn->child[j]->getInsLeftCount(posF,prob));
      }
    }
  }	
}

// handle extra phrase arcs

// setBeta() must be modified too.

#define PH_NOINS 1

inline void
GraphNode::setArcProbPhrase(Graph *graph, TNode *tnode)
{
  const double lambda1 = tnode->lambda1prob;
  int offset = tnode->numr*3;

  if (arcs.size()>=offset+3) {
    // extra phrase arcs exist
    for (int ix=0; ix<3; ix++) {
      GraphArc *ap = arcs[offset+ix];
      if (ap) {
#if PH_NOINS
	// use phrase only no-insert (ix==0)
	double phraseP = 0.0;
	if (ap->nlen>0 && ix==0) phraseP = tnode->probT[ap->npos][ap->nlen];
	ap->prob = phraseP;
#else
	double insP = tnode->getInsProb(ix,ap->inspos);
	double phraseP = 0.0;
	if (ap->nlen>0) phraseP = tnode->probT[ap->npos][ap->nlen];
	ap->prob = insP * phraseP;
#endif
	ap->prob *= lambda1;
      }
    }
  }
}

inline void
GraphNode::getArcCountPhrase(Graph *graph, TNode *tnode)
{
  int offset;
  if (hasPhraseArcs(tnode,offset)) {
    // extra phrase arcs exist
    for (int ix=0; ix<3; ix++) {
      GraphArc *ap = arcs[offset+ix];
      if (ap) {
	double cnt = alpha * ap->prob * ap->rBeta;
	if (cnt>0) {
#if PH_NOINS
	  // use phrase only no-insert (ix==0)
	  if (ap->nlen>0 && ix==0) tnode->countT[ap->npos][ap->nlen] += cnt;
#else
	  tnode->addInsCount(ix,ap->inspos,cnt);
	  if (ap->nlen>0) tnode->countT[ap->npos][ap->nlen] += cnt;
#endif
	  tnode->lambda1cnt += cnt;
	}
      }
    }
  }
}

inline void
GraphNode::showBestIndent(int indent)
{
  for (int i=0; i<indent; i++) cout << "  ";
}

bool
GraphNode::hasPhraseArcs(TNode *tnode, int& offset)
{
  // check if there are extra phrase arcs, which are placed after
  // the regular arcs.
  // also returns the indexes for those arcs (=> offset)

  offset = tnode->numr*3;
  return (arcs.size() >= offset+3);
}  

void
GraphNode::showBestPhrase(Graph *graph, TNode *tnode, int indent)
{
  int offset;
  if (hasPhraseArcs(tnode,offset)) {
    // find best phrase

    double maxP; int ixBest;
    FindBestPhraseArc(offset,ixBest,maxP);

    // obtain max in best regular arcs.
    double maxR = 0.0;

    // find best arc (as marked ap->rBeta >= 0, which is best partition)
    int rIdx;
    FindBestRegularArc(offset,tnode,maxR);

    // print if best phrase is better than best regular arcs
    if (maxP > maxR) {
      GraphArc *ap = arcs[offset+ixBest];
      if (ixBest==2) {		// insert left
	showBestIndent(indent);
	cout << "  <>";
	graph->showTransIns(tnode,ixBest,ap->inspos); // NULL -> F
      }
      showBestIndent(indent);
      graph->showTrans(tnode->epos, tnode->elen, ap->npos, ap->nlen);
      if (ixBest==1) {		// insert right
	showBestIndent(indent);
	cout << "  <>";
	graph->showTransIns(tnode,ixBest,ap->inspos); // NULL -> F
      }
    }
  }
}

GraphArc *
GraphNode::FindBestPhraseArc(int offset, int& ixBest, double& maxP)
{
  // find best arc within phrase arcs.
  // offset must be obtained by hasPhraseArcs() beforehand.
  // returns ixBest and maxP.

  GraphArc *best_ap = 0;
  maxP=0.0; ixBest=-1;
  for (int ix=0; ix<3; ix++) {
    GraphArc *ap = arcs[offset+ix];
    if (ap) {
      if (ap->prob > maxP) {
	maxP = ap->prob; ixBest = ix;
	best_ap = ap;
      }
    }
  }
  return best_ap;
}

GraphArc *
GraphNode::FindBestRegularArc(int offset, TNode *tnode, double& maxR)
{
  // find best arc (as marked ap->rBeta >= 0, which is best partition).
  // returns best prob as maxR
  // note: use only non-leaf node

  GraphArc *ap, *best_ap=0;
  for (int i=0; i<offset; i++) {
    ap = arcs[i];
    if (ap && ap->rBeta>=0) {
      best_ap = ap; break;
    }
  }

  if (best_ap==0) {
    // all regular arcs have prob=0
    maxR = 0.0;
  } else {
    // best partition is ap->rBeta
    Partition *pt = ap->pttn[ap->rBeta];
    double prod=1.0;
    for (int k=0; k<pt->numc; k++) prod *= pt->child[k]->beta;
    maxR = ap->prob * prod;
  }
  return best_ap;
}

GraphArc *
GraphNode::FindBestRegularArc(TNode *tnode, int& rIdx, int& insIdx)
{
  for (int xr=0; xr<tnode->numr; xr++)
    for (int xi=0; xi<3; xi++) {
      GraphArc *ap = arcs[xr*3+xi];
      if (ap && ap->rBeta>=0) {
	rIdx = xr; insIdx = xi;
	return ap;
      }
    }
  return 0;
}

void
Graph::showBestAlignment()
{
  cout << "A: ";
  groot->showBestAlignment(this,troot);
  cout << "\n";
}  

void
GraphNode::showBestAlignment(Graph *graph, TNode *tnode)
{
  int xr,xi;
  GraphArc *ap_R = FindBestRegularArc(tnode,xr,xi);

  // If no best regular arc is found, return.
  // This may happen if total prob is zero.
  if (ap_R==0) return;

  if (tnode->isLeaf()) {
    // leaf node
    GraphArc *ap = ap_R;
    cout << tnode->epos << " " << tnode->elen << " "
	 << ap->npos << " " << ap->nlen << ":";

  } else {
    // non-leaf node

    // check phrase translation
    bool found_phrase=false; int offset; double maxP=0, maxR=0;
    if (tnode->numc()>1 && hasPhraseArcs(tnode,offset)) {
      int ixBest;
      GraphArc *ap_P = FindBestPhraseArc(offset,ixBest,maxP);
      FindBestRegularArc(offset,tnode,maxR); // to obtain maxR
      if (maxP > maxR) {
	found_phrase=true;
	GraphArc *ap = ap_P;
	cout << tnode->epos << " " << tnode->elen << " "
	     << ap->npos << " " << ap->nlen << ":";
      }
    }

    // if no phrase translation applied here, call it recursively
    if (!found_phrase) {
      // similar to showBest.
      // best partition is ap->rBeta
      GraphArc *ap = ap_R;
      Partition *pt = ap->pttn[ap->rBeta];
      static const int maxc = Params::MaxChildren;
      TNode *cn[maxc];
      tnode->reorder(xr,cn);	// set reordered children
      for (int i=0; i<tnode->numc(); i++)
	pt->child[i]->showBestAlignment(graph,cn[i]);
    }
  }
}

void
GraphNode::showUsedProbs(Graph *graph, TNode *tnode)
{
  // prints probs used in the best alignment, thus setBestBeta() must be
  // called beforehand.
  // format: ProbR: numc rIdx= xx prob= prob rule= X -> A B C
  //         ProbN: numc iIdx/pos/prob= idx1 jpos1 prob1 : idx2 jpos2 prob2... : rule= X -> A B C
  //                   (jposx is a index to jwords[] for inserted wrd. -1 if none)
  //         ProbT: elen jlen prob ewords... -> jwords...
  // note: external converter may be needed when -oldmode, which separately
  // keeps P(NULL->jwords) (using dmp2decode3.pl) to obtain real ProbN.

  if (tnode->isLeaf()) showUsedProbT(graph,tnode); // print ProbT
  else {
    // check if phrase translation is available, see showBestPhrase(),showBestAlignment()
    bool foundPhrase=false; int offset; GraphArc *bestArcP,*bestArcR;
    if (hasPhraseArcs(tnode,offset)) {
      double maxP=0,maxR=0; int ixBest;
      bestArcP = FindBestPhraseArc(offset,ixBest,maxP);
      bestArcR = FindBestRegularArc(offset,tnode,maxR);
      if (maxP > maxR) foundPhrase = true;
    }

    if (foundPhrase) showUsedProbPhraseT(graph,tnode,bestArcP); // print ProbT for phrase

    int xr,xi;
    GraphArc *ap = FindBestRegularArc(tnode,xr,xi); // to obtain xr

    if (!foundPhrase || (foundPhrase && graph->param->ShowUsedProbsInPhrase && ap)) {
      // print ProbR,ProbN here...

      // TopIns
      if (tnode==graph->troot) {
	string nstr = (xi==0)?"none":((xi==1)?"right":"left");
	cout << "ProbN: 1 iIdx/pos/prob= " << nstr
	     << " " << ((xi==0) ? -1 : ap->inspos)
	     << " " << tnode->getInsProb(xi,ap->inspos)
	     << " : rule= TOP -> " << tnode->label << "\n";
      }

      // ProbR:
      int numc = tnode->numc();
      cout << "ProbR: " << numc << " rIdx= " << xr
	   << " prob= " << tnode->probR[xr];
      // print cfg
      cout << " rule= " << tnode->label << " ->";
      for (int i=0; i<numc; i++) cout << " " << tnode->child[i]->label;
      cout << "\n";

      // ProbN:
      cout << "ProbN: " << numc << " iIdx/pos/prob=";

      // insPos is actually stored in children.
      // find best partition (ap->rBeta)

      Partition *pt = ap->pttn[ap->rBeta];
      TNode *cn[Params::MaxChildren];
      tnode->reorder(xr,cn);
      for (int i=0; i<numc; i++) {
	// find idx
	int idx=0;
	while(cn[idx]!=tnode->child[i]) idx++;

	// find best arc for each children (a duplicated effort...)
	GraphNode *gn2 = pt->child[idx];
	TNode *tnode2 = cn[idx];
	bool foundP2=false; int off2,ixBest; GraphArc *apP2,*apR2,*ap2;
	if (gn2->hasPhraseArcs(tnode2,off2)) {
	  double maxP=0,maxR=0;
	  apP2 = gn2->FindBestPhraseArc(off2,ixBest,maxP);
	  apR2 = gn2->FindBestRegularArc(off2,tnode2,maxR);
	  if (maxP > maxR) foundP2=true;
	}
	int xi2,xr2;
	if (foundP2) {
	  xi2 = ixBest; ap2 = apP2;
	} else {
	  ap2 = gn2->FindBestRegularArc(tnode2,xr2,xi2); // to obtain xi2
	  //ap2 = apR2;
	}
	string nstr = (xi2==0)?"none":((xi2==1)?"right":"left");
	cout << " " << nstr << " " << ((xi2==0) ? -1 : ap2->inspos)
	     << " " << tnode2->getInsProb(xi2,ap2->inspos) << " :";
      }

      // print cfg
      cout << " rule= " << tnode->label << " ->";
      for (int i=0; i<numc; i++) cout << " " << tnode->child[i]->label;
      cout << "\n";

      // call it recursively for children
      // find best partition (ap->rBeta)
      {
	Partition *pt = ap->pttn[ap->rBeta];
	TNode *cn[Params::MaxChildren];
	tnode->reorder(xr,cn);
	for (int i=0; i<numc; i++) {
	  // find idx
	  int idx=0;
	  while(cn[idx]!=tnode->child[i]) idx++;
	  pt->child[idx]->showUsedProbs(graph,cn[idx]);
	}
      }
    }
  }
  if (tnode==graph->troot) cout << "\n";
}

void
GraphNode::showUsedProbT(Graph *graph, TNode *tnode)
{
  int xr,xi;
  GraphArc *ap = FindBestRegularArc(tnode,xr,xi);
  showUsedProbT(graph,tnode,ap);
}

void
GraphNode::showUsedProbT(Graph *graph, TNode *tnode, GraphArc *ap)
{
  int epos=tnode->epos, elen=tnode->elen, jpos=ap->npos, jlen=ap->nlen;
  cout << "ProbT: " << elen << " " << jlen;

  double prob = (jlen==0) ? tnode->probT[0][0] : tnode->probT[jpos][jlen];
  cout << " " << prob;

  for (int i=0; i<elen; i++) cout << " " << graph->esent[epos+i];
  cout << " ->";
  if (jlen==0) cout << " NULL";
  else for (int i=0; i<jlen; i++) cout << " " << graph->jsent[jpos+i];
  cout << "\n";
}

void
GraphNode::showUsedProbPhraseT(Graph *graph, TNode *tnode, GraphArc *ap)
{
  showUsedProbT(graph,tnode,ap);
}

// 
// Show Phrase Probs
//

// similar to -uprob with Graph::showBestAlignment()

void
Graph::ShowPhraseProbs()
{
  // first, set best prob (taken from Graph::ShowBest())

  double totProb = groot->beta;	// save before used in setBestBeta()
  this->setBestBeta(troot);
  double bestProb = groot->beta;

#if 1
  // print sents, alignment, and prob
  ShowSents();
  showBestAlignment();
  cout << "(Prob=" << bestProb << " [" << bestProb/totProb << "])\n\n";
#endif

  // next, process each nodes recursively
  groot->showPhraseProbs(this,troot);
}

void
GraphNode::showPhraseProbs(Graph *graph, TNode *tnode)
{
  // similar to GraphNode::showUsedProbs()

  // prints phrase probs (non 1-to-1 t-table) used in the best alignment.
  // assumes setBestBeta() was called beforehand.
  // output format: ProbPH: elen jlen prob label ewords... -> jwords...

  if (tnode->isLeaf()) showPhraseProbT(graph,tnode);
  else {
    // check if phrase translation is available
    bool foundPhrase=false; int offset; GraphArc *bestArcP,*bestArcR;
    if (hasPhraseArcs(tnode,offset)) {
      double maxP=0,maxR=0; int ixBest;
      bestArcP = FindBestPhraseArc(offset,ixBest,maxP);
      bestArcR = FindBestRegularArc(offset,tnode,maxR);
      if (maxP > maxR) foundPhrase = true;
    }
    if (foundPhrase) showPhraseProbT(graph,tnode,bestArcP);
    else {
      // call it recursively for children
      // find best partition (ap->rBeta)
      int xr,xi;
      GraphArc *ap = FindBestRegularArc(tnode,xr,xi); // to obtain xr

      // If no best regular arc is found, return.
      // This may happen if total prob is zero.
      if (ap==0) return; 

      Partition *pt = ap->pttn[ap->rBeta];
      TNode *cn[Params::MaxChildren];
      tnode->reorder(xr,cn);
      int numc = tnode->numc();
      for (int i=0; i<numc; i++) {
	// find idx
	int idx=0;
	while(cn[idx]!=tnode->child[i]) idx++;
	pt->child[idx]->showPhraseProbs(graph,cn[idx]);
      }
    }
  }
}

void
GraphNode::showPhraseProbT(Graph *graph, TNode *tnode)
{
  // print if this uses non 1-to-1 t-table

  int xr,xi;
  GraphArc *ap = FindBestRegularArc(tnode,xr,xi);
  showPhraseProbT(graph,tnode,ap);
}

void
GraphNode::showPhraseProbT(Graph *graph, TNode *tnode, GraphArc *ap)
{
  // print if this uses non 1-to-1 t-table

  int epos=tnode->epos, elen=tnode->elen, jpos=ap->npos, jlen=ap->nlen;
  if (elen==1 && jlen<=1) return;	// do nothing

  cout << "ProbPH: " << elen << " " << jlen;

  double prob = (jlen==0) ? tnode->probT[0][0] : tnode->probT[jpos][jlen];

  // lambda1prob is included if -lambda1vary
  if (elen!=1 && graph->param->LambdaPhraseVary)  prob *= tnode->lambda1prob;

  cout << " " << prob;

  cout << " ";
  tnode->showNodeLabel();

  for (int i=0; i<elen; i++) cout << " " << graph->esent[epos+i];
  cout << " ->";
  if (jlen==0) cout << " NULL";
  else for (int i=0; i<jlen; i++) cout << " " << graph->jsent[jpos+i];
  cout << "\n";
}

// End of Graph
