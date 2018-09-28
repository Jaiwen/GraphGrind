#include <algorithm>

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
  
  // graph<intT> G = readGraphFromGalois<intT>(iFile,true);
  graph<intT> G = readGraphFromFile<intT>(iFile);
  
  edgeArray<intT> EA = edgesFromGraph(G);
  G.del();

//  std::random_shuffle( &EA.E[0], &EA.E[EA.nonZeros] );

#if 0
  intT* A = newA(intT,EA.nonZeros);
  parallel_for(intT i=0;i<EA.nonZeros;i++){
    if(EA.E[i].u == EA.E[i].v) A[i] = 0;
    else A[i] = 1;
  }
  intT m = sequence::plusScan(A,A,EA.nonZeros);

  if(m < EA.nonZeros){
    cout << m << " " << EA.nonZeros << endl;
    edge<intT>* E_packed = newA(edge<intT>,m);
    E_packed[0] = EA.E[0];
    parallel_for(intT i=1;i<EA.nonZeros;i++){
      if(A[i] != A[i]-1) E_packed[A[i]] = EA.E[i];
    }
    EA.nonZeros = m;
    free(EA.E);
    EA.E = E_packed;
  }

  free(A);
#endif
  // cout<<"a\n";
  // EA = makeSymmetric(EA);
  // cout<<"b\n";
  writeEdgeArrayToFile<intT>(EA,oFile);
  // writeEdgeArrayToBinary<intT>(EA,oFile);
  EA.del();
}
