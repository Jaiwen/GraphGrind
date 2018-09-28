/* scan.h                  -*-C++-*-
 *
 *************************************************************************
 *
 * Copyright (C) 2014 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *************************************************************************/

/**
 * @file scan.h
 *
 * @brief Routines for a parallel scan in Cilk Plus.
 *
 * \addtogroup Scan Scan
 *
 * @author Arch Robison and Jim Sukha
 * @version 1.05
 *
 * The @c scan module contains routines for a parallel prefix scan.
 *
 * The @c parallel_scan routine performs a parallel scan.
 *
 * The @c pack routine copies all elements from an input array 
 * that satisfy a user-specified predicate into an output array.
 * 
 * This code is adapted from the scan code implemented for the book <a
 * href = http://parallelbook.com>Structured Parallel Programming</a>
 * by Michael McCool, James Reinders, and Arch Robison.
 *
 *   @todo Performance tests for the @c scan module.
 */


#ifndef __CILKPUB_SCAN_H_
#define __CILKPUB_SCAN_H_

#include <assert.h>
#include <cstddef>
#include <functional>

#include <cilk/cilk.h>

namespace cilkpub {

    namespace internal {

        /**
         * @brief Buffer for temporary space of objects of type @c T.
         *
         * For any arrays smaller than @c BASE_STACK_SIZE bytes,
         *
         * This memory will be allocated as a fixed_size array of
         * BASE_STACK_SIZE bytes.
         */
	template<typename T, int BASE_STACK_SIZE=4096>
        class scan_temp_space {
            static const size_t n = BASE_STACK_SIZE/sizeof(T);
	    T temp[n];
	    T* base;
	public:
            /// Get pointer to temporary space.
	    T* data() {return base;}

            /// Indexing operator into temporary space.
	    T& operator[]( size_t k ) {return base[k];}

            /// Construct temporary space of size @c size.
	    scan_temp_space( size_t size ) {
		base = size<=n ? temp : new T[size];
	    }

            /// Destructor
	    ~scan_temp_space() {
		if( base!=temp )
		    delete[] base;
	    }
        };

        /**
         *@brief Return greatest power of 2 less than m
         *
         * @todo We could probably use bit-tricks to get make this
         * operation faster.  But it probably isn't worth it.
         */
        size_t int_log2_split( size_t m ) {
            size_t k=1;
            while( 2*k<m ) k*=2;
            return k;
        }

        /**
         * @brief Upsweep pass, which performs reductions on tiles and
         * combines values together in a binary tree.
         *
         * For example, if we are doing a prefix scan for adding up
         * elements numbers in the range [1, m], with @c i=0, @c m=8
         * and @c tilesize=1, then the upsweep pass conceptually
         * performs the following operations:
         *
         * A. Reduce each tile into a single value.  This operation is
         *    trivial for adding numbers 1 through m.
         *
         *    This produces @c r[0:7] of:
         *    @code
         *       1     2     3     4     5     6     7     8
         *    @endcode
         *
         * B. Combine the results together according to the binary
         *    tree representation.
	 *
         *    @code
         *       1     2     3     4     5     6     7     8
         *        \    |      \    |      \    |      \    |
         *         \   |       \   |       \   |       \   |
         *          ---3        ---7        --11        --15
         *             \           |           \           |
         *              \          |            \          |
         *               ---------10             ---------26
         *                          \                      |
         *                           \                     |
         *                            --------------------36
         *    @endcode
         *
         *    This step produces an output @c r[0:7] of
         *    @code
         *       1     3     3    10     5    11     7    36
         *    @endcode
	 *
	 * Although the described example shows an @c m 
         * which is a power of 2, this method works for any @c m > 0.
         *
         * @param i            Index of initial tile
         * @param m            Total number of tiles to sweep over.
         * @param tilesize     Size of all tiles
         * @param r            Array of size @c m to store output
         * @param lastsize     Size of last tile.
         * @param reduce       Reduce function for a tile.
         * @param combine      Combine function for merging two elements of type T.
         */
        template<typename T, typename R, typename C>
	void upsweep( size_t i,
		      size_t m,
		      size_t tilesize,
		      T r[],
		      size_t lastsize,
		      R reduce,
		      C combine )
	{
	    if( m==1 ) {
  	        r[0] = reduce(i*tilesize, lastsize);
	    } else {
	        // Note: even though int_log2_split is a method that
  	        // takes O(lg m) time, this overhead is amortized to
  	        // O(m) total work (even for tilesize=1).  The
  	        // analysis is similar to that of creating a heap.
  	        // (See MakeHeap in CLRS).
	        size_t k = int_log2_split(m);
	    	cilk_spawn upsweep<T, R, C>(i, k,
                                            tilesize,
                                            r,
                                            tilesize,
                                            reduce,
                                            combine);
	    	upsweep<T, R, C>(i+k, m-k,
                                 tilesize,
                                 r+k,
                                 lastsize,
                                 reduce,
                                 combine);
	    	cilk_sync;
	    	if( m==2*k )
	    	  r[m-1] = combine(r[k-1], r[m-1]);
	    }
	}


