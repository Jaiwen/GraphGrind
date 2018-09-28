//Converts a SNAP graph (http://snap.stanford.edu/data/index.html) to
////Ligra adjacency graph format. To symmetrize the graph, pass the "-s"
////flag. For undirected graphs on SNAP, the "-s" flag must be passed
////since each edge appears in only one direction

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
  commandLine P(argc,argv,"[-s] <inFile> <outFile>");
  pair<char*,char*> fnames = P.IOFileNames();
  char* iFile = fnames.first;
  char* oFile = fnames.second;
  bool sym = P.getOption("-s");
  edgeArray<intT> G = readSNAP<intT>(iFile);
  writeGraphToFile(graphFromEdges(G,sym),oFile);
}

