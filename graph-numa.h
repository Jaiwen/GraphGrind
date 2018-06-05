// -*- C++ -*-
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include "parallel.h"
#include <assert.h>
#include "numa_page_check.h"
#include "mm.h"
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <cstring>
#include <string>
#include <utility>
#include <algorithm>

#include <sys/mman.h>
#include <numaif.h>
#include <numa.h>

#ifndef EDGES_HILBERT
#define EDGES_HILBERT 0
#endif

template<typename It, typename Cmp>
void mysort( It begin, It end, Cmp cmp )
{
    std::sort( begin, end, cmp );
}

using namespace std;
#define PAGESIZE (4096)

// To be used in boolean expressions
// **************************************************************
//    ADJACENCY ARRAY REPRESENTATION
// **************************************************************

class symmetricVertex
{
private:
    intE* neighbors;
    intT degree;
public:
    void del()
    {
        delete [] neighbors;
    }
    symmetricVertex() {}
    symmetricVertex(intE* n, intT d) : neighbors(n), degree(d)
    {
    }

#ifndef WEIGHTED
    intE getInNeighbor(intT j)
    {
        return neighbors[j];
    }
    intE getOutNeighbor(intT j)
    {
        return neighbors[j];
    }
    intE getInWeight(intT j)
    {
        return 1;
    }
    intE getOutWeight(intT j)
    {
        return 1;
    }
#else
    //weights are stored in the entry after the neighbor ID
    //so size of neighbor list is twice the degree
    intE getInNeighbor(intT j)
    {
        return neighbors[2*j];
    }
    intE getOutNeighbor(intT j)
    {
        return neighbors[2*j];
    }
    intE getInWeight(intT j)
    {
        return neighbors[2*j+1];
    }
    intE getOutWeight(intT j)
    {
        return neighbors[2*j+1];
    }
#endif
    intT getInDegree() const
    {
        return degree;
    }
    intT getOutDegree() const
    {
        return degree;
    }
    void setInNeighbors(intE* _i)
    {
        neighbors = _i;
    }
    void setOutNeighbors(intE* _i)
    {
        neighbors=_i;
    }
    void setInDegree(intT _d)
    {
        degree = _d;
    }
    void setOutDegree(intT _d)
    {
        degree = _d;
    }
    intE* getInNeighborPtr()
    {
        return neighbors;
    }
    intE* getOutNeighborPtr()
    {
        return neighbors;
    }
    void flipEdges() {}
};

class asymmetricVertex
{
private:
    intE* inNeighbors;
    intE* outNeighbors;
    intT outDegree;
    intT inDegree;

public:
    intE* getInNeighborPtr()
    {
        return inNeighbors;
    }
    intE* getOutNeighborPtr()
    {
        return outNeighbors;
    }
    void del()
    {
        delete [] inNeighbors;
        delete [] outNeighbors;
    }
    asymmetricVertex() {}
    asymmetricVertex(intE* iN, intE* oN, intT id, intT od
                    ) : inNeighbors(iN), outNeighbors(oN), inDegree(id), outDegree(od)
    {}
#ifndef WEIGHTED
    intE getInNeighbor(intT j)
    {
        return inNeighbors[j];
    }
    intE getOutNeighbor(intT j)
    {
        return outNeighbors[j];
    }
    intE getInWeight(intT j)
    {
        return 1;
    }
    intE getOutWeight(intT j)
    {
        return 1;
    }
#else
    intE getInNeighbor(intT j)
    {
        return inNeighbors[2*j];
    }
    intE getOutNeighbor(intT j)
    {
        return outNeighbors[2*j];
    }
    intE getInWeight(intT j)
    {
        return inNeighbors[2*j+1];
    }
    intE getOutWeight(intT j)
    {
        return outNeighbors[2*j+1];
    }
#endif
    intT getInDegree() const
    {
        return inDegree;
    }
    intT getOutDegree() const
    {
        return outDegree;
    }
    void setInNeighbors(intE* _i)
    {
        inNeighbors = _i;
    }
    void setOutNeighbors(intE* _i)
    {
        outNeighbors = _i;
    }
    void setInDegree(intT _d)
    {
        inDegree = _d;
    }
    void setOutDegree(intT _d)
    {
        outDegree = _d;

    }
    void flipEdges()
    {
        swap(inNeighbors,outNeighbors);
        swap(inDegree,outDegree);
    }
};

class Edge_Hilbert;
class Edge
{
private:
    intT src, dst;
#ifdef WEIGHTED
    intE weight;
#endif

public:
    Edge() { }
#ifdef WEIGHTED
    Edge( intT s, intT d, intE w ) : src( s ), dst( d ), weight( w ) { }
#else
    Edge( intT s, intT d, intE w ) : src( s ), dst( d ) { }
    Edge( intT s, intT d ) : src( s ), dst( d ) { }
#endif
    Edge( const Edge & e ) : src( e.src ), dst( e.dst )
#ifdef WEIGHTED
        , weight( e.weight )
#endif
    { }
        Edge( const Edge_Hilbert & e );

