// This code is part of the project "Ligra: A Lightweight Graph Processing
// Framework for Shared Memory", presented at Principles and Practice of
// Parallel Programming, 2013.
// Copyright (c) 2013 Julian Shun and Guy Blelloch
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
#define APPCACHE 0
#if APPCACHE
#include "papi_code.h"
#endif
#include "ligra-numa.h"
#include "math.h"
#include <fcntl.h>
int MaxIter=100;
//Optimisation modes for Vertex Map
template <class vertex>
struct PR_F
{
    double* p_curr, *p_next;
    vertex* V;
	double* add_factor;
    static const bool use_cache = true;
    struct cache_t
    {
        double p_next;
		//double error;
		//double acc;
		//intT InDegree;
    };
    PR_F(double* _p_curr, double* _p_next, vertex* _V, double* _add_factor) :
		p_curr(_p_curr), p_next(_p_next), V(_V), add_factor(_add_factor){}
    inline bool update(intT s, intT d)  //update function applies PageRank equation
    {
        p_next[d] += add_factor[s];
        return 1;
    }
    inline bool updateAtomic (intT s, intT d)   //atomic Update
    {
        writeAdd(&p_next[d],add_factor[s]);
        return 1;
    }
    /*=============================================================*/
    /*--------EdgeMap-Cache-based routines , used with CSC --------*/
    /*=============================================================*/
    inline void create_cache(cache_t &cache, intT d)
    {
        cache.p_next = p_next[d];
    }
    inline bool update(cache_t &cache, intT s)
    {
        cache.p_next += add_factor[s];
        return 1;
    }

    inline void commit_cache(cache_t &cache, intT d)
    {
	// Cache is used only in sequential mode
	p_next[d] = cache.p_next;
    }

    inline bool cond (intT d)
    {
        //return cond_true(d);    //does nothing
		return true;    //does nothing
    }
};

/*=======================================================================*/
/*--------------------VERTEX MAP-----------------------------------------*/
/*=======================================================================*/

//vertex map function to update its p value according to PageRank equation
struct PR_Vertex_F
{
    double damping;
    double addedConstant;
    double* p_curr;
    double* p_next;
    PR_Vertex_F(double* _p_curr, double* _p_next, double _damping, intT n) :
        damping(_damping), addedConstant((1-_damping)*(1/(double)n)),
        p_curr(_p_curr), p_next(_p_next) {}
    inline bool operator () (intT i)
    {
        p_next[i] = damping*p_next[i] + addedConstant;
        return 1;
    }
};

//vertex map function to update its p value according to PageRank equation with csum
struct PR_Vertex_Csum
{
    double addedConstant;
    double* p_next;
    PR_Vertex_Csum(double* _p_next, double _addedConstant, intT n) :
        addedConstant(_addedConstant), p_next(_p_next) {}
    inline bool operator () (intT i)
    {
        p_next[i] = p_next[i] + addedConstant;
        return 1;
    }
};

//vertex map function to update its p value according to PageRank equation with csum
struct PR_Vertex_norm
{
    double csum;
    double* p_next;
    PR_Vertex_norm(double* _p_next, double _csum, intT n) :
        csum(_csum), p_next(_p_next) {}
    inline bool operator () (intT i)
    {
        p_next[i] = (double)p_next[i]/csum;
        //p_next[i] = (Ftype)p_next[i]*csum;
        return 1;
    }
};

//resets p
struct PR_Vertex_Reset
{
    double* p_curr;
    PR_Vertex_Reset(double* _p_curr) :
        p_curr(_p_curr) {}
    inline bool operator () (intT i)
    {
        p_curr[i] = 0.0;
        return 1;
    }
};


double seqsum( double *a, intT n) {
    double d = 0.;
    double err = 0.;
   for( intT i=0; i < n; ++i ) {
	//The code below achieves
	d += a[i];
	// but does so with high accuracy
	
	/*double tmp = d;
	double y = (double)a[i] + err;
	d = tmp + y;
	err = tmp - d;
	err += y;*/
    }
    return d;
}

double sum( const partitioner &part, double* a, intT n, int scale) {
	double d = 0.;
	double err = 0.;
	double tmp, y;
        int p= part.get_num_partitions();
        double *psum=new double [p];
        map_partition( k, part, {
	 intT s = part.start_of(k);
	 intT e = part.start_of(k+1);
	 psum[k] = seqsum( &a[s], e-s);
       } );
       for( int i=0; i < p; ++i ) {
        /* tmp = d;
          y = psum[i] + err;
          d = tmp + y;
          err = tmp - d;
          err  += y;*/
          d += psum[i];
      }
      delete [] psum;
      if(scale)
         d = (1-d)/n;
      return d;
}