        /**
         * @brief Downsweep pass, which computes the final scan over
         * the range.
         *
         * The downsweep pass takes the result of an upsweep pass, and
         * recursively walks down the tree to combine results to
         * generate the final scan output.
         *
         * For example, if we are doing a prefix scan for adding up
         * elements numbers in the range [1, m], with @c i=0, @c m=8
         * and @c tilesize=1, then the downsweep pass conceptually
         * starts with the input @c r[0:7] from the upsweep pass:
         * @code
         *     r[0]  r[1]  r[2]  r[3]  r[4]  r[5]  r[6]  r[7]
         *       1     3     3    10     5    11     7    36
         * @endcode
         *
         * It then walks down the tree to produce the final output.
         *      downsweep(0, 8, 1, r, initial, reduce, combine)
         * recursively executes two tasks in parallel:
         *
         * @code
         *      downsweep(0, 4, 1, r, 1,
         *                initial,
         *                reduce, combine),
         * @endcode  and
         * @code
         *      downsweep(4, 8, 1, r, 1,
         *                combine(initial, r[3]),
         *                reduce, combine)
         * @endcode
         *
         * This produces an output @c r[0:7] of:
         * @code
         *     r[0]  r[1]  r[2]  r[3]  r[4]  r[5]  r[6]  r[7]
         *       1     3     6    10    15    21    28    36
         * @endcode
         *
         * The @c scan function will then use the value at @c r[i] to
         * generate the final output for each tile.
	 *
	 * Although the described example shows an @c m 
         * which is a power of 2, this method works for any @c m > 0.
         *
         * @param i            Index of initial tile
         * @param m            Total number of tiles to sweep over.
         * @param tilesize     Size of all tiles
         * @param r            Array of size @c m to store output
         * @param lastsize     Size of last tile.
         * @param initial      Initial value to use for scan.
         * @param combine      Combine function for merging two elements of type T.
         * @param scan         Scan function for a tile.
         */
        template<typename T, typename C, typename S>
        void downsweep( size_t i,
			size_t m,
			size_t tilesize,
			const T r[],
			size_t lastsize,
			T initial,
			C combine,
			S scan )
	{
	    if( m==1 ) {
		scan(i*tilesize, lastsize, initial);
	    } else {
		size_t k = int_log2_split(m);
		cilk_spawn downsweep<T, C, S>(i, k,
                                              tilesize, r,
                                              tilesize, initial,
                                              combine, scan);
		initial = combine(initial, r[k-1]);
		downsweep<T, C, S>(i+k, m-k,
                                   tilesize, r+k,
                                   lastsize, initial,
                                   combine, scan);
		// implicit cilk_sync;
	    }
	}
    }  // namespace internal


