// -*- C++ -*-
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include "parallel.h"
#include <assert.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <cstring>
#include <utility>
#include <algorithm>
#include <cilk/cilk.h>

#if NUMA
#include <numa.h>
#include <numaif.h>
static int num_numa_node=numa_num_configured_nodes();
// Copied from Cilk include/internal/abi.h:
typedef uint64_t cilk64_t;
typedef void (*__cilk_abi_f64_t)(void *data, cilk64_t low, cilk64_t high);

extern "C" {
    void __cilkrts_cilk_for_numa_64(__cilk_abi_f64_t body, void *data,
				    cilk64_t count, int grain);
}

#else
static int num_numa_node = 1;
#endif // NUMA

#ifndef PARTITION_RANGE
#define PARTITION_RANGE 1
#endif
//This is the function for hugepage mmap allocation
//Based on the NUMA Awareness
#if PARTITION_RANGE
class partitioner
{
    intT num_partitions;
    intT * partition;
#if CPU_PARTITION
    intT * vstarts;
#endif
    intT * starts;
    int num_per_node;
public:
    // Deep copy semantics: every copy gets a new array
    partitioner() : num_partitions( 0 ), partition( 0 ), starts( 0 ), 
#if CPU_PARTITION
vstarts ( 0 ), 
#endif
num_per_node(0) { }
    partitioner( intT n, intT e ) : num_partitions( n )
    {
        partition = new intT [num_partitions+1];
        starts = new intT [num_partitions+1];
#if CPU_PARTITION
        vstarts = new intT [num_partitions+1];
#endif
        partition[num_partitions] = e;
        num_per_node = num_partitions/num_numa_node;
    }
    partitioner( const partitioner & p ) : num_partitions( p.num_partitions )
    {
        partition = new intT [num_partitions+1];
        starts = new intT [num_partitions+1];
        std::copy( &p.partition[0], &p.partition[num_partitions+1], partition );
        std::copy( &p.starts[0], &p.starts[num_partitions+1], starts );
#if CPU_PARTITION
        vstarts = new intT [num_partitions+1];
        std::copy( &p.vstarts[0], &p.vstarts[num_partitions+1], vstarts );
#endif
        num_per_node = num_partitions/num_numa_node;
    }
    const partitioner & operator = ( const partitioner & p )
    {
        if( partition )
            delete [] partition;
        if( starts )
            delete [] starts;
#if CPU_PARTITION
        if( vstarts )
            delete [] vstarts;
#endif
        num_partitions = p.num_partitions;
        num_per_node = p.num_partitions/num_numa_node;
        partition = new intT [num_partitions+1];
        starts = new intT [num_partitions+1];
        std::copy( &p.partition[0], &p.partition[num_partitions+1], partition );
        std::copy( &p.starts[0], &p.starts[num_partitions+1], starts );
#if CPU_PARTITION
        vstarts = new intT [num_partitions+1];
        std::copy( &p.vstarts[0], &p.vstarts[num_partitions+1], vstarts );
#endif
        return *this;
    }
    ~partitioner()
    {

        if( partition )
            delete [] partition;
        if( starts )
            delete [] starts;
#if CPU_PARTITION
        if( vstarts )
            delete [] vstarts;
#endif
    }
    // For easy interfacing with partitionByDegree()
    intT * as_array()
    {
        return partition;
    }
    int get_num_per_node_partitions() const
    {
        return num_per_node;
    }

    int get_num_partitions() const
    {
        return num_partitions;
    }
    intT get_num_elements() const
    {
        return partition[num_partitions];
    }
    intT set_num_elements(intT i)
    {
        return partition[num_partitions]=i;
    }

    // Translate vertex id to partition
    intT partition_of( intT vertex_id ) const
    {
        intT n = 0;
        for( intT p=0; p < num_partitions; ++p )
        {
            n += partition[p];
            if( vertex_id < n )
                return p;
        }
        abort(); // should not occur unless vertex_id is out of range
    }

    //Get the size of each partition
    intT get_size(intT i) const
    {
        return partition[i];
    }

    //get the start number of each partition
    void compute_starts()
    {
        intT startID=0;
        for( intT i=0; i <= num_partitions; i++ )
        {
            starts[i] = startID;
            startID += partition[i];
        }
    }
    intT start_of(intT i) const
    {
        return starts[i];
    }
#if CPU_PARTITION
    intT vertexstart_of(intT i) const
    {
        return vstarts[i];
    }
    void compute_vertexstarts()
    {
        intT startID=0;
        for( intT i=0; i < num_partitions; i++ )
        {
            vstarts[i] = startID;
            startID += partition[num_partitions]/num_partitions;
        }
        vstarts[num_partitions] = partition[num_partitions];
    }
#endif
    // Get offset of vertex id within its partition
    intT offset_of( intT vertex_id ) const
    {
        intT n = 0;
        for( intT p=0; p < num_partitions; ++p )
        {
            n += partition[p];
            if( vertex_id < n )
                return vertex_id - (n-partition[p]);
        }
        abort(); // should not occur unless vertex_id is out of range
    }

