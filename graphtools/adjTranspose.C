#include "IO.h"
#include "parseCommandLine.h"
#include "graph.h"
#include "graphIO.h"
#include "dataGen.h"
#include "graphUtils.h"
#include "parallel.h"
using namespace benchIO;
using namespace dataGen;
using namespace std;



int parallel_main(int argc, char* argv[]) {
    commandLine P(argc,argv,"<inFile> <outFile>");
    pair<char*,char*> fnames = P.IOFileNames();
    char* iFile = fnames.first;
    char* oFile = fnames.second;

    // Get graph from file
    // graph<intT> G = readGraphFromFile<intT>(iFile);
    // edgeArray<intT> EA = edgesFromGraph(G);
    // G.del();
    std::cerr << argv[0] << ": reading input file...\n";
    _seq<char> S = readStringFromFile(iFile);
    std::cerr << argv[0] << ": splitting input by words...\n";
    words W = stringToWords(S.A, S.n);
    if (W.Strings[0] != AdjGraphHeader) {
	cout << "Bad input file (J header)" << endl;
	abort();
    }
    std::cerr << argv[0] << ": converting input to integers...\n";
    intT len = W.m-1;
    intT * In = new intT[len];
    {parallel_for(intT i=0; i < len; i++) In[i] = atol(W.Strings[i + 1]);}
    W.del(); // to deal with performance bug in malloc -- HV: need to free...

    intT n = In[0];
    intT m = In[1];
    if (len != n + m + 2) {
	cout << "Bad input file (edge count)" << endl;
	abort();
    }

    std::cerr << argv[0] << ": building transposed edge array...\n";
    edge<intT> * E = new edge<intT>[m];

    intT* offsets = In+2;
    intT* edges = In+2+n;
  //  intT running = 0;
    parallel_for (intT i=0; i < n; i++) {
	intT o = offsets[i];
	intT l = ((i == n-1) ? m : offsets[i+1])-offsets[i];
	for( intT j=0; j < l; ++j ) {
	    // store in transposed format
	    E[o+j].v = i;
	    E[o+j].u = edges[o+j];
//	    assert( running == o+j );
//	    ++running;
	}
    }

    // Re-convert to graph
    std::cerr << argv[0] << ": sorting by source vertex...\n";
    // cilkpub::cilk_sort_in_place( &E[0], &E[m], cmpuF<intT>() );
    std::sort( &E[0], &E[m], cmpuF<intT>() );

    std::cerr << argv[0] << ": computing offsets (in-degrees)...\n";
    offsets[0] = 0;
/*
    intT l=E[0].u;
    for( intT i=0; i < m; ++i ) {
	if( E[i].u != l ) {
	    // offsets[l+1] = i;
	    // offsets[l+2] = i; if degree(l+1) == 0
	    for( intT j=l; j < E[i].u; ++j )
		offsets[j+1] = i;
	    l = E[i].u;
	}
    }
    for( intT j=l; j < n-1; ++j )
	offsets[j+1] = m;
*/
    intT o = 0;
    for( intT v=0; v < n; ++v ) {
	offsets[v] = o;
	while( o < m && E[o].u == v )
	    ++o;
    }
    assert( o == m );

    std::cerr << argv[0] << ": computing edge sources...\n";
    parallel_for (intT i=0; i < n; i++) {
	intT * O = &edges[offsets[i]];
	edge<intT> * v = &E[offsets[i]];
	intT d = (i == n-1 ? m : offsets[i+1]) - offsets[i];
	for (intT j = 0; j < d; j++) 
	    O[j] = v[j].v; // destinations
    }
    delete[] E;

    // Write to file
    std::cerr << argv[0] << ": writing output graph...\n";
    int r = writeArrayToFile("AdjacencyGraph", In, 2+n+m, oFile);

    delete[] In;

    return 0;
}