    intE getSource() const
    {
        return src;
    }
    intE getDestination() const
    {
        return dst;
    }

#ifdef WEIGHTED
    intT getWeight() const
    {
        return weight;
    }
#else
    intT getWeight() const
    {
        return 1;
    }
#endif
    void flipEdge()
    {
      swap(src,dst);
    }
};
class Edge_Hilbert : public Edge
{
private:
    intT e2d;

public:
    Edge_Hilbert() { }
#ifdef WEIGHTED
    Edge_Hilbert( intT s, intT d, intE w ) : Edge(s,d,w), e2d(0) {}
#else
    Edge_Hilbert( intT s, intT d, intE w ) : Edge(s,d), e2d(0) { }
    Edge_Hilbert( intT s, intT d ) : Edge(s,d), e2d(0) { }
#endif
    Edge_Hilbert( const Edge & e ) : Edge(e), e2d(0) {}
	
    
	intT getE2d() const
	{
		return e2d;
	}
	void setE2d(intT value)
	{
		e2d = value;
	}

};

Edge::Edge( const Edge_Hilbert & e ) : src( e.src ), dst( e.dst )
#ifdef WEIGHTED
        , weight( e.weight )
#endif
    { }

template<typename T>
typename std::make_unsigned<T>::type roundUpPow2( T n_u )
{
    const unsigned int num_bits = sizeof(T) * 8;

    typename std::make_unsigned<T>::type n = n_u;
    --n;
    for( unsigned int shift=1; shift < num_bits; shift <<= 1 )
        n |= n >> shift;
    ++n;

    return n;
}

class PairSort
{
  intT n;
public:
   PairSort ( intT n_) : n(n_){ }
   bool operator () ( const intT & i, const intT & j ) 
   {
        return i>j;
        //return i.second.getInDegree() < j.second.getInDegree();
   }
};

class CSRSort
{
  intT n;
public:
   CSRSort ( intT n_) : n(n_){ }
   bool operator () ( const Edge & i, const Edge & j ) 
   {
           return i.getSource()<j.getSource();
   }
};
//second sort for csr
class CSRDestSort
{
  intT n;
public:
   CSRDestSort ( intT n_) : n(n_){ }
   bool operator () ( const Edge & i, const Edge & j ) 
   {
           if (i.getSource()==j.getSource())
             return i.getDestination()<j.getDestination();
           else
             return i.getSource()<j.getSource();
   }
};

// Source code based on https://en.wikipedia.org/wiki/Hilbert_curve
class HilbertEdgeSort
{
    intT n;

public:
    HilbertEdgeSort( intT n_ ) : n( roundUpPow2(n_) ) { }

    bool operator () ( const Edge_Hilbert & l, const Edge_Hilbert & r ) const
    {
        return e2d( l ) < e2d( r );
    }

private:
    intT e2d( const Edge_Hilbert & e ) const
    {
        if(e.getE2d() == 0)
		{
			const_cast<Edge_Hilbert *>(&e)->setE2d(xy2d(e.getSource(), e.getDestination()));
		}
        return e.getE2d();				  
    }
    //convert (x,y) to d
    intT xy2d( intT x, intT y ) const
    {
        intT rx, ry, s, d=0;
        for (s=n/2; s>0; s/=2)
        {
            rx = (x & s) > 0;
            ry = (y & s) > 0;
            d += s * s * ((3 * rx) ^ ry);
            rot(s, &x, &y, rx, ry);
        }
        return d;
    }

    //convert d to (x,y)
    void d2xy(intT d, intT *x, intT *y) const
    {
        intT rx, ry, s, t=d;
        *x = *y = 0;
        for (s=1; s<n; s*=2)
        {
            rx = 1 & (t/2);
            ry = 1 & (t ^ rx);
            rot(s, x, y, rx, ry);
            *x += s * rx;
            *y += s * ry;
            t /= 4;
        }
    }
    static void rot( intT n, intT *x, intT *y, intT rx, intT ry )
    {
        if (ry == 0)
        {
            if (rx == 1)
            {
                *x = n-1 - *x;
                *y = n-1 - *y;
            }
            std::swap( *x, *y );
        }
    }
};

template<class Edge>
class EdgeList
{
private:
    mmap_ptr<Edge> edges;
    intE num_edges;
    intT num_vertices;
    int numanode;
public:
    EdgeList() {}
    EdgeList(intE m, intT n, int pp) : num_edges(m), num_vertices(n), numanode(pp)
    {
	  edges.local_allocate(num_edges,numanode);
    }
    void del()
    {
          edges.del();
    }
    typedef Edge * iterator;
    typedef const Edge * const_iterator;
    
    typedef Edge_Hilbert * hil_iterator;
    typedef const Edge_Hilbert * const_hil_iterator;

