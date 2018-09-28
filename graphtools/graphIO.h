// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2010 Guy Blelloch and the PBBS team
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

#ifndef _BENCH_GRAPH_IO
#define _BENCH_GRAPH_IO

#include <iostream>
#include <fstream>
#include <stdint.h>

#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include "parallel.h"
#include "IO.h"
#include "graphUtils.h"

using namespace benchIO;
using namespace std;

template <class intT>
int xToStringLen(edge<intT> a) { 
  return xToStringLen(a.u) + xToStringLen(a.v) + 1;
}

template <class intT>
void xToString(char* s, edge<intT> a) { 
  int l = xToStringLen(a.u);
  xToString(s, a.u);
  s[l] = ' ';
  xToString(s+l+1, a.v);
}

template <class intT>
int xToStringLen(wghEdge<intT> a) { 
  return xToStringLen(a.u) + xToStringLen(a.v) + xToStringLen(a.weight) + 2;
}

template <class intT>
void xToString(char* s, wghEdge<intT> a) { 
  int lu = xToStringLen(a.u);
  int lv = xToStringLen(a.v);
  xToString(s, a.u);
  s[lu] = ' ';
  xToString(s+lu+1, a.v);
  s[lu+lv+1] = ' ';
  xToString(s+lu+lv+2, a.weight);
}

namespace benchIO {
  using namespace std;

  string AdjGraphHeader = "AdjacencyGraph";
  string AdjwghGraphHeader = "WeightedAdjacencyGraph";
  string EdgeArrayHeader = "EdgeArray";
  string WghEdgeArrayHeader = "WeightedEdgeArray";

  template <class intT>
      int writeGraphToFile( graph<intT> G, char* fname,
			    std::string header = AdjGraphHeader ) {
    intT m = G.m;
    intT n = G.n;
    intT totalLen = 2 + n + m;
    intT *Out = newA(intT, totalLen);
    Out[0] = n;
    Out[1] = m;
    parallel_for (intT i=0; i < n; i++) {
      Out[i+2] = G.V[i].degree;
    }
    intT total = sequence::scan(Out+2,Out+2,n,utils::addF<intT>(),(intT)0);
    for (intT i=0; i < n; i++) {
      intT *O = Out + (2 + n + Out[i+2]);
      vertex<intT> v = G.V[i];
      for (intT j = 0; j < v.degree; j++) 
#ifndef WEIGHTED
	O[j] = v.Neighbors[j];
#else
	O[j] = v.getNeighbors(j);
#endif
    }
    int r = writeArrayToFile(header, Out, totalLen, fname);
    free(Out);
    return r;
  }
  template <class intT>
      int writewghGraphToFile( graph<intT> G, char* fname,
			    std::string header = AdjwghGraphHeader ) {
    intT m = G.m;
    intT n = G.n;
    intT totalLen = 2 + n + 2*m;
    intT *Out = newA(intT, totalLen);
    Out[0] = n;
    Out[1] = m;
    parallel_for (intT i=0; i < n; i++) {
      Out[i+2] = G.V[i].degree;
    }
    intT total = sequence::scan(Out+2,Out+2,n,utils::addF<intT>(),(intT)0);
    for (intT i=0; i < n; i++) {
      intT *O = Out + (2 + n + Out[i+2]);
      vertex<intT> v = G.V[i];
      for (intT j = 0; j < v.degree; j++) 
      {
	O[j] = v.Neighbors[j];
      }
    }
    int r = writeArrayToFile(header, Out, totalLen, fname);
    free(Out);
    return r;
  }
  template <class intT>
  int writeEdgeArrayToBinary(edgeArray<intT> EA, char* fname) {
      intT m = EA.nonZeros;
      ofstream file( fname, ios::out | ios::binary );
      if (!file.is_open()) {
	  std::cout << "Unable to open file: " << fname << std::endl;
	  return 1;
      }
      file.write( (const char *)EA.E, m * sizeof( EA.E[0] ) );
      file.close();
      return 0;
  }

