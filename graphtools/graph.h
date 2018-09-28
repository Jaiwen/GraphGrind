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

#ifndef _GRAPH_INCLUDED
#define _GRAPH_INCLUDED

#include <iostream>
#include <algorithm>
#include "utils.h"
//typedef int vindex;

// **************************************************************
//    SPARSE ROW MAJOR REPRESENTATION
// **************************************************************

template <class ETYPE, class intT>
struct sparseRowMajor {
  intT numRows;
  intT numCols;
  intT nonZeros;
  intT* Starts;
  intT* ColIds;
  ETYPE* Values;
  void del() {free(Starts); free(ColIds); if (Values != NULL) free(Values);}
  sparseRowMajor(intT n, intT m, intT nz, intT* S, intT* C, ETYPE* V) :
    numRows(n), numCols(m), nonZeros(nz), 
    Starts(S), ColIds(C), Values(V) {}
};

//typedef sparseRowMajor<double> sparseRowMajorD;

// **************************************************************
//    EDGE ARRAY REPRESENTATION
// **************************************************************

template <class intT>
struct edge {
  intT u;
  intT v;
  edge(intT f, intT s) : u(f), v(s) {}
  edge() {}
};

template <class intT>
struct edgeArray {
  edge<intT>* E;
  intT numRows;
  intT numCols;
  intT nonZeros;
  void del() {free(E);}
  edgeArray(edge<intT> *EE, intT r, intT c, intT nz) :
    E(EE), numRows(r), numCols(c), nonZeros(nz) {}
  edgeArray() {}
};

// **************************************************************
//    WEIGHED EDGE ARRAY
// **************************************************************

template <class intT>
struct wghEdge {
  intT u, v;
  //double weight;
  intT weight;
  wghEdge(intT _u, intT _v, intT w) : u(_u), v(_v), weight(w) {}
  wghEdge() {}
};

template <class intT>
struct wghEdgeArray {
  wghEdge<intT> *E;
  intT n; intT m;
  wghEdgeArray(wghEdge<intT>* EE, intT nn, intT mm) : E(EE), n(nn), m(mm) {}
  void del() { free(E);}
  wghEdgeArray() {}
};

// **************************************************************
//    ADJACENCY ARRAY REPRESENTATION
// **************************************************************

template <class intT>
struct vertex {
#if 0
  intT* Neighbors;
  intT degree;
  void del() {free(Neighbors);}
  vertex(intT* N, intT d) : Neighbors(N), degree(d) {}
  intT getDegree() const{ return degree;}
#else
  intT* inNeighbors;
  intT* Neighbors;
  intT indegree;
  intT degree;
  void del() {free(Neighbors);free(inNeighbors);}
  vertex(intT* N, intT d) : Neighbors(N), degree(d) {}
  vertex(intT* N, intT* iN, intT id, intT d) : Neighbors(N), inNeighbors(iN), indegree(id), degree(d) {}
  intT getDegree() const{ return degree;}
  void setOutDegree(intT i ) {degree=i;}
  intT getInDegree() const{ return indegree;}
  void setInDegree(intT i ) {indegree=i;}
#endif
#ifndef WEIGHTED
  intT getNeighbors(intT i){ return Neighbors[i];}
  void setNeighbors(intT *i){ Neighbors=i;}
  void setNeighbors(intT _i, intT _j){Neighbors[_i]=_j; }
  void setInNeighbors(intT *i) {inNeighbors=i;}
  intT getInNeighbors(intT i){ return inNeighbors[i];}
#else
  intT getNeighbors(intT i){ return Neighbors[2*i];}
  intT getWeight(intT i) {return Neighbors[2*i+1];}
  intT getInNeighbors(intT i){ return inNeighbors[2*i];}
  intT getInWeight(intT i) {return inNeighbors[2*i+1];}
  void setNeighbors(intT *i){Neighbors=i;}
  void setInNeighbors(intT *i) {inNeighbors=i;}
#endif
};

template <class intT>
struct graph {
  vertex<intT> *V;
  intT n;
  intT m;
  intT* allocatedInplace;
  intT* inEdges;
  graph(vertex<intT>* VV, intT nn, intT mm) 
    : V(VV), n(nn), m(mm), allocatedInplace(NULL){}
  graph(vertex<intT>* VV, intT nn, intT mm, intT* ai) 
    : V(VV), n(nn), m(mm), allocatedInplace(ai){}
  graph(vertex<intT>* VV, intT nn, intT mm, intT* ai,intT* in) 
    : V(VV), n(nn), m(mm), allocatedInplace(ai),inEdges(in) {}
  graph copy() {
    vertex<intT>* VN = newA(vertex<intT>,n);
#ifndef WEIGHTED
    intT* inEdges = newA(intT,m);
#else
    intT* inEdges = newA(intT,2*m);
#endif
#if 0
    intT k = 0;
    for (intT i=0; i < n; i++) {
      VN[i] = V[i];
      VN[i].Neighbors = Edges + k;
      for (intT j =0; j < V[i].degree; j++) 
	Edges[k++] = V[i].Neighbors[j];
    }
    return graph(VN, n, m, Edges);
#endif
  } 
  void del() {
    if (allocatedInplace == NULL) 
      for (intT i=0; i < n; i++) V[i].del();
    else free(allocatedInplace);
    if (inEdges) free(inEdges);
    free(V);
  }
};

#endif // _GRAPH_INCLUDED