    iterator begin()
    {
        return &edges[0];
    }
    iterator end()
    {
        return &edges[num_edges];
    }
    const_iterator begin() const
    {
        return &edges[0];
    }
    const_iterator end() const
    {
        return &edges[num_edges];
    }
    const_iterator cbegin() const
    {
        return &edges[0];
    }
    const_iterator cend() const
    {
        return &edges[num_edges];
    }

    size_t get_num_edges() const
    {
        return num_edges;
    }
    size_t get_num_vertices() const
    {
        return num_vertices;
    }
    Edge & operator[] ( long i )
    {
        return edges[i];
    }
    const Edge & operator[] ( long i ) const
    {
        return edges[i];
    }
    void hilbert_sort()
    {
		mmap_ptr<Edge_Hilbert> hilbert_edges;
		hilbert_edges.local_allocate(num_edges,numanode);
		std::copy( begin(), end(), hilbert_edges.get() );													 
		mysort(&hilbert_edges[0], &hilbert_edges[num_edges], HilbertEdgeSort(num_edges));
		std::copy( &hilbert_edges[0], &hilbert_edges[num_edges], edges.get() );														   
		hilbert_edges.del();
    }
    void CSR_sort()
    {
	mysort(&edges[0], &edges[num_edges], CSRDestSort(num_edges));
    }
};

// wholeGraph for whole graph loading
// and sparse iteration graph traversal
// uses NUMA interleave to allocate
template <class vertex>
class wholeGraph
{
public:
    mmap_ptr<vertex> V;  //mmap
    intT n;
    intT m;
    mmap_ptr<intT> flags;
    mmap_ptr<intE> allocatedInplace;
    mmap_ptr<intE> inEdges;
    bool transposed;
    bool isSymmetric;

    wholeGraph() {}
    wholeGraph(intT nn, intT mm, bool issym)
        : n(nn), m(mm), isSymmetric(issym),
          transposed(false)
    {

//NUMA_AWARE and Ligra_normal without partition
           V.Interleave_allocate(n);
#ifndef WEIGHTED
	   allocatedInplace.Interleave_allocate(m);
#else//WEIGHTED
           allocatedInplace.Interleave_allocate(2*m);
#endif
            if(!isSymmetric)
            {
#ifndef WEIGHTED
                inEdges.Interleave_allocate(m);
#else//WEIGHTED
                inEdges.Interleave_allocate(2*m);
#endif
            }
    }

    void del()
    {
        flags.del();
        allocatedInplace.del();
        V.del();
        inEdges.del();
    }

//    void reorder_vertices( intT * __restrict reorder );
//    asymmetricVertex * newV = new asymmetricVertex [n];

    void transpose()
    {
        if(!isSymmetric)
        {
            parallel_for(intT i=0; i<n; i++)
		V[i].flipEdges();
            transposed = !transposed;
        }
    }

};


template <class vertex>
class graph
{
public:
    intT n;
    intT m;
    intT CSCVn;
    mmap_ptr<vertex> V;  //mmap
    mmap_ptr< pair<intT,vertex> > CSCV;
    partitioner csc;
    bool transposed;
    bool isSymmetric;

//Compressed partitioned_graph after partitioning,
//remove the empty vertices,
//restruct the vertext set,
//CSC for backwark and CSR for forward;
    graph() {}
    graph(intT nn, intT mm, intT cscn, int coo, bool issym)
        : n(nn), m(mm), isSymmetric(issym),
          transposed(false),
          CSCVn(cscn),
          csc(coo,cscn)
    {
//NUMA_AWARE and Ligra_normal without partition
    }

    void del()
    {
        CSCV.del(); 
    //    V.del();
    }

  //  void reorder_vertices( intT * __restrict reorder );
    const partitioner & get_csc_partitioner() const
    {
        return csc;
    }
    void transpose()
    {
        if(!isSymmetric)
        {
            if(CSCV)
            {
                parallel_for (intT i=0; i<CSCVn; i++)
		    CSCV[i].second.flipEdges();
            }
            parallel_for(intT i=0; i<n; i++)
		V[i].flipEdges();
            transposed = !transposed;
        }
    }

};
//Graph partitioning, contain partitioned graph,
//partitioner value
//Select there partitioning method,
//Mix partition (only four partition), first partitioning 
//as two partition by souce and then repartitioning by destination
//from two to four 
//PartitionBySour : Partition By source, all the 
//home vertices contain out-degrees, improve locality 
//for backward, BFS, BC, Component and PageRank
//PartitionByDest : Partitino By destination, all the
//home vertices contain in-degrees, improve locality
//for forward, PageRankDelta, SPMV, BP, BellmanFord
template <class vertex>
class partitioned_graph
{
public:
    typedef vertex vertex_type;