  template <class intT>
  int writeEdgeArrayToFile(edgeArray<intT> EA, char* fname) {
    intT m = EA.nonZeros;
    int r = writeArrayToFile(EdgeArrayHeader, EA.E, m, fname);
    //int r = writeArrayToFile(EA.E, m, fname);
    return r;
  }

  template <class intT>
  int writeWghEdgeArrayToFile(wghEdgeArray<intT> EA, char* fname) {
    intT m = EA.m;
    int r = writeArrayToFile(WghEdgeArrayHeader, EA.E, m, fname);
    return r;
  }

  template <class intT>
  edgeArray<intT> readEdgeArrayFromFile(char* fname) {
    _seq<char> S = readStringFromFile(fname);
    words W = stringToWords(S.A, S.n);
    if (W.Strings[0] != EdgeArrayHeader) {
      cout << "Bad input file (EA header)" << endl;
      abort();
    }
    intT n = (W.m-1)/2;
    edge<intT> *E = newA(edge<intT>,n);
    {parallel_for(intT i=0; i < n; i++)
      E[i] = edge<intT>(atol(W.Strings[2*i + 1]), 
		  atol(W.Strings[2*i + 2]));}
    //W.del(); // to deal with performance bug in malloc

    intT maxR = 0;
    intT maxC = 0;
    for (intT i=0; i < n; i++) {
      maxR = max<intT>(maxR, E[i].u);
      maxC = max<intT>(maxC, E[i].v);
    }
    return edgeArray<intT>(E, maxR+1, maxC+1, n);
  }