   /**
    * @brief Parallel scan on n elements.
    *
    * This method performs a parallel scan on a range @c[0, n).
    *
    * In the base case, this scan operates on tiles of size at most @c
    * tilesize.
    *
    * The @c reduce function should operate on a single tile
    * representing a contiguous range @code [s, s+m) @endcode.
    * This function should have prototype
    * @code
    *    T reduce(size_t s, size_t m);
    * @endcode
    *
    * The @c combine function merges two elements of type @c T
    * together.   This function should have prototype
    * @code
    *    T combine(T left, T right);
    * @endcode
    *
    * The @c scan function performs a local scan on a single tile
    * representing a contiguous range @code [s, s+m) @endcode,
    * starting with the given initial value.
    * This function should have prototype
    * @code
    *    void scan(size_t s, size_t m, T initial);
    * @endcode
    *
    *
    * For example, consider a simple parallel prefix scan to compute
    * the cumulative sum of an array @c a of length @c n, into an
    * output array @c b, i.e., compute
    *  @c b[i] as \sum_{j=0}^{i} @c a[i].
    * For this operation, we would use the following functions:
    *
    * @code
    *    T reduce(size_t s, size_t m) {
    *       T sum = 0;
    *       for (size_t i = s; i < s + m; ++i) {
    *          sum += a[i];
    *       }
    *       return sum;
    *    }
    *    T combine(T left, T right) {
    *       return left + right;
    *    }
    *    T scan(size_t s, size_t m, T initial_val) {
    *       for (size_t i = s; i < s + m; ++i) {
    *          initial_val += a[i];
    *          b[i] = initial_val;
    *       }
    *    }
    * @endcode
    *
    * This example is an *inclusive scan* since each output @b[i]
    * includes the value @c a[i] in the sum being computed.
    *
    * To perform an *exclusive scan* where the sum @c b[i] excludes
    * the term @c a[i], i.e., @c b[i] = \sum_{j=0}^{i-1} @c a[i], we
    * use a slightly different @c scan function:
    *
    * @code
    *    T scan(size_t s, size_t m, T initial_val) {
    *       for (size_t i = s; i < s + m; ++i) {
    *          b[i] = initial_val;
    *          initial_val += a[i];
    *       }
    *    }
    * @endcode
    *
    * In general, one can convert from an inclusive scan to an
    * exclusive scan by only changing the @c scan function.  The @c
    * reduce or @c combine operations can remain the same.
    *
    * @param n         Number of elements to scan.
    * @param initial   Initial element of type @c T to apply to the scan.
    * @param tilesize  Tile size.
    * @param reduce    Reduce function for a tile.
    * @param combine   Combine function for merging two elements of type T.
    * @param scan      Scan function for a tile.
    */
    template<typename T, typename R, typename C, typename S>
    void parallel_scan( size_t n,
                        T initial,
                        size_t tilesize,
                        R reduce,
                        C combine,
                        S scan )
    {
	if( n>0 ) {
	    size_t m = (n-1)/tilesize;
	    internal::scan_temp_space<T> r(m+1);
	    internal::upsweep<T, R, C>(0, m+1,
                                       tilesize,
                                       r.data(),
                                       n-m*tilesize,
                                       reduce,
                                       combine);
	    internal::downsweep<T, C, S>(0, m+1,
                                         tilesize,
                                         r.data(),
                                         n-m*tilesize,
                                         initial,
                                         combine,
                                         scan);
	}
    }


    /// Implementation of pack, using parallel scan.

    /// Default tilesize to use for pack method.
    const size_t cilkpub_default_pack_tilesize = 10000;

    /**
     * @brief Reduce function for pack, implemented using parallel
     * scan.
     *
     * The reduce function counts up the number of elements the array
     * @c a that satisfies the specified predicate @c Pred.
     */
    template <typename T, typename Pred>
    class pack_reduce_functor {
    private:
        const T* a;
        Pred p;

    public:
        /**
         * @brief Construct the pack reduce function with specified
         * input array and predicate.
         */
        pack_reduce_functor(const T a_[], Pred p_)
            : a(a_)
            , p(p_)
        { }