    partitioner coo_partition;
    intT m,n;
    bool source;
    bool part_ver;
    bool part_relabel;
private:
    // All variables should be private
    EdgeList<Edge> * localEdgeList;
    graph<vertex> CSCGraph;
public:
    partitioned_graph( wholeGraph<vertex> & GA, 
                       int coo_part, bool partition_source, bool partition_vertex, bool partition_relabel)
        : m(GA.m),n(GA.n),
          coo_partition(coo_part,GA.n), 
          source(partition_source), part_ver(partition_vertex),part_relabel(partition_relabel)
    {
        //cerr<<"m="<<m<<"n="<<n<<endl;
        const int coo_perNode = coo_partition.get_num_per_node_partitions();
        localEdgeList = new EdgeList<Edge>[coo_part];
        if(partition_vertex)
             partitionByVertex( GA, coo_part, coo_partition.as_array(), partition_relabel);
        else
             partitionByDegree( GA, coo_part, coo_partition.as_array(), partition_source,partition_relabel );
        coo_partition.compute_starts();
#if CPU_PARTITION
        coo_partition.compute_vertexstarts();
#endif
          timer par;
          par.start(); 
          if(!partition_vertex){
            cerr<<"edge partitioning...."<<endl;
	    map_partitionL( coo_partition, [&]( int p ) {
		    int i = p / coo_partition.get_num_per_node_partitions();
                    if( partition_source)
                        localEdgeList[p] = COOPartitionBySour( GA, coo_partition.start_of(p), coo_partition.start_of(p+1),i);
                    else   //Partition Function for ICS and ICPP paper 
                        localEdgeList[p] = COOPartitionByDest( GA, coo_partition.start_of(p), coo_partition.start_of(p+1),i);
#if EDGES_HILBERT
                        localEdgeList[p].hilbert_sort();
#else
                        localEdgeList[p].CSR_sort();
#endif
                } );
	  }
         cerr<<"COO: "<<par.stop()<<endl;
        //cerr<<"CSC Chunk"<<endl;
        CSCGraph = PartitionByDest(GA,0,GA.m,coo_part);
        if(partition_vertex)
            cscpartitionByVertex(CSCGraph,coo_part,CSCGraph.csc.as_array(),partition_relabel);
        else
            cscpartitionByDegree(CSCGraph,coo_part,CSCGraph.csc.as_array(),partition_source, partition_relabel);
        CSCGraph.csc.compute_starts(); 

    }
    void del()
    {
        
        for( int p=0; p < coo_partition.get_num_partitions(); ++p )
	    localEdgeList[p].del();

        delete [] localEdgeList;
        CSCGraph.del();
    }

    const EdgeList<Edge> & get_edge_list_partition( intT p )
    {
        return localEdgeList[p];
    }

    graph<vertex> & get_partition()
    {
        return CSCGraph;
    }