  template <class intT>
  wghEdgeArray<intT> readWghEdgeArrayFromFile(char* fname) {
    _seq<char> S = readStringFromFile(fname);
    words W = stringToWords(S.A, S.n);
    if (W.Strings[0] != WghEdgeArrayHeader) {
      cout << "Bad input file (WEA header)" << endl;
      abort();
    }
    intT n = (W.m-1)/3;
    wghEdge<intT> *E = newA(wghEdge<intT>,n);
    {parallel_for(intT i=0; i < n; i++)
      E[i] = wghEdge<intT>(atol(W.Strings[3*i + 1]), 
			   atol(W.Strings[3*i + 2]),
			   atof(W.Strings[3*i + 3]));}
    //W.del(); // to deal with performance bug in malloc

    intT maxR = 0;
    intT maxC = 0;
    for (intT i=0; i < n; i++) {
      maxR = max<intT>(maxR, E[i].u);
      maxC = max<intT>(maxC, E[i].v);
    }
    return wghEdgeArray<intT>(E, max<intT>(maxR,maxC)+1, n);
  }
  template <class intT>
  edgeArray<intT> readSNAP(char* fname) {
    _seq<char> S = readStringFromFile(fname);
    char* S2 = newA(char,S.n);
    //ignore starting lines with '#' and find where to start in file 
    long k=0;
    while(1) {
      if(S.A[k] == '#') {
	while(S.A[k++] != '\n') continue;
      }
      if(k >= S.n || S.A[k] != '#') break; 
    }
    parallel_for(long i=0;i<S.n-k;i++) S2[i] = S.A[k+i];
    S.del();

    words W = stringToWords(S2, S.n-k);
    long n = W.m/2;
    edge<intT> *E = newA(edge<intT>,n);
    {parallel_for(long i=0; i < n; i++)
      E[i] = edge<intT>(atol(W.Strings[2*i]), 
		  atol(W.Strings[2*i + 1]));}
    W.del();

    long maxR = 0;
    long maxC = 0;
    for (long i=0; i < n; i++) {
      maxR = max<intT>(maxR, E[i].u);
      maxC = max<intT>(maxC, E[i].v);
    }
    long maxrc = max<intT>(maxR,maxC) + 1;
    return edgeArray<intT>(E, maxrc, maxrc, n);
  }
  template <class intT>
  graph<intT> readGraphFromFile(char* fname) {
    _seq<char> S = readStringFromFile(fname);
    words W = stringToWords(S.A, S.n);
    if (W.Strings[0] != AdjGraphHeader) {
      cout << "Bad input file (J header)" << endl;
      abort();
    }
    intT len = W.m -1;
    intT * In = newA(intT, len);
    {parallel_for(intT i=0; i < len; i++) In[i] = atol(W.Strings[i + 1]);}
    W.del(); // to deal with performance bug in malloc -- HV: need to free...

    intT n = In[0];
    intT m = In[1];
    if (len != n + m + 2) {
      cout << "Bad input file (edge count)" << endl;
      abort();
    }
    vertex<intT> *v = newA(vertex<intT>,n);
    intT* offsets = In+2;
    intT* edges = In+2+n;
    parallel_for (intT i=0; i < n; i++) {
      intT o = offsets[i];
      intT l = ((i == n-1) ? m : offsets[i+1])-offsets[i];
      v[i].degree = l;
      v[i].Neighbors = edges+o;
    }
    return graph<intT>(v,n,m,In);
  }

template <class intT>
graph<intT> readGraphFromBinary(char* iFile, bool isSymmetric) {
  char* config = (char*) ".config";
  char* adj = (char*) ".adj";
  char* idx = (char*) ".idx";
  char configFile[strlen(iFile)+strlen(config)+1];
  char adjFile[strlen(iFile)+strlen(adj)+1];
  char idxFile[strlen(iFile)+strlen(idx)+1];
  *configFile=*adjFile=*idxFile='\0';
  strcat(configFile,iFile);
  strcat(adjFile,iFile);
  strcat(idxFile,iFile);
  strcat(configFile,config);
  strcat(adjFile,adj);
  strcat(idxFile,idx);

  ifstream in(configFile, ifstream::in);
  intT n;
  in >> n;
  in.close();

  ifstream in2(adjFile,ifstream::in | ios::binary); //stored as uints
  in2.seekg(0, ios::end);
  intT size = in2.tellg();
  in2.seekg(0);
  intT m = size/sizeof(uint);

  char* s = (char *) malloc(size);
  in2.read(s,size);
  in2.close();

  intT* edges = (intT*) s;
  ifstream in3(idxFile,ifstream::in | ios::binary); //stored as longs
  in3.seekg(0, ios::end);
  size = in3.tellg();
  in3.seekg(0);
  if(n != size/sizeof(intT)) { cout << "File size wrong\n"; abort(); }

  char* t = (char *) malloc(size);
  in3.read(t,size);
  in3.close();
  uintT* offsets = (uintT*) t;

  vertex<intT>* v = newA(vertex<intT>,n);

  {parallel_for(intT i=0;i<n;i++) {
    uintT o = i == 0 ? 0 : offsets[i-1];
   //uintT o = offsets[i];
   //uintT l = ((i==n-1) ? m : offsets[i+1])-offsets[i];
    uintT l=offsets[i]-o;
      v[i].setOutDegree(l);
      //v[i].degree=l;
      v[i].setNeighbors((intT*)edges+o);
    }}
  free(offsets);
  return graph<intT>(v,n,m,edges);
}

template <class intT>
graph<intT> readGraphFromGalois(char* fname, bool isSymmetric) {
    int fd;
    struct stat buffer;

    if( !(fd = open( fname, O_RDONLY )) ) {
	std::cerr << "Error in Galois input file: cannot open '"
		  << fname << "'\n";
	abort();
    }

    off_t len = lseek( fd, 0, SEEK_END );
    if( len == (off_t)-1 ) {
	std::cerr << "Error in Galois input file: seek failed\n";
	abort();
    }

    // Could add MAP_POPULATE to preload all pages into memory
    // Could add MAP_HUGETLB to use huge pages
    const char * data = (const char *)mmap( 0, len, PROT_READ,
					    MAP_SHARED, fd, 0 );
    if( data == (const char *)-1 ) {
	std::cerr << "Cannot mmap input graph file\n";
	abort();
    }

    uint64_t * header = (uint64_t*)&data[0];
    if( header[0] != 1 ) {
	std::cerr << "Error in Galois input file: version ("
		  << std::hex << header[0] << std::dec << ") != 1\n";
	abort();
    }

    intT n = header[2];
    intT m = header[3];
    bool wgh = header[1] == 4;

    size_t * offsets = (size_t*)(data + sizeof( header[0] ) * 4);
    uint32_t * edest
	= (uint32_t*)(data + sizeof( header[0] ) * 4 + sizeof( offsets[0] ) * n);
    uint32_t * ewght
	= (uint32_t*)(data + sizeof( header[0] ) * 4
		      + sizeof( offsets[0] ) * n
		      + sizeof( edest[0] ) * ( m + (m&1) ));

    intT * edges = 0;
#ifdef WEIGHTED
    edges = new intT[m*2];
#else
    edges = new intT[m];
#endif

#ifdef WEIGHTED
    cerr<<"wgh reading"<<endl;
    parallel_for( intT i=0;i<m;i++ ) {
	edges[2*i] = edest[i];
	edges[2*i+1] = ewght[i];
    }
#else
    // Copy even though because we could re-use mmap data because we need
    // to avoid disk accesses.
    parallel_for( intT i=0;i<m;i++ ) {
	edges[i] = edest[i];
    }
#endif

    vertex<intT> * v = newA(vertex<intT>,n);

    parallel_for(intT i=0;i<n;i++) {
	uintT o = i == 0 ? 0 : offsets[i-1];
	uintT l = offsets[i] - o;
	v[i].degree = l; // (l); 
#ifndef WEIGHTED
	v[i].Neighbors = (&edges[o]);
#else
	v[i].Neighbors = (&edges[2*o]);
#endif
    }
    if (!isSymmetric){
#if 1
     cerr<<"both in and out"<<endl;
            intT* tOffsets = new intT [n];
            {
                parallel_for(intT i=0; i<n; i++) tOffsets[i] = INT_T_MAX;
            }
#ifndef WEIGHTED
    intT* inEdges = new intT [m]; 
    pair<intT, intT> * temp = new pair<intT, intT > [m];
#else
    intT* inEdges = new intT [2*m];
    pair<intT, pair<intT,intT> >* temp = new pair<intT, pair<intT,intT> > [m];
#endif
                parallel_for(intT i=0; i<n; i++)
                {
                    uintT o = i == 0 ? 0 : offsets[i-1];
                    for(intT j=0; j<v[i].getDegree(); j++)
                    {
#ifndef WEIGHTED
                        temp[o+j] = make_pair(v[i].getNeighbors(j),i);
#else
                        temp[o+j] = make_pair(v[i].getNeighbors(j),make_pair(i,v[i].getWeight(j)));
#endif
                    }
                }
#ifndef WEIGHTED
            quickSort(temp,m,pairFirstCmp<intT>());
#else
            quickSort(temp,m,pairFirstCmp< pair<intT,intT> >());
#endif
            //tOffsets[0] = 0;
            tOffsets[temp[0].first] = 0;
#ifndef WEIGHTED
            inEdges[0] = temp[0].second;
#else
            inEdges[0] = temp[0].second.first;
            inEdges[1] = temp[0].second.second;
#endif
            parallel_for(intT i=1; i<m; i++)
            {
#ifndef WEIGHTED
                inEdges[i] = temp[i].second;
#else
                inEdges[2*i] = temp[i].second.first;
                inEdges[2*i+1] = temp[i].second.second;
#endif
                if(temp[i].first != temp[i-1].first)
                {
                    tOffsets[temp[i].first] = i;
                }
            }
            free(temp);

            //fill in offsets of degree 0 vertices by taking closest non-zero
            //offset to the right
            sequence::scanIBack(tOffsets,tOffsets,n,utils::minF<intT>(),(intT)m);

            {
                parallel_for(uintT i=0; i<n; i++)
                {
                    uintT o = tOffsets[i];
                    uintT l = ((i == n-1) ? m : tOffsets[i+1])-tOffsets[i];
                    v[i].indegree=l;
#ifndef WEIGHTED
                    v[i].setInNeighbors(inEdges+o);
#else
                    v[i].setInNeighbors(inEdges+2*o);
#endif
                }
            }
            delete [] tOffsets;
#endif
    munmap( (void*)data, len );
    close( fd );
    return graph<intT>( v, n, m, edges , inEdges);
    }
    else{
      munmap( (void*)data, len );
      close( fd );
      return graph<intT>( v, n, m, edges);
    }

}


};

#endif // _BENCH_GRAPH_IO