double seqnormdiff( double *a, double *b, intT n) {
    double d = 0.;
    double err = 0.;
    for( intT i=0; i < n; ++i ) {
	//The code below achieves
	// d += fabs(a[i]- b[i]);
	// but does so with high accuracy
	
	 d += fabs(a[i]- b[i]);
       /*double tmp = d;
	double y = fabs( double(a[i]) - double(b[i]) ) + err;
	d = tmp + y;
	err = tmp - d;
	err += y;*/
    }
    return d;
}

double normdiff( const partitioner &part, double* a, double* b, intT n ) {
	double d = 0.;
        int p= part.get_num_partitions();
        double *psum = new double [p];
        double err = 0.;
        double tmp, y;
         /*calculate sum by partitions*/
         map_partition( k, part, {
	 intT s = part.start_of(k);
	 intT e = part.start_of(k+1);
	 psum[k] = seqnormdiff( &a[s], &b[s], e-s);
         } );
       
         for( int i=0; i < p; ++i ) {
	     d += psum[i];
	    /*  tmp = d;
	      y = psum[i] + err;
	      d = tmp + y;
   	      err = tmp - d;
	      err += y;*/
	}
        delete [] psum;
	return d;
}

template <class GraphType>
void Compute(GraphType &GA, long start)
{
    typedef typename GraphType::vertex_type vertex; // Is determined by GraphTyp
    const partitioner &part = GA.get_partitioner();
    graph<vertex> & WG = GA.get_partition();
    const int perNode = part.get_num_per_node_partitions();
    intT n = GA.n;
    intT m = GA.m;
    const double damping = 0.85;
    const double epsilon = 0.0000001;
    //Data Array
    //p_curr and p_next to do special allocation
    //frontier also need special node allocation
    //blocksize equal to the szie of each partitioned
    double one_over_n = 1/(double)n;

    mmap_ptr<double> p_curr;
    p_curr.part_allocate (part);
    mmap_ptr<double> p_next;
    p_next.part_allocate (part);
	//edge_factor and add_factor to pre-calculate contributions in edgemap
    mmap_ptr<double> edge_factor;  
    edge_factor.part_allocate (part);
    mmap_ptr<double> add_factor;
    add_factor.part_allocate (part);

	//initiliaze 
    map_vertexL (part, [&] (intT j) { p_curr[j] = one_over_n; } );
    map_vertexL (part, [&] (intT j) { p_next[j] = 0; } );
    map_vertexL( part, [&] (intT j) { edge_factor[j] = (double)0.85/(WG.V[j].getOutDegree()); } );
#if APPCACHE
        PAPI_initial();         /*PAPI Event inital*/
#endif
    int count=0;
	double L1_norm=2;

    partitioned_vertices Frontier = partitioned_vertices::bits(part,n, m);
    while(count<MaxIter && L1_norm > epsilon)
    {
#if APPCACHE
        PAPI_start_count();   /*start PAPI counters*/
#endif
        /* pre-calculate the contributions to be added in edgemap*/
        map_vertexL( part, [&] (intT j) { add_factor[j] = (double) edge_factor[j]*p_curr[j]; } );

		/*pass on the value of p_curr, p_next and add_factor to edgemap*/
        partitioned_vertices output=edgeMap(GA, Frontier, PR_F<vertex>(p_curr,p_next,WG.V,add_factor),m/20);
#if APPCACHE
        PAPI_stop_count();   /*stop PAPI counters*/
        PAPI_print();   /* PAPI results print*/
#endif
       /*apply vertex map [(1-sum)/n] */
        vertexMap(part,Frontier,PR_Vertex_Csum(p_next,sum(part, p_next,n ,1),n));

	   //compute L1-norm between p_curr and p_next
	   L1_norm= normdiff(part, p_curr, p_next, n);
	   
	   /* normalize pagerank values */
        vertexMap(part,Frontier,PR_Vertex_norm(p_next,sum(part,p_next,n,0),n));

        /* reset pagerank vector*/
        vertexMap(part,Frontier, PR_Vertex_Reset(p_curr));
        count++;
        swap(p_curr,p_next);
        output.del();
        //cerr<<"Iteration: "<<count<<endl;
        //Frontier.del();
        //output.bit = true;
        //Frontier = output;
    }
#if APPCACHE
        PAPI_total_print(1);   /* PAPI results print*/
        PAPI_end();
#endif
    Frontier.del();
    p_curr.del();
    p_next.del();
    edge_factor.del();
    add_factor.del();
}