    // Get a partition of the graph to operate on
    int get_num_coo_partitions() const
    {
        return coo_partition.get_num_partitions();
    }
    // Get the partitioner object constructed for this graph
    const partitioner & get_partitioner() const
    {
        return coo_partition;
    }
    const partitioner & get_coo_partitioner() const
    {
        return coo_partition;
    }
   // wholeGraph<vertex> & getWholeGraph() { return GA; }
    void transpose()
    {
	// Do transpositions on NUMA node storing graph partition
	CSCGraph.transpose();
    }
    bool transposed() const {
	return CSCGraph.transposed; // Assume status of partitions matches whole graph
    }

private:
// These functions are specific to the partitioned graph. No one needs
// to know about them, I think.
// graphFilter is polymer method, partitioend graph:
// Backward: each local vertex contains its own out-degree
// Forward: each local vertex contains its own in-degree
// PartitionByDest :
// Backward: each local vertex contains its own in-degree
// Forward:  each local vertex contains its own in-degree
// PartitionBySour:
// Backward: each local vertex contains its own out-degree
// Forward: each local vertex contains its own out-degree
    void partitionByDegree( wholeGraph<vertex> GA, int numOfNode, intT *sizeArr,
                            bool useOutDegree , bool useRelabel);
    void cscpartitionByDegree( graph<vertex> GA, int numOfNode, intT *sizeArr,
                            bool useOutDegree, bool useRelabel );
    void cscpartitionByVertex( graph<vertex> GA, int numOfNode, intT *sizeArr,
                            bool useRelabel );
    void partitionByVertex( wholeGraph<vertex> GA, int numOfNode, intT *sizeArr,
                            bool useRelabel );
    graph<vertex> PartitionByDest(wholeGraph<vertex>& GA, int rangeLow, int rangeHi ,int numanode);
    graph<vertex> PartitionBySour(wholeGraph<vertex>& GA, int rangeLow, int rangeHi ,int numanode);
    EdgeList<Edge> COOPartitionByDest(wholeGraph<vertex>& GA, int rangeLow, int rangeHi ,int numanode);
    EdgeList<Edge> COOPartitionBySour(wholeGraph<vertex>& GA, int rangeLow, int rangeHi ,int numanode);
    wholeGraph<vertex> PartitionByDestW(wholeGraph<vertex>& GA, int rangeLow, int rangeHi ,int numanode);
    wholeGraph<vertex> PartitionBySourW(wholeGraph<vertex>& GA, int rangeLow, int rangeHi ,int numanode);
};
// ======================================================================
// Graph Filtering (Graph Partitioned)
// ======================================================================
//This method is partitioned by outdegree, CSR, store the all out-degree for
//all local vertices of each partitions.
//And part indegree for whole vertices
// Place edge (u,v) in partition of u
template <class vertex>
graph<vertex> partitioned_graph<vertex>::PartitionBySour(wholeGraph<vertex> &GA, int rangeLow, int rangeHi,int numanode)
{
    vertex *V = GA.V;
    const intT n = GA.n;
    bool isSymmetric = GA.isSymmetric;

    intT nnz = 0, nnzi = 0, nnzo = 0, ne = 0, ni=0;
    for( intT i=0; i < n; ++i )
    {
            if( V[i].getInDegree() != 0 )
                ++nnzi;
            if( V[i].getOutDegree() != 0 )
                ++nnzo;
    }

    graph<vertex> FG(n, GA.m, nnzi,nnzo,numanode,isSymmetric);
    FG.CSCV.Interleave_allocate(FG.CSCVn);
    FG.CSRV.Interleave_allocate(FG.CSRVn);

    intT k=0;
    for( intT i=0; i < n; ++i )
    {
        if( V[i].getInDegree() != 0 )
        {
            FG.CSCV[k].first = i;
            FG.CSCV[k].second = V[i];
            k++;
        }
    }
    intT a=0;
    for( intT j=0; j < n; ++j )
    {
        if( V[j].getOutDegree() != 0 )
        {
            FG.CSRV[a].first = j;
            FG.CSRV[a].second = V[j];
            a++;
        }
    }
#if 0
    cerr << "CSCCSRCompressed graph n=" << n << " Compressed=" << nnz
         << " CSC=" << nnzi << " CSR=" << nnzo << " out-edges=" << ne 
         << " in-edges" << ni << " compressed" << endl;
#endif 
    return FG;//return graph<vertex>(newVertexSet, GA.n, GA.m);
}
//This is used for MIX method 
//get the wholeGraph for next source partition
template <class vertex>
wholeGraph<vertex> partitioned_graph<vertex>::PartitionByDestW(wholeGraph<vertex> &GA, int rangeLow, int rangeHi,int numanode)
{
    vertex *V = GA.V;
    const intT n = GA.n;
    bool isSymmetric = GA.isSymmetric;
    intT *counters = new intT [n];
    intT *offsets = new intT [n];
    intT *inCounters = new intT [n];
    intT *inOffsets = new intT [n];
    {
        parallel_for (intT i = 0; i < n; i++)
        {
            counters[i] = 0;
            inCounters[i] = 0;
            intT d = V[i].getOutDegree();
            for (intT j = 0; j < d; j++)
            {
                intT ngh = V[i].getOutNeighbor(j);
                if (rangeLow <= ngh && ngh < rangeHi)
                    counters[i]++;
            }
            if(!isSymmetric)
            {
                if(rangeLow <= i && i < rangeHi)
                    inCounters[i] = V[i].getInDegree();
            }
        }
    }
    intT totalSize = 0;
    intT totalInSize = 0;
    for (intT i = 0; i < n; i++)
    {
        offsets[i] = totalSize;
        totalSize += counters[i];

        if(!isSymmetric)
        {
            inOffsets[i] = totalInSize;
            totalInSize += inCounters[i];
        }
    }

    if(!isSymmetric)
        assert( totalSize == totalInSize );

    wholeGraph<vertex> FG(n, totalSize, isSymmetric);
    {
        parallel_for (intT i = 0; i < n; i++)
        {
            FG.V[i].setOutDegree(counters[i]);
            if(!isSymmetric)
                FG.V[i].setInDegree(inCounters[i]);
        }
    }
    delete [] counters;
    delete [] inCounters;

    intE *edges = FG.allocatedInplace;
    intE *inEdges = FG.inEdges;

    {
        parallel_for (intT i = 0; i < n; i++)
        {
#ifndef WEIGHTED
            intE *localEdges = &edges[offsets[i]];
#else
            intE *localEdges = &edges[offsets[i]*2];
#endif
            intT counter = 0;
            intT d = V[i].getOutDegree();
            for (intT j = 0; j < d; j++)
            {
#ifndef WEIGHTED
                intT ngh = V[i].getOutNeighbor(j);
#else
                intT ngh = V[i].getOutNeighbor(j);
                intT wgh = V[i].getOutWeight(j);
#endif
                if (rangeLow <= ngh && ngh < rangeHi)
                {
#ifndef WEIGHTED
                    localEdges[counter] = ngh;
#else
                    localEdges[counter*2]= ngh;
                    localEdges[counter*2+1]= wgh;
#endif
                    counter++;
                }
            }

            FG.V[i].setOutNeighbors(localEdges);
            if(!isSymmetric)
            {
#ifndef WEIGHTED
                intE *localInEdges = &inEdges[inOffsets[i]];
#else
                intE *localInEdges = &inEdges[inOffsets[i]*2];
#endif
                intT incounter = 0;
                if (rangeLow <= i && i < rangeHi)
                {
                    d = V[i].getInDegree();
                    for (intT j = 0; j < d; j++)
                    {
#ifndef WEIGHTED
                        intT ngh = V[i].getInNeighbor(j);
                        localInEdges[incounter] = ngh;
#else
                        intT ngh = V[i].getInNeighbor(j);
                        intT wgh = V[i].getInWeight(j);

                        localInEdges[incounter*2] = ngh;
                        localInEdges[incounter*2+1] = wgh;
#endif

                        incounter++;
                    }
                    FG.V[i].setInNeighbors(localInEdges);
                }
            }
        }
    }

    delete [] offsets;
    delete [] inOffsets;
    return FG;
}

