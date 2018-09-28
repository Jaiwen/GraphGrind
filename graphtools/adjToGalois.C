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

// Adds a random double precision weight to each edge

#include <math.h>
#include <stdint.h>
#include <assert.h>
#include "IO.h"
#include "graph.h"
#include "graphIO.h"
#include "parseCommandLine.h"
#include "dataGen.h"
#include "parallel.h"
using namespace benchIO;
using namespace dataGen;
using namespace std;

int parallel_main(int argc, char* argv[]) {
  commandLine P(argc,argv,"<inFile> <outFile>");
  pair<char*,char*> fnames = P.IOFileNames();
  char* iFile = fnames.first;
  char* oFile = fnames.second;

  ifstream graph( iFile, ios::in );
  std::string type;
  size_t n, m;
  graph >> type; // AdjacencyGraph or WeightedAdjacencyGraph
  graph >> n;
  graph >> m;

  ofstream file( oFile, ios::out | ios::trunc | ios::binary );
  //uint64_t header[4] = {
  intT header[4] = {
      1, // version
      (type == "WeightedAdjacencyGraph" ? 4 : 1), // sizeof edge weight/data
      (intT)n,//(uint64_t)n, // num nodes
      (intT)m //(uint64_t)m // num edges
  };
  file.write( (const char *)header, sizeof(header) );

  // Now the per-vertex offsets in the edge list
  size_t zero;
  graph >> zero;
  assert( zero == 0 );
  for( size_t i=1; i < n; ++i ) {
      size_t off;
      graph >> off;
      file.write( (const char *)&off, sizeof(off) );
  }
  intT last = m;
  //uint64_t last = m;
  file.write( (const char *)&last, sizeof(last) );

  // Now the edge list
  for( size_t i=0; i < m; ++i ) {
      intT v;
      //uint64_t v;
      graph >> v;
      // Generate graph with long with uint32, edgelong with uint64
      uint32_t vv = v;
      if( v != (uint32_t)vv ) {
	  std::cerr << "Error: edge list element " << i << " exceeds 32 bits\n";
	  abort();
      }
      file.write( (const char *)&vv, sizeof(vv) );
  }
  
  // Write even number of edges
  if( m&1 ) {
      // Generate graph with long with uint32, edgelong with uint64
      uint32_t pad = 0;
      file.write( (const char *)&pad, sizeof(pad) );
  }

  // Edge data
  if( type == "AdjacencyGraph" ) {
      char v = 0;
      for( size_t i=0; i < m; ++i ) {
	  file.write( (const char *)&v, sizeof(v) );
      }
  } else if( type == "WeightedAdjacencyGraph" ) {
      for( size_t i=0; i < m; ++i ) {
	  uint32_t v;
	  graph >> v;
      // Generate graph with long with uint32, edgelong with uint64
	  uint32_t vv = v;
	  if( v != (uint32_t)vv ) {
	      std::cerr << "Error: edge weight " << i << " exceeds 32 bits\n";
	      abort();
	  }
	  file.write( (const char *)&vv, sizeof(vv) );
      }
  } else {
      std::cerr << "Unknown graph type '" << type << "'\n";
  }

  file.close();

  return 0;
}
