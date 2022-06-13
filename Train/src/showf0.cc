#include "graph.h"

void
Graph::ShowFertZero()
{
  // similar to Graph::ShowPhraseProbs()

  double totProb = groot->beta;	// save before used in setBestBeta()
  this->setBestBeta(troot);
  double bestProb = groot->beta;

  // print sent and alignment
  ShowSents();
  if (totProb>0) {
    showBestAlignment();
    cout << "(Prob=" << bestProb << " [" << bestProb/totProb << "])\n\n";
  } else {
    cout << "A: n/a\n\n";
    cout << "(Prob=" << bestProb << " [" << "n/a" << "])\n\n";
  }

  if (totProb>0) {		// prob may be zero!
    if (groot->nonUnaryAllFertZeroExists(this,troot)) {
      cout << "## nonUnaryAllFertZero found\n";
      groot->showBest(this,false,troot,0); // show tree
      cout << "\n";
    } else {
      groot->showFertZero(this,troot);
    }
  }
}

bool
GraphNode::nonUnaryAllFertZeroExists(Graph *graph, TNode *tnode)
{
  if (tnode->isLeaf()) return false;

  for (int xr=0; xr<tnode->numr; xr++)
    for (int xi=0; xi<3; xi++) {
      GraphArc *ap = arcs[xr*3+xi];
      if (ap && ap->rBeta>=0) {	// rBeta is used for best path marker
	// print Leaf or Children
	if (tnode->isLeaf()) {
	  return false;
	} else {
	  // best partition is ap->rBeta
	  Partition *pt = ap->pttn[ap->rBeta];
	  static const int maxc = Params::MaxChildren;
	  TNode *cn[maxc];
	  // set reordered children
	  tnode->reorder(xr,cn);
	  int numc = tnode->numc();

	  if (numc==1) {
	    // this node is ok, but check lower nodes
	    return pt->child[0]->nonUnaryAllFertZeroExists(graph,cn[0]);
	  } else {
	    // first check if this is ok
	    bool allzero = true;
	    for (int i=0; i<numc; i++) {
	      if (! (pt->child[i]->allFertZero(graph,cn[i]))) {
		allzero=false; break;
	      }
	    }
	    if (allzero) return true;
	    else {
	      // this is ok. check each child
	      for (int i=0; i<numc; i++) {
		if (pt->child[i]->nonUnaryAllFertZeroExists(graph,cn[i]))
		  return true;
	      }
	      return false;
	    }
	  }
	}
      }
    }
}

bool
GraphNode::allFertZero(Graph *graph, TNode *tnode)
{
  // returns true if all leaves are fertzero words

  for (int xr=0; xr<tnode->numr; xr++)
    for (int xi=0; xi<3; xi++) {
      GraphArc *ap = arcs[xr*3+xi];
      if (ap && ap->rBeta>=0) {	// rBeta is used for best path marker
	// print Leaf or Children
	if (tnode->isLeaf()) {
	  int flen = ap->nlen;
	  return (flen==0);
	} else {
	  // best partition is ap->rBeta
	  Partition *pt = ap->pttn[ap->rBeta];
	  static const int maxc = Params::MaxChildren;
	  TNode *cn[maxc];
	  // set reordered children
	  tnode->reorder(xr,cn);
	  for (int i=0; i<tnode->numc(); i++) {
	    if (! (pt->child[i]->allFertZero(graph,cn[i])))
	      return false;
	  }
	  return true;
	}
      }
    }
}

void
GraphNode::showFertZero(Graph *graph, TNode *tnode)
{
  // show fertility zero context, i.e. show f0rule and f0words

  for (int xr=0; xr<tnode->numr; xr++)
    for (int xi=0; xi<3; xi++) {
      GraphArc *ap = arcs[xr*3+xi];
      if (ap && ap->rBeta>=0) {	// rBeta is used for best path marker
	// print Leaf or Children
	if (tnode->isLeaf()) {
	  // do nothing
	} else {
	  // best partition is ap->rBeta
	  Partition *pt = ap->pttn[ap->rBeta];
	  static const int maxc = Params::MaxChildren;
	  TNode *cn[maxc];
	  // set reordered children
	  tnode->reorder(xr,cn);
	  int numc = tnode->numc();

	  if (numc==1) {
	    // this node is ok. go lower nodes
	    pt->child[0]->showFertZero(graph,cn[0]);
	  } else {
	    // first check if this node has both f0 and non-f0 child.
	    // note: it is guaranteed that not all children are f0, since
	    // nonUnaryAllFertZeroExists() detects it already.

	    vector<bool> f0; vector<GraphNode*> gcn;
	    bool f0exist=false;
	    for (int i=0; i<numc; i++) {
	      // find idx
	      int ix=0;
	      while(cn[ix]!=tnode->child[i]) ix++;
	      bool isf0 = pt->child[ix]->allFertZero(graph,cn[ix]);
	      f0.push_back(isf0); gcn.push_back(pt->child[ix]);
	      if (isf0) f0exist=true;
	    }

	    // show f0rule and words
	    if (f0exist) showFertZeroRule(graph,tnode,f0,gcn);

	    // check lower nodes
	    for (int i=0; i<numc; i++) {
	      if (! f0[i]) gcn[i]->showFertZero(graph,tnode->child[i]);
	    }
	  }
	}
      }
    }
}

void
GraphNode::showFertZeroRule(Graph *graph, TNode *tnode,
			    vector<bool>& f0, vector<GraphNode*>& gcn)
{
  // show f0 rule "X n -> A B C" and f0words
  cout << "F0Rule: ";

  // lhs
  tnode->showNodeLabel();

  int numc=tnode->numc();
  for (int i=0; i<numc; i++)
    if (f0[i]) cout << " " << i;

  cout << " ->";
  
  // rhs
  for (int i=0; i<numc; i++) {
    cout << " ";
    tnode->child[i]->showNodeLabel();
  }
  cout << "\n";

  // show f0 words
  for (int i=0; i<numc; i++) {
    if (f0[i]) {
      cout << "F0Word: ";
      gcn[i]->showFertZeroWord(graph,tnode->child[i]);
      cout << "\n";
    }
  }
}

void
GraphNode::showFertZeroWord(Graph *graph, TNode *tnode)
{
  // show f0word "X -> Y -> eword"

  tnode->showNodeLabel();
  cout << " -> ";

  for (int xr=0; xr<tnode->numr; xr++)
    for (int xi=0; xi<3; xi++) {
      GraphArc *ap = arcs[xr*3+xi];
      if (ap && ap->rBeta>=0) {	// rBeta is used for best path marker
	// print Leaf or Children
	if (tnode->isLeaf()) {
	  int flen = ap->nlen;
	  if (flen!=0) cout << "error:: non-f0 word";
	  string eword = graph->esent[tnode->epos];
	  cout << eword;
	} else {
	  // best partition is ap->rBeta
	  Partition *pt = ap->pttn[ap->rBeta];
	  if (tnode->numc()!=1) cout << "error:: non-unary node";
	  else pt->child[0]->showFertZeroWord(graph,tnode->child[0]);
	}
      }
    }
}
