GraphGrind
===========================
Accelerating Graph Analytics by Utilising the Memory Locality of Graph Partitioning
======================

Compilation
--------

Recommended environment

* Intel icc-14.0.0 compiler
* c++ &gt;= 6.3.0 with support for Cilk+, 

To compile with g++ using Cilk, define the environment variable
CILK. 
 
```
$ make -j 16 
```
This is to compile and build with 16 threads in parallel. You can use the
number of your choice.

Cilk Library repo:
-----------
* cilk-swan repo contains the library of cilk_runtime. 
* Before running apps, export LD_LIBRARY_PATH=your-compiler-path:./cilk-swan/lib:$LD_LIBRARY_PATH
* USE cilk-swan/libl/libcilkrts.so as cilk_library 

Compiling flags:
----------
* NUMA: USE special NUMA allocation and NUMA_aware cilk_for for NUMA machine. (ICS paper)
* EDGE_HILBERT：USE the hilbert to re-bulid the coo edgelist for` better locality (ICPP)
      		If you want to use COO-CSR sort, define this value equal to 0 in the Makefile.
* PART96: USE sequential loop (for) within each parallel partition edge traversal, no atomics operation.(ICPP)
* CILK: USE cilk as parallelism tool.
Run Examples
-------
Example of running the code: An example unweighted graph
rMatGraph_J_5_100 and weighted graph rMatGraph_WJ_5_100 are
provided. Symmetric graphs should be called with the "-s"
flag for better performance. For example:

```
$ LD_PRELOAD="./cilk-swan/lib/libcilkrts.so" ./BFS -c 384 -r 100 -v vertex -b graph_input
``` 

* "-c" flag followed by an integer to indicate the number of coo partitions.
* "-v" flag followed by "edge" or "vertex", edge indicates using vertex balance partitioning, ensures almost equal number of vertices per partition. Vertex indicates using edge balance partitioning, enusres almost equal number of edges per partition. 
* "-r" flag followed by an integer to indicate the start source vertex for some search algorithms, e.g., BFS, BC and BellmanFord.
* "-rounds" flag followed by an integer to indicate how many rounds (iterations) you want to run.
* "-b" flag indicates binary graph format will be used.
* "-o" flag indicates this application uses VEBO graph, our graph ordering graph.

Input Format
-----------
The input format of an unweighted graphs should be in one of two
formats.

1) The adjacency graph format from the Problem Based Benchmark Suite
 (http://www.cs.cmu.edu/~pbbs/benchmarks/graphIO.html). The adjacency
 graph format starts with a sequence of offsets one for each vertex,
 followed by a sequence of directed edges ordered by their source
 vertex. The offset for a vertex i refers to the location of the start
 of a contiguous block of out edges for vertex i in the sequence of
 edges. The block continues until the offset of the next vertex, or
 the end if i is the last vertex. All vertices and offsets are 0 based
 and represented in decimal. The specific format is as follows:

AdjacencyGraph  
&lt;n>  
&lt;m>  
&lt;o0>  
&lt;o1>  
...  
&lt;o(n-1)>  
&lt;e0>  
&lt;e1>  
...  
&lt;e(m-1)>  

This file is represented as plain text.
The binary file is Galois format.

By default, format (1) is used. To run an input with format (2), pass
the "-b" flag as a command line argument.

By default the offsets are stored as 32-bit integers, and to represent
them as 64-bit integers, compile with the variable LONG defined. By
default the vertex IDs (edge values) are stored as 32-bit integers,
and to represent them as 64-bit integers, compile with the variable
EDGELONG defined.


Graph Applications
---------
Currently, GraphGrind comes with 8 implementation files: BFS.C
(breadth-first search), BC.C (betweenness centrality), BP.C (graph
Bayesian Belief Propagation), Components.C (connected components), BellmanFord.C
(Bellman-Ford shortest paths, need weighted graph), PageRank.C, PageRankDelta.C and
SPMV.C (Sparse Matrix-vector Mulplication,need weighted graph).


Publications:
-----------
1) Sun, Jiawen, Hans Vandierendonck, and Dimitrios S. Nikolopoulos. "GraphGrind: addressing load imbalance of graph partitioning." Proceedings of the International Conference on Supercomputing (ICS’17). ACM, 2017. 

2) Sun, Jiawen, Hans Vandierendonck, and Dimitrios S. Nikolopoulos. "Accelerating Graph Analytics by Utilising the Memory Locality of Graph Partitioning." Parallel Processing (ICPP’17), 2017 46th International Conference on. IEEE, 2017. 