template <class vertex>
wholeGraph<vertex> partitioned_graph<vertex>::PartitionBySourW(wholeGraph<vertex> &GA, int rangeLow, int rangeHi,int numanode)
{
    vertex *V = GA.V;
    const intT n = GA.n;
    bool isSymmetric = GA.isSymmetric;
    intT *counters = new intT [n];
    intT *offsets = new intT [n];
    intT *inCounters = new intT [n];
    intT *inOffsets = new intT [n];
    {
        parallel_for (intT i = 0; i < n; i++)
        {
            counters[i] = 0;
            inCounters[i] = 0;
            if(!isSymmetric)
            {
              intT d = V[i].getInDegree();
              for (intT j = 0; j < d; j++)
              {
                intT ngh = V[i].getInNeighbor(j);
                if (rangeLow <= ngh && ngh < rangeHi)
                    inCounters[i]++;
              }
	    }
              if(rangeLow <= i && i < rangeHi)
                  counters[i] = V[i].getOutDegree();
        }
    }
    intT totalSize = 0;
    intT totalInSize = 0;
    for (intT i = 0; i < n; i++)
    {
        offsets[i] = totalSize;
        totalSize += counters[i];

        if(!isSymmetric)
        {
            inOffsets[i] = totalInSize;
            totalInSize += inCounters[i];
        }
    }

    if(!isSymmetric)
        assert( totalSize == totalInSize );

    wholeGraph<vertex> FG(n, totalSize, isSymmetric);
    {
        parallel_for (intT i = 0; i < n; i++)
        {
            FG.V[i].setOutDegree(counters[i]);
            if(!isSymmetric)
                FG.V[i].setInDegree(inCounters[i]);
        }
    }
    delete [] counters;
    delete [] inCounters;

    intE *edges = FG.allocatedInplace;
    intE *inEdges = FG.inEdges;
    {
        parallel_for (intT i = 0; i < n; i++)
        {
#ifndef WEIGHTED
            intE *localInEdges = &inEdges[inOffsets[i]];
#else
            intE *localInEdges = &inEdges[inOffsets[i]*2];
#endif
            intT incounter = 0;
            if(!isSymmetric)
            {
                intT d = V[i].getInDegree();
                for (intT j = 0; j < d; j++)
                {
#ifndef WEIGHTED
                    intT ngh = V[i].getInNeighbor(j);
#else
                    intT ngh = V[i].getInNeighbor(j);
                    intT wgh = V[i].getInWeight(j);
#endif
                    if (rangeLow <= ngh && ngh < rangeHi)
                    {
#ifndef WEIGHTED
                        localInEdges[incounter] = ngh;
#else
                        localInEdges[incounter*2]= ngh;
                        localInEdges[incounter*2+1]= wgh;
#endif

                        incounter++;
                    }
                }
                FG.V[i].setInNeighbors(localInEdges);
            }
#ifndef WEIGHTED
            intE *localEdges = &edges[offsets[i]];
#else
            intE *localEdges = &edges[offsets[i]*2];
#endif
            intT counter = 0;
            if (rangeLow <= i && i < rangeHi)
            {
                intT ind = V[i].getOutDegree();
                for (intT j = 0; j < ind; j++)
                {
#ifndef WEIGHTED
                    intT ngh = V[i].getOutNeighbor(j);
                    localEdges[counter] = ngh;
#else
                    intT ngh = V[i].getOutNeighbor(j);
                    intT wgh = V[i].getOutWeight(j);
                    localEdges[counter*2] = ngh;
                    localEdges[counter*2+1] = wgh;
#endif

                    counter++;
                }
                FG.V[i].setOutNeighbors(localEdges);
            }
        }

    }

    delete [] offsets;
    delete [] inOffsets;
    // We know that all vertices with incoming edges are located within this
    // partition, so when iterating over destinations of edges, we can limit the
    // range to:
    //FG.n_dst_start = rangeLow;
    //FG.n_dst_end = rangeHi;
    return FG;
}
//This method is designed for indegree, and out model use now
//all the vertices of graph will store part outdegree (CSR) for push
//local vertices of partitioend graph will store all in-degree (CSC) for pull
// Place edge (u,v) in partition of v
template <class vertex>
graph<vertex> partitioned_graph<vertex>::PartitionByDest(wholeGraph<vertex> &GA, int rangeLow, int rangeHi,int num_part)
{
    vertex *V = GA.V;
    const intT n = GA.n;
    bool isSymmetric = GA.isSymmetric;

    intT nnzi = 0;
    for( intT i=0; i < n; ++i )
    {
       if( V[i].getInDegree() != 0 )
          ++nnzi;
    }

    graph<vertex> FG(n, GA.m, nnzi,num_part,isSymmetric);
    FG.CSCV.Interleave_allocate(FG.CSCVn);
  //  FG.V.Interleave_allocate(n);
      FG.V = GA.V;
//    FG.CSRV.Interleave_allocate(FG.CSRVn);

    intT k=0;
    for( intT i=0; i < n; ++i )
    {
        if( V[i].getInDegree() != 0 )
        {
            FG.CSCV[k].first = i;
            FG.CSCV[k].second = V[i];
            k++;
        }
    }
#if 0
    cerr << "CSCCSRCompressed graph n=" << n 
         << " CSC=" << FG.CSCVn
         << " compressed" << endl;
#endif
    return FG;
}
//EDGELIST IMPLEMENTATION FOR GRAPH PARTITION
template <class vertex>
EdgeList<Edge> partitioned_graph<vertex>::COOPartitionByDest(wholeGraph<vertex> &GA, int rangeLow, int rangeHi,int numanode)
{
    vertex *V = GA.V;
    const intT n = GA.n;
    bool isSymmetric = GA.isSymmetric;
    intT totalSize = 0;
        for (intT i = rangeLow; i < rangeHi; i++)
              totalSize += V[i].getInDegree();
        EdgeList<Edge> el (totalSize,n,numanode);
        long k = 0;
       
        for( intT i=rangeLow; i<rangeHi; i++ )
        {
            for( intT j=0; j < V[i].getInDegree(); ++j )
            {
                intT d = V[i].getInNeighbor( j );
#ifndef WEIGHTED
                el[k++] = Edge( d, i );
#else
                el[k++] = Edge( d, i, V[i].getInWeight( j ) );
#endif
	    }
         }
    assert( k == totalSize );
    //cerr<<"VERTER_CHUNCK: "<<rangeHi-rangeLow<<" and EDGE: "<<totalSize<<endl;
    return el;
}

