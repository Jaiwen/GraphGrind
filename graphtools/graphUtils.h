// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2011 Guy Blelloch and the PBBS team
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef _GRAPH_UTILS_INCLUDED
#define _GRAPH_UTILS_INCLUDED

#include "graph.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <math.h>
#include "parallel.h"
#include "graphUtils.h"
#include "sequence.h"
#include "utils.h"
#include "quickSort.h"
//#include "blockRadixSort.h"
#include "deterministicHash.h"

#include <cilkpub/sort.h>
typedef pair<intT, intT> intPair;
typedef pair<intT, pair<intT,intT> > intTriple;
using namespace std;

template <class E>
struct pairFirstCmp
{
	bool operator() (pair<intT,E> a, pair<intT,E> b)
        {
	    return a.first < b.first;
	}
};


template <class intT>
wghEdgeArray<intT> addRandWeights(edgeArray<intT> G) {
  intT m = G.nonZeros;
  intT n = G.numRows;
  wghEdge<intT> *E = newA(wghEdge<intT>, m);
  for (intT i=0; i < m; i++) {
    E[i].u = G.E[i].u;
    E[i].v = G.E[i].v;
    E[i].weight = utils::hashInt(i);
  }
  return wghEdgeArray<intT>(E, n, m);
}

template <class intT>
edgeArray<intT> edgesFromSparse(sparseRowMajor<double,intT> M) {
  edge<intT> *E = newA(edge<intT>,M.nonZeros);
  intT k = 0;
  for (intT i=0; i < M.numRows; i++) {
    for (intT j=M.Starts[i]; j < M.Starts[i+1]; j++) {
      if (M.Values[j] != 0.0) {
	E[k].u = i;
	E[k].v = M.ColIds[j];
	k++;
      }
    }
  }
  intT nonZeros = k;
  return edgeArray<intT>(E,M.numRows,M.numCols,nonZeros);
}

template <class intT>
int cmpInt(intT v, intT b) {
  return (v > b) ? 1 : ((v == b) ? 0 : -1);}

template <class intT>
struct hashEdge {
  typedef edge<intT>* eType;
  typedef edge<intT>* kType;
  eType empty() {return NULL;}
  kType getKey(eType v) {return v;}
  intT hash(kType e) {
    return utils::hashInt(e->u) + utils::hashInt(100*e->v); }
  int cmp(kType a, kType b) {
    int c = cmpInt(a->u, b->u);
    return (c == 0) ? cmpInt(a->v,b->v) : c;
  }
  bool replaceQ(eType v, eType b) {return 0;}
};

template <class intT>
_seq<edge<intT>* > removeDuplicates(_seq<edge<intT> *>  S) {
  return removeDuplicates(S,hashEdge<intT>());}



template <class intT>
edgeArray<intT> remDuplicates(edgeArray<intT> A) {
  intT m = A.nonZeros;
  edge<intT> **EP = newA(edge<intT>*,m);
  parallel_for (intT i=0;i < m; i++) EP[i] = A.E+i;
   _seq<edge<intT> *> F = removeDuplicates(_seq<edge<intT> *>(EP,m));
   //_seq<edge<intT>* > F = removeDuplicates(_seq<edge<intT> *>(EP,m),hashEdge<intT>());
  free(EP);
  intT l = F.n;
  edge<intT> *E = newA(edge<intT>,m);
  parallel_for (intT j=0; j < l; j++) E[j] = *F.A[j];
  F.del();
  return edgeArray<intT>(E,A.numRows,A.numCols,l);
}

template <class intT>
struct nEQF {bool operator() (edge<intT> e) {return (e.u != e.v);}};

template <class intT>
edgeArray<intT> makeSymmetric(edgeArray<intT> A) {
  cerr<<"make symmetric of edges"<<endl;
  intT m = A.nonZeros;
  edge<intT> *E = A.E;
  edge<intT> *F = newA(edge<intT>,2*m);
  if( F == 0 ) { std::cerr << "Out of memory: " << (2*m) << "\n"; abort(); }
  intT mm = sequence::filter(E,F,m,nEQF<intT>());
  parallel_for (intT i=0; i < mm; i++) {
    F[i+mm].u = F[i].v;
    F[i+mm].v = F[i].u;
  }
  edgeArray<intT> R = remDuplicates(edgeArray<intT>(F,A.numRows,A.numCols,2*mm));

  free(F);
  return R;
  //return edgeArray<intT>(F,A.numRows,A.numCols,2*mm);
}