    /* Fancy C++ style iterator
        typedef intT * iterator;
        iterator begin() const { return &partition[0]; }
        iterator end() const { return &partition;}
    */
};
#else
class partitioner
{
    intT num_partitions, elements;
    short * partition;
    intT * size;

public:
    // Deep copy semantics: every copy gets a new array
    partitioner() : num_partitions( 0 ), partition( 0 ), size( 0 ),
        elements( 0 ) { }
    partitioner( intT n, intT e ) : num_partitions( n ), elements( e )
    {
        partition = new short [e];
        size = new intT [num_partitions+1];
    }
    partitioner( const partitioner & p ) : num_partitions( p.num_partitions ),
        elements( p.elements )
    {
        partition = new short [elements];
        size = new intT [num_partitions+1];
        std::copy( &p.partition[0], &p.partition[elements], partition );
        std::copy( &p.size[0], &p.size[num_partitions+1], size );
    }
    const partitioner & operator = ( const partitioner & p )
    {
        if( partition )
            delete [] partition;
        if( starts )
            delete [] starts;
        num_partitions = p.num_partitions;
        elements = p.elements;
        partition = new short [elements];
        size = new intT [num_partitions+1];
        std::copy( &p.partition[0], &p.partition[elements], partition );
        std::copy( &p.size[0], &p.size[num_partitions+1], size );
        return *this;
    }
    ~partitioner()
    {

        if( partition )
            delete [] partition;
        if( starts )
            delete [] starts;
    }
    short *as_array()
    {
        return partition;
    }
    int get_num_partitions() const
    {
        return num_partitions;
    }
    intT get_num_elements() const
    {
        return elements;
    }
    intT set_num_elements(intT i)
    {
        return elements = i;
    }
    void set_size(intT p, intT s)
    {
        size[p] = s;
    }

    // Translate vertex id to partition
    intT partition_of( intT vertex_id ) const
    {
        return partition[vertex_id];
    }

    //Get the size of each partition
    intT get_size(intT i) const
    {
        return size[i];
    }
};
#endif

struct IsPart
{
    partitioner & part;
    short p;

    IsPart( partitioner & part_, short p_ ) : part( part_ ), p( p_ ) { }
    intT operator() ( intT i )
    {
        return part.partition_of(i)==p ? 1 : 0;
    }
};


template<typename Fn>
struct PartitionOp {
    const partitioner & part;
    Fn data;

    PartitionOp( const partitioner & part_, Fn data_ )
	: part( part_ ), data( data_ ) { }

    static void func(void *data, uint64_t low, uint64_t high) {
	PartitionOp<Fn> * datap = reinterpret_cast<PartitionOp<Fn> *>( data );
	intT perNode = datap->part.get_num_per_node_partitions();
	parallel_for( uint64_t n=low*perNode; n < high*perNode; ++n )
	    datap->data( n );
    }
};

template<typename Fn>
struct VertexOp {
    const partitioner & part;
    Fn data;

    VertexOp( const partitioner & part_, Fn data_ )
	: part( part_ ), data( data_ ) { }

    static void func(void *data, uint64_t low, uint64_t high) {
	VertexOp<Fn> * datap = reinterpret_cast<VertexOp<Fn> *>( data );
	intT perNode = datap->part.get_num_per_node_partitions();
	intT ps = datap->part.start_of( low * perNode );
	intT pe = datap->part.start_of( high * perNode );
#if defined(CILK)
	_Pragma( STRINGIFY(cilk grainsize = _SCAN_BSIZE) ) parallel_for(
	    intT v=ps; v < pe; ++v )
	    datap->data( v );
#else
	parallel_for( intT v=ps; v < pe; ++v )
	    datap->data( v );
#endif
    }
};


#if NUMA
template<typename Fn>
void map_partitionL( const partitioner & part, Fn fn ) {
    PartitionOp<Fn> op( part, fn );
    __cilkrts_cilk_for_numa_64( &PartitionOp<Fn>::func,
				reinterpret_cast<void *>( &op ),
				num_numa_node, 1 );
    
}
#define map_partition(vname,part,code)					\
    {									\
	map_partitionL( part, [&]( int vname ) { code } );		\
    }
#else // not NUMA
template<typename Fn>
void map_partitionL( const partitioner & part, Fn fn ) {
    intT _np = part.get_num_partitions();
    parallel_for( intT vname=0; vname < _np; ++vname ) {
	fn( vname );
    }
}
#define map_partition(vname,part,code)					\
    {									\
	intT _np = (part).get_num_partitions();				\
	parallel_for( intT vname=0; vname < _np; ++vname ) {		\
	    code;							\
	}								\
    }
#endif // NUMA

#if NUMA
template<typename Fn>
void map_vertexL( const partitioner & part, Fn fn ) {
    VertexOp<Fn> op( part, fn );
    __cilkrts_cilk_for_numa_64( &VertexOp<Fn>::func,
				reinterpret_cast<void *>( &op ),
				num_numa_node, 1 );
    
}
#else
template<typename Fn>
void map_vertexL( const partitioner & part, Fn fn ) {
#if defined(CILK)
    _Pragma( STRINGIFY(cilk grainsize = _SCAN_BSIZE) ) parallel_for(
	intT v=0; v < part.get_num_elements(); ++v )
	fn( v );
#else
    parallel_for( intT v=0; v < part.get_num_elements(); ++v )
	fn( v );
#endif
}
#endif

#define map_vertex(vname,part,code) 	 	 	 	 	\
    do { 	 	 	 			 	 	\
	map_partition(_p,part, {					\
		intT _s = part.start_of(_p);				\
		intT _e = part.start_of(_p+1);				\
		for( intT _v=_s; _v < _e; ++_v ) {			\
		    intT vname = _v;					\
		    code;						\
		}							\
	    } );							\
    } while( 0 )