template <class vertex>
EdgeList<Edge> partitioned_graph<vertex>::COOPartitionBySour(wholeGraph<vertex> &GA, int rangeLow, int rangeHi,int numanode)
{
    vertex *V = GA.V;
    const intT n = GA.n;
    bool isSymmetric = GA.isSymmetric;
    intT totalSize = 0;
        for (intT i = rangeLow; i < rangeHi; i++)
              totalSize += V[i].getOutDegree();

        EdgeList<Edge> el (totalSize,n,numanode);
        long k = 0;
        for( intT i=rangeLow; i<rangeHi; i++ )
        {
            for( intT j=0; j < V[i].getOutDegree(); ++j )
            {
                intT d = V[i].getOutNeighbor( j );
#ifndef WEIGHTED
                el[k++] = Edge( i, d );
#else
                el[k++] = Edge( i, d, V[i].getOutWeight( j ) );
#endif
	    }
        }
    	assert( k == totalSize );
    return el;
}

// ======================================================================
// Graph Partitioning
// ======================================================================
//vertex balancing partition
template <class vertex>
void partitioned_graph<vertex>::partitionByVertex(wholeGraph<vertex> GA, int numOfNode, intT *sizeArr,  bool useRelabel)
{
  if(useRelabel)
  {
    cerr<<"Relabel chunk size..."<<endl;
    const intT n = GA.n;
    for (int i = 0; i < numOfNode; i++)
        sizeArr[i] = 0;
   
    intT counter=0;
    for (intT j=0; j<n; ++j)
    {
        sizeArr[counter]++;
        if (j<n-1 && GA.V[j+1].getInDegree()>GA.V[j].getInDegree() && counter<numOfNode-1)
            counter++;
    } 
   }
   else
   {
     cerr<<"Original chunk size..."<<endl;
     intT chunck=0;
     for ( intT i=0; i < numOfNode-1; ++i)
     {
        sizeArr[i]=GA.n/numOfNode;
        chunck+=sizeArr[i];
     }
     sizeArr[numOfNode-1]=GA.n-chunck;
   }
}
//For wholegraph degree
template <class vertex>
void partitioned_graph<vertex>::cscpartitionByDegree(graph<vertex> GA, int numOfNode, intT *sizeArr,  bool useOutDegree, bool useRelabel)
{
  if (useRelabel)
  {
    const intT n = GA.CSCVn;
    for (int i = 0; i < numOfNode; i++)
        sizeArr[i] = 0;
   
    intT counter=0;
    for (intT j=0; j<n; ++j)
    {
        sizeArr[counter]++;
        if (j<n-1 && GA.CSCV[j+1].second.getInDegree()>GA.CSCV[j].second.getInDegree() && counter<numOfNode-1)
            counter++;
    } 
    assert( counter+1 == numOfNode );
  }
  else
  {
    const intT n = GA.CSCVn;
    intT *degrees = new intT [n];
    if (useOutDegree)
            parallel_for(intT i = 0; i < n; i++) degrees[i] = GA.CSCV[i].second.getOutDegree();
    else
            parallel_for(intT i = 0; i < n; i++) degrees[i] = GA.CSCV[i].second.getInDegree();

    intT edges[numOfNode];
    for (int i = 0; i < numOfNode; i++)
    {
        edges[i] = 0;
        sizeArr[i] = 0;
    }
    
    intT averageDegree = GA.m / numOfNode;
    int counter = 0;
    for (intT i = 0; i < n; i++)
    {
        edges[counter]+=degrees[i];
        sizeArr[counter]++;
        if (edges[counter]<averageDegree && degrees[i+1]+edges[counter]> 1.1*averageDegree)
            counter++;
        if (edges[counter]>=averageDegree && counter <numOfNode-1)
            counter++;
    }
    assert( counter+1 == numOfNode );
    delete [] degrees;
  }
}