        /**
         * @brief Pack reduce operation on a tile.
         */
        size_t operator() ( size_t i, size_t m ) {
            size_t sum = 0;
            for ( size_t j=i; j<i+m; ++j )
                if ( p(a[j]) )
                    sum++;
            return sum;
        }
    };

    /**
     * @brief Combine function for pack, implemented using parallel
     * scan.
     *
     * This functor copies all elements from @c a into @c b that
     * satisfy the predicate @c p.
     *
     * @c n should be the size of the array @c a.
     *
     * @c result is a pointer to store the output --- the number of
     * elements placed into the output array @c b.
     */
    template <typename T, typename Pred>
    class pack_scan_functor {
    private:
        const T* m_a;
        T* m_b;
        Pred m_p;
        size_t m_n;
        size_t* m_result;
    public:
        /**
         * @brief Construct the pack scan function.
         *
         * @param a       Input array
         * @param b       Output array
         * @param p       Predicate function to filter output
         * @param n       Size of input array
         * @param result  Pointer to location to store size of output.
         */
        pack_scan_functor(const T a[],
                          T b[],
                          Pred p,
                          size_t n,
                          size_t* result)
            : m_a(a)
            , m_b(b)
            , m_p(p)
            , m_n(n)
            , m_result(result)
        {
        }

        /**
         * @brief Scan function for pack.
         *
         * @param s           Initial index of range to scan
         * @param m           Number of elements in range to scan
         * @param output_idx  Initial index in output array for first output element.
         */
        void operator() ( size_t s, size_t m, T output_idx ) {
  	    // Hoist the variables out of the loop manually.
	    // in case the compiler doesn't do it for us.
  	    // The pack method only works if m_a and m_b don't overlap,
	    // but the compiler may not know that...
	    const T* a = m_a;
	    T* b = m_b;
	    Pred p = m_p;
            for( size_t j=s; j<s+m; ++j )
                if( p(a[j]) )
		  b[output_idx++] = a[j];
            if( s+m==m_n )
                // Save result from last tile
                *m_result = output_idx;
        }
    };

    /**
     * @brief Pack function, which copies all data in @c a that
     * satisfies the predicate @c p into @c b.
     *
     *
     * @c p should be an idempotent predicate function that operates
     *  on an element of type @c T (from @c a).
     *  (It may be invoked more than once).
     * The input array @c a and output array @b should not overlap.
     *
     * @c tilesize  is the size of a tile to use in
     * the base of the scan. 
     *
     * @param a    Input array
     * @param n    Size of input array
     * @param b    Output array
     * @param p    Predicate function
     *
     * @pre @c a should be an input array of size @c n
     * @pre @c b should be an output array of size sufficient to
     *           hold all the elements satisfying @c pred.
     * @pre @c p should be an idempotent predicate.
     */
    template<typename T, size_t tilesize, typename Pred>
    size_t pack( const T a[], size_t n, T b[], Pred p )
    {
	size_t result;
        pack_reduce_functor<T, Pred> reduce_func(a, p);
        pack_scan_functor<T, Pred> scan_func(a, b, p,
                                             n,
                                             &result);
	parallel_scan( n,
                       size_t(0),
		       tilesize,
                       reduce_func,
		       std::plus<T>(),
                       scan_func );
	return result;
    }



    /**
     * @brief Pack function, using a default tilesize.
     *
     *
     * @c p should be an idempotent predicate function that operates
     *  on an element of type @c T (from @c a).
     *  (It may be invoked more than once).
     * The input array @c a and output array @b should not overlap.
     *
     * @param a    Input array
     * @param n    Size of input array
     * @param b    Output array
     * @param p    Predicate function
     *
     * @pre @c a should be an input array of size @c n
     * @pre @c b should be an output array of size sufficient to
     *           hold all the elements satisfying @c pred.
     * @pre @c p should be an idempotent predicate.
     */
    template <typename T, typename Pred>
    size_t pack( const T a[], size_t n, T b[], Pred p )
    {
        return pack<T, cilkpub_default_pack_tilesize, Pred>(a, n, b, p);
    }


} // namespace cilkpub


#endif // __CILKPUB_SCAN_H_