template <class intT>
struct getuFA {
    edge<intT> * EA;

    getuFA( edge<intT> * _EA ) : EA( _EA ) { }

    intT operator() (intT i) { return EA[i].u; }
};

template <class intT>
struct getuF {intT operator() (edge<intT> e) {return e.u;} };

#ifndef WEIGHTED
template <class intT>
struct cmpuF {
    bool operator() (edge<intT> e0, edge<intT> e1) {
	return e0.u == e1.u ? e0.v < e1.v : e0.u < e1.u;
    }
};
#else
template <class intT>
struct cmpuF {
    bool operator() (wghEdge<intT> e0, wghEdge<intT> e1) {
	return e0.u == e1.u ? e0.v < e1.v : e0.u < e1.u;
    }
};
#endif

template <class intT>
#ifndef WEIGHTED
graph<intT> graphFromEdges(edgeArray<intT> EA, bool makeSym) {
#else
graph<intT> graphFromEdges(wghEdgeArray<intT> EA, bool makeSym) {
#endif
#ifndef WEIGHTED
  edgeArray<intT> A;
  if (makeSym) A = makeSymmetric<intT>(EA);
#else
  wghEdgeArray<intT> A;
  if (makeSym) {}
#endif
  else {  // should have copy constructor
      // TODO: create version that does not create copy of input
      // buto modifies it
      cerr<<"not symetric"<<endl;
#ifndef WEIGHTED
    edge<intT> *E = newA(edge<intT>,EA.nonZeros);
    parallel_for (intT i=0; i < EA.nonZeros; i++) E[i] = EA.E[i];
    A = edgeArray<intT>(E,EA.numRows,EA.numCols,EA.nonZeros);
#else
    wghEdge<intT> *E = newA(wghEdge<intT>,EA.m);
    parallel_for (intT i=0; i < EA.m; i++) E[i] = EA.E[i];
    A = wghEdgeArray<intT>(E,EA.n,EA.m);
#endif
  }
  cerr<<"edges to graph starts....."<<endl;
#ifndef WEIGHTED
  intT m = A.nonZeros;
  intT n = max<intT>(A.numCols,A.numRows);
#else
  intT m = A.m;
  intT n = A.n;
#endif
  //intT* offsets = newA(intT,n*2);
 // intSort::iSort(A.E,offsets,m,n,getuF<intT>());
  //cilkpub::cilk_sort_in_place( &A.E[0], &A.E[m], cmpuF<intT>() );
  quickSort(A.E,m,cmpuF<intT>());
  //cilkpub::cilk_sort_in_place( &A.E[0], &A.E[m], cmpuF<intT>() );
  cerr<<"edges sorting....."<<endl;

  intT* offsets = newA(intT,n+1);
  parallel_for( intT i=0; i <n; ++i ) { offsets[i] = INT_T_MAX; };
#ifndef WEIGHTED   
  intPair* temp = newA(intPair,m);
#else
  intTriple* temp = newA(intTriple,m);
#endif
  {
	parallel_for(intT i=0; i<m; i++)
#ifndef WEIGHTED
	    temp[i] = make_pair(A.E[i].u,A.E[i].v);
#else
	    temp[i] = make_pair(A.E[i].u, make_pair(A.E[i].v,A.E[i].weight));
#endif
  }
#ifndef WEIGHTED
  quickSort(temp,m,pairFirstCmp<intT>());
#else
  quickSort(temp,m,pairFirstCmp< pair<intT, intT> >());
#endif
  cerr<<"pairing...."<<endl;
  offsets[temp[0].first] = 0;
  {
	parallel_for(intT i=1; i<m; i++)
	{
	   if(temp[i].first !=temp[i-1].first)
	      offsets[temp[i].first] =i;
          // printf("temp[%ld] u %ld and v %ld and offset %ld\n",i,temp[i].first,temp[i].second,offsets[i]);
	}
  }
  cerr<<"scanIBack....."<<endl;
 // fill the empty degree offsets with closest offset value
  sequence::scanIBack(offsets, offsets, n, utils::minF<intT>(), (intT)m);
  //sequence::cumCountDAC( offsets, (intT)0, (intT)m, (intT)n,
	//	      getuFA<intT>(&A.E[0]) );
  offsets[n] = m;
  // Special case: last-numbered vertices have degree zero.
  // This is unlikely (?), so don't do much special
#if 0 //check offsets
  for( intT i=n-1; i > 0; --i )
      if( offsets[i] == 0 )
	  offsets[i] = offsets[i+1];
  for(intT i=0; i<n; i++) 
          printf("offset[%ld] %ld\n",i,offsets[i]);
#endif
/* Sequential code
  offsets[0] = 0;
  intT l=A.E[0].u;
  for( intT i=0; i < m; ++i ) {
      if( A.E[i].u != l ) {
	  for( intT j=l; j < A.E[i].u; ++j )
	      offsets[j+1] = i;
	  l = A.E[i].u;
      }
  }
  for( intT j=l; j < n; ++j )
      offsets[j+1] = m;
*/
//////////////////todo
  cerr<<"filling graph starts....."<<endl;
#ifndef WEIGHTED
  intT *X = newA(intT,m);
#else
  intT *X = newA(intT,2*m);
#endif
  vertex<intT> *v = newA(vertex<intT>,n);
  //      printf("the offsets %ld  %ld %ld \n",offsets[n],offsets[n-1],offsets[n-2]);
  parallel_for (intT i=0; i < n; i++) {
    //uintT o = i == 0 ? 0 : offsets[i-1];
    //uintT l = offsets[i] - o;
    uintT o = offsets[i];
    uintT l = offsets[i+1] - offsets[i];
    if( l < 0 || l > n ) {
        printf("the l =%ld in %ld n=%ld with offset %ld and %ld\n",l,i,n,offsets[i+1],offsets[i]);
	abort();
    }
    v[i].degree = l;
#ifndef WEIGHTED
    v[i].Neighbors = X+o;
#else
    v[i].Neighbors = X+2*o;
#endif
    for (intT j=0; j < l; j++) { // TODO: potentially parallelize (conditionally)
#ifndef WEIGHTED
      v[i].Neighbors[j] = A.E[o+j].v;
#else
      v[i].Neighbors[2*j] = A.E[o+j].v;
      v[i].Neighbors[2*j+1] = A.E[o+j].weight;
#endif
    }
  }
  A.del();
  free(offsets);
  cerr<<"graph generating...."<<endl;
  return graph<intT>(v,n,m,X);
}

template <class intT>
#ifndef WEIGHTED
edgeArray<intT> edgesFromGraph(graph<intT> G) {
#else
wghEdgeArray<intT> edgesFromGraph(graph<intT> G) {
#endif
  intT numRows = G.n;
  intT nonZeros = G.m;
  vertex<intT>* V = G.V;
#ifndef WEIGHTED
  edge<intT> *E = newA(edge<intT>, nonZeros);
#else
  wghEdge<intT> *E = newA(wghEdge<intT>, nonZeros);
#endif
  intT k = 0;
  for (intT j=0; j < numRows; j++)
    for (intT i = 0; i < V[j].degree; i++)
#ifndef WEIGHTED
      E[k++] = edge<intT>(j,V[j].Neighbors[i]);
#else
      E[k++] = wghEdge<intT>(j,V[j].getNeighbors(i),V[j].getWeight(i));
#endif
#ifndef WEIGHTED
  return edgeArray<intT>(E,numRows,numRows,nonZeros);
#else
  return wghEdgeArray<intT>(E,numRows,nonZeros);
#endif
}

template <class intT>
edgeArray<intT> edgesFromWghGraph(graph<intT> G) {
  intT numRows = G.n;
  intT nonZeros = G.m;
  vertex<intT>* V = G.V;
  edge<intT> *E = newA(edge<intT>, nonZeros);
  intT k = 0;
  for (intT j=0; j < numRows; j++)
    for (intT i = 0; i < V[j].degree; i++)
      E[k++] = edge<intT>(j,V[j].Neighbors[i]);
  return edgeArray<intT>(E,numRows,numRows,nonZeros);
}

template <class eType, class intT>
sparseRowMajor<eType,intT> sparseFromGraph(graph<intT> G) {
  intT numRows = G.n;
  intT nonZeros = G.m;
  vertex<intT>* V = G.V;
  intT *Starts = newA(intT,numRows+1);
  intT *ColIds = newA(intT,nonZeros);
  intT start = 0;
  for (intT i = 0; i < numRows; i++) {
    Starts[i] = start;
    start += V[i].degree;
  }
  Starts[numRows] = start;
  parallel_for (intT j=0; j < numRows; j++)
    for (intT i = 0; i < (Starts[j+1] - Starts[j]); i++) {
      ColIds[Starts[j]+i] = V[j].Neighbors[i];
    }
  return sparseRowMajor<eType,intT>(numRows,numRows,nonZeros,Starts,ColIds,NULL);
}

// if I is NULL then it randomly reorders
template <class intT>
graph<intT> graphReorder(graph<intT> Gr, intT* I) {
  intT n = Gr.n;
  intT m = Gr.m;
  bool noI = (I==NULL);
  if (noI) {
    I = newA(intT,Gr.n);
    parallel_for (intT i=0; i < Gr.n; i++) I[i] = i;
    random_shuffle(I,I+Gr.n);
  }
  vertex<intT> *V = newA(vertex<intT>,Gr.n);
  for (intT i=0; i < Gr.n; i++) V[I[i]] = Gr.V[i];
  for (intT i=0; i < Gr.n; i++) {
    for (intT j=0; j < V[i].degree; j++) {
      V[i].Neighbors[j] = I[V[i].Neighbors[j]];
    }
    sort(V[i].Neighbors,V[i].Neighbors+V[i].degree);
  }
  free(Gr.V);
  if (noI) free(I);
  return graph<intT>(V,n,m,Gr.allocatedInplace);
}

template <class intT>
int graphCheckConsistency(graph<intT> Gr) {
  vertex<intT> *V = Gr.V;
  intT edgecount = 0;
  for (intT i=0; i < Gr.n; i++) {
    edgecount += V[i].degree;
    for (intT j=0; j < V[i].degree; j++) {
      intT ngh = V[i].Neighbors[j];
      utils::myAssert(ngh >= 0 && ngh < Gr.n,
		      "graphCheckConsistency: bad edge");
    }
  }
  if (Gr.m != edgecount) {
    cout << "bad edge count in graphCheckConsistency: m = " 
	 << Gr.m << " sum of degrees = " << edgecount << endl;
    abort();
  }
  return 0;
}

template <class intT>
sparseRowMajor<double,intT> sparseFromCsrFile(const char* fname) {
  FILE *f = fopen(fname,"r");
  if (f == NULL) {
    cout << "Trying to open nonexistant file: " << fname << endl;
    abort();
  }

  intT numRows;  intT numCols;  intT nonZeros;
  intT nc = fread(&numRows, sizeof(intT), 1, f);
  nc = fread(&numCols, sizeof(intT), 1, f);
  nc = fread(&nonZeros, sizeof(intT), 1, f); 

  double *Values = (double *) malloc(sizeof(double)*nonZeros);
  intT *ColIds = (intT *) malloc(sizeof(intT)*nonZeros);
  intT *Starts = (intT *) malloc(sizeof(intT)*(1 + numRows));
  Starts[numRows] = nonZeros;

  size_t r;
  r = fread(Values, sizeof(double), nonZeros, f);
  r = fread(ColIds, sizeof(intT), nonZeros, f);
  r = fread(Starts, sizeof(intT), numRows, f); 
  fclose(f);
  return sparseRowMajor<double,intT>(numRows,numCols,nonZeros,Starts,ColIds,Values);
}

template <class intT>
edgeArray<intT> edgesFromMtxFile(const char* fname) {
  ifstream file (fname, ios::in);
  char* line = newA(char,1000);
  intT i,j = 0;
  while (file.peek() == '%') {
    j++;
    file.getline(line,1000);
  }
  intT numRows, numCols, nonZeros;
  file >> numRows >> numCols >> nonZeros;
  //cout << j << "," << numRows << "," << numCols << "," << nonZeros << endl;
  edge<intT> *E = newA(edge<intT>,nonZeros);
  double toss;
  for (i=0, j=0; i < nonZeros; i++) {
    file >> E[j].u >> E[j].v >> toss;
    E[j].u--;
    E[j].v--;
    if (toss != 0.0) j++;
  }
  nonZeros = j;
  //cout << "nonzeros = " << nonZeros << endl;
  file.close();  
  return edgeArray<intT>(E,numRows,numCols,nonZeros);
}

#endif // _GRAPH_UTILS_INCLUDED