template <class vertex>
void partitioned_graph<vertex>::cscpartitionByVertex(graph<vertex> GA, int numOfNode, intT *sizeArr,  bool useRelabel)
{
  if (useRelabel)
  {
    const intT n = GA.CSCVn;
    for (int i = 0; i < numOfNode; i++)
        sizeArr[i] = 0;
   
    intT counter=0;
    for (intT j=0; j<n; ++j)
    {
        sizeArr[counter]++;
        if (j<n-1 && GA.CSCV[j+1].second.getInDegree()>GA.CSCV[j].second.getInDegree() && counter<numOfNode-1)
            counter++;
    } 
  }
   else
   {
    intT chunck=0;
    for ( intT i=0; i < numOfNode-1; ++i)
    {
        sizeArr[i]=GA.CSCVn/numOfNode;
        chunck+=sizeArr[i];
    }
    sizeArr[numOfNode-1]=GA.CSCVn-chunck;
   }
}

template <class vertex>
void partitioned_graph<vertex>::partitionByDegree(wholeGraph<vertex> GA, int numOfNode, intT *sizeArr,  bool useOutDegree, bool useRelabel)
{
  if (useRelabel)
  {
    cerr<<"Relabel chunk size..."<<endl;
    const intT n = GA.n;
    for (int i = 0; i < numOfNode; i++)
        sizeArr[i] = 0;
   
    intT counter=0;
    for (intT j=0; j<n; ++j)
    {
        sizeArr[counter]++;
        if (j<n-1 && GA.V[j+1].getInDegree()>GA.V[j].getInDegree() && counter<numOfNode-1)
            counter++;
    } 
    assert( counter+1 == numOfNode );
  }
  else{
    cerr<<"Original chunk size..."<<endl;
    intT* degrees = new intT [n];
    if (useOutDegree)
    {
        {
            parallel_for(intT i = 0; i < n; i++) degrees[i] = GA.V[i].getOutDegree();
        }
    }
    else
    {
        {
            parallel_for(intT i = 0; i < n; i++) degrees[i] = GA.V[i].getInDegree();
        }
    }
    intT * edges= new intT [numOfNode];
    for (int i = 0; i < numOfNode; i++)
    {
        edges[i] = 0;
        sizeArr[i] = 0;
    }
    
    intT averageDegree = GA.m / numOfNode;
    cerr<<"Average Degree: "<<averageDegree<<endl;
    int counter = 0;
    for (intT i = 0; i < n; i++)
    {
        edges[counter]+=degrees[i];
        sizeArr[counter]++;
        if (edges[counter]<averageDegree && degrees[i+1]+edges[counter]> 1.1*averageDegree)
            counter++;
        if (edges[counter]>=averageDegree && counter <numOfNode-1)
            counter++;
    }
#define PSIZE 0
#if PSIZE
     intT a=0,b=0;
    for (intT i=0; i<numOfNode;i++)
    {
        cerr<<" Part " <<i<<" with size "<<sizeArr[i]<<" and edges "<<edges[i]<<endl;
        a+=edges[i];
        b+=sizeArr[i];
    }
    assert(a==GA.m);
    assert(b==GA.n);
    abort();
#endif
    assert( counter+1 == numOfNode );
    delete [] degrees;
    delete [] edges;
   }
}
