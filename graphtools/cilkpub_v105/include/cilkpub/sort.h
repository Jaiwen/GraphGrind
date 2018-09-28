/* sort.h                  -*-C++-*-
 *
 *************************************************************************
 *
 * Copyright (C) 2012 Intel Corporation
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
 * @file sort.h
 *
 * @brief Routines for sorting arrays in Cilk.
 *
 * \addtogroup Sort Sort
 *
 * @author Arch Robison and Jim Sukha 
 * @version 1.03
 *
 * The @c sort module contains basic routines for sorting arrays
 * in parallel.
 *
 * The @c cilk_sort_in_place() routine uses a simple in-place parallel
 * quicksort algorithm.
 *
 * The @c cilk_sort() routine uses a samplesort algorithm.  The top
 * level of this algorthm partitions the data into a small number of
 * bins, and moves the data so that bins are stored contiguously.
 * Then, the routine uses quicksort to sort each bin.
 *
 * @warning A @c cilk_sort() on items of type @c T constructs
 * temporary copies of the items.  If the sort throws an exception,
 * the copies will be deallocated without their destructors being
 * called.
 *
 * The sorts in this module are adapted from the sample sort code
 * described in the book <a href = http://parallelbook.com>Structured
 * Parallel Programming</a> by Michael McCool, James Reinders, and
 * Arch Robison.
 *
 * @warning These sort routines have only been tested on sorting
 * arrays-like containers.  In principle, these sorts should be
 * applicable to any data structures with random-access iterators, but
 * this code has not been tested on them. Moreover, sorts may not be
 * efficient if random-access requires more than O(1) time.
 *
 *   @todo Performance tests for the @c sort module. 
 *   @todo Test @c sort on generic data structures.
 */

#ifndef __CILKPUB_SORT_H_
#define __CILKPUB_SORT_H_

#include <assert.h>
#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <cilk/cilk.h>

namespace cilkpub {

    namespace internal {
        // Tell Doxygen to ignore undocumented functions here.
        //@cond internal

        /// Cutoff for serial base case in parallel quicksort.
        const int QUICKSORT_SERIAL_CUTOFF = 500;

        /// Cutoff where sample sort uses quicksort instead.
        const int SAMPLESORT_QSORT_CUTOFF = 2000;

        /// Lower bound on bin size for samplesort
        const size_t SAMPLESORT_BIN_CUTOFF = 1024;

        // Compare for less than specified key (Commute=true) or greater than specified key (Commute=false).
        template <typename RandomAccessIterator, typename Compare, bool Commute>
        class pivot_comparator {
        public:
            pivot_comparator(Compare comp, RandomAccessIterator key)
                : m_comp(comp)
                , m_key(key)
            {
            }

            bool operator() (const typename std::iterator_traits<RandomAccessIterator>::value_type& x) {
                return Commute ? m_comp(x, *m_key) : m_comp(*m_key, x);
            }
        private:
            Compare m_comp;
            RandomAccessIterator m_key;
        };
    

        /**
         * @brief Choose median of three keys.
         */
        template <typename RandomAccessIterator, typename Compare>
        RandomAccessIterator median_of_three(RandomAccessIterator x,
                                             RandomAccessIterator y,
                                             RandomAccessIterator z,
                                             Compare comp) {
            return comp(*x, *y)
                ? (comp(*y, *z) ? y : (comp(*x, *z) ? z : x))
                : (comp(*z, *y) ? y : (comp(*z, *x) ? z : x));
        }


        /**
         * @brief Choose a partition key as median of medians.
         */
        template <typename RandomAccessIterator, typename Compare>
        RandomAccessIterator choose_partition_key(RandomAccessIterator first,
                                                  RandomAccessIterator last,
                                                  Compare comp) {
            typename std::iterator_traits<RandomAccessIterator>::difference_type offset;
            offset = (last-first)/8;
            return median_of_three(
                median_of_three(first,
                                first+offset,
                                first+offset*2,
                                comp),
                median_of_three(first+offset*3,
                                first+offset*4,
                                last-(3*offset+1),
                                comp), 
                median_of_three(last-(2*offset+1),
                                last-(offset+1),
                                last-1,
                                comp),
                comp);
        }


        /**
         * Choose a key and partitions [first...last) with this key.
         * Returns pointer to where the partition key is in partitioned sequence.
         *
         * @post All elements in range [first, middle) are < key.
         * @post All elements in range [middle, last) are >= key.
         */
        template <typename RandomAccessIterator, typename Compare>
        RandomAccessIterator divide(RandomAccessIterator first,
                                    RandomAccessIterator last,
                                    Compare comp) {
            // Move partition key to front.
            std::swap( *first, *choose_partition_key(first, last, comp) );

            RandomAccessIterator key = first;


            RandomAccessIterator middle =
                std::partition( first+1,
                                last,
                                pivot_comparator<RandomAccessIterator, Compare, true>(comp, key)
                    ) - 1;
        
            if( middle!=first ) {
                // Move partition key to between the partitions
                std::swap( *first, *middle );
            }
            return middle;
        }


        /// Return true if all elements in [first+1, last) are >= *first.
        template <typename RandomAccessIterator, typename Compare>
        bool all_less_than_equal_to_first(RandomAccessIterator first,
                                          RandomAccessIterator last,
                                          Compare comp)
        {
            // Search all elements in [first+1, last) for elements x that
            // have x > key.  If we find none, then, we know x <= key.
            RandomAccessIterator key = first;
            return (last ==
                    std::find_if( first+1,
                                  last,
                                  pivot_comparator<RandomAccessIterator, Compare, false>(comp, key)
                        ));
        }


        template <typename RandomAccessIterator, typename Compare>
        void parallel_quicksort(RandomAccessIterator first,
                                RandomAccessIterator last,
                                Compare comp) {
            while( (last-first) > QUICKSORT_SERIAL_CUTOFF ) {

                // Divide and partition the array.
                RandomAccessIterator middle = divide(first, last, comp);

                // If the pivot element is at the beginning, do a special
                // check for all the elements being equal.
                if (first == middle) {
                    // Because of the divide, we know all elements in
                    // [first, last) are >= first.
                    // 
                    // If all elements in [first, last) are <= first, then
                    // we are already sorted.
                    if (all_less_than_equal_to_first(first, last, comp))
                        return;
                }
            
                // Now have two subproblems: [first..middle) and (middle..last)
                if( middle-first < last-(middle+1) )  {
                    // Left problem [first..middle) is smaller, so spawn it.
                    cilk_spawn parallel_quicksort(first, middle, comp);
                    // Solve right subproblem in next iteration.
                    first = middle+1;
                } else {
                    // Right problem (middle..last) is smaller, so spawn it.
                    cilk_spawn parallel_quicksort(middle+1, last, comp);
                    // Solve left subproblem in next iteration.
                    last = middle;
                }
            }
            // Base case
            std::sort(first, last, comp);
        }


        /// Max number of bins for sample sort.  Must not exceed 256.
        const size_t MAX_BINS = 32;
        
        /// Index type for bins.
        typedef unsigned char bindex_type;

        inline size_t floor_lg2( size_t n ) {
            size_t k = 0;
            for( ; n>1; n>>=1 )
                ++k;
            return k;
        }

        inline size_t choose_number_of_bins( size_t n ) {
            return std::min( MAX_BINS,
                             size_t(1)<<floor_lg2(n/SAMPLESORT_BIN_CUTOFF) );
        }


        // Assumes that m is a power of 2
        template <typename RandomAccessIterator,
                  typename ValueType,
                  typename Compare >
        void build_sample_tree( RandomAccessIterator xs,
                                RandomAccessIterator xe,
                                ValueType tree[],
                                size_t m,
                                Compare comp )
        {
            // Compute oversampling coefficient o as approximately log(xe-xs)
            assert(m <= MAX_BINS);
            size_t o = floor_lg2(xe-xs);

            const size_t O_MAX = 8*(sizeof(size_t));
            size_t n_sample = o*m-1;
            ValueType tmp[ O_MAX * MAX_BINS-1 ];

            size_t r = (xe-xs-1)/(n_sample-1);
            // Generate oversampling
            for( size_t i=0; i<n_sample; ++i )
                tmp[i] = xs[i*r];

            // Sort the samples
            std::sort( tmp, tmp+n_sample, comp );

            // Select samples and put them into the tree
            size_t step = n_sample+1;
            for( size_t level=1; level<m; level*=2 ) {
                for( size_t k=0; k<level; ++k )
                    tree[level-1+k] = tmp[step/2-1+k*step];
                step /= 2;
            }
        }


        // Set bindex[0..n) to the bin index of each key in x[0..n),
        // using the given implicit binary tree with m-1 entries.
        template <typename RandomAccessIterator,
                  typename ValueType,
                  typename Compare >
        void map_keys_to_bins( const RandomAccessIterator x, size_t n,
                               const ValueType tree[], size_t m,
                               bindex_type bindex[],
                               size_t freq[],
                               Compare comp ) {
            size_t d = floor_lg2(m);
            // Work-around for Clang's lack of array notation.
#if CILKPLUS_CLANG
            for (size_t j = 0; j < m; ++j)
                freq[j] = 0;
#else
            freq[0u:m] = 0;
#endif
            for( size_t i=0; i<n; ++i ) {
                size_t k = 0;
                for( size_t j=0; j<d; ++j )
                    k = 2*k+2 - comp(x[i], tree[k]);
                ++freq[bindex[i] = k-(m-1)];
            }
        }
        
        /**
          * Move-construct src from dst.
          */        
        template<typename T>
        inline void move_construct( T& dst, T& src ) {
#if CILKPUB_HAVE_STD_MOVE
            new(&dst) T(std::move(src));
#else
            new(&dst) T(src);
#endif
        }

        /**
          * Move src to dst and destroy src.
          */
        template<typename T>
        inline void move_destroy( T& dst, T& src ) {
#if CILKPUB_HAVE_STD_MOVE
            dst = std::move(src);
#else
            dst = src; 
#endif
            src.~T();
        }

        /**
         * Moves data from the original container into a temporary
         * array, partitioned into bins.
         *
         * @note @c ValueType should be @c std::iterator_traits<RandomAccessIterator>::value_type.
         *
         * @post @c y contains the elements from the original arrary,
         * partitioned into bins.  @c tally is the count array
         * summarizing the data.
         *  
         * @param xs     Iterator indicating start in the original container.
         * @param xe     Iterator indicating end in the original container.
         * @param m      Number of bins 
         * @param y      Temporary array to store partitioned data.
         * @param tally  Count array summarizing the data.
         * @param comp   Comparator for sorting.
         */
        template <typename RandomAccessIterator,
                  typename ValueType,
                  typename Compare >
        void bin( RandomAccessIterator xs,
                  RandomAccessIterator xe,
                  size_t m,
                  ValueType* y,
                  size_t tally[MAX_BINS][MAX_BINS],
                  Compare comp,
                  bindex_type bindex[])
        {
            ValueType tree[MAX_BINS - 1];
            build_sample_tree( xs, xe, tree, m, comp );
            
            size_t block_size = ((xe-xs)+m-1)/m;
        
            cilk_for( size_t i=0; i<m; ++i ) {
                // Compute bounds [js,je) on a block
                size_t js = i*block_size;
                size_t je = std::min( js+block_size, size_t(xe-xs) );

                // Map keys in a block to bins
                size_t freq[MAX_BINS];
                map_keys_to_bins( xs+js, je-js, tree, m, bindex+js, freq, comp );

                // Compute where each bucket starts
                ValueType* dst[MAX_BINS];
                size_t s = 0;
                for( size_t j=0; j<m; ++j ) {
                    dst[j] = y+js+s;
                    s += freq[j];
                    tally[i][j] = s;
                }

                // Scatter keys into their respective buckets
                for( size_t j=js; j<je; ++j ) {
                    move_construct(*dst[bindex[j]]++, xs[j]);
                }
            }
        }

        /**
         * Moves data from bins in temporary array @c y back into the
         * original container and sorts the results.
         *
         * @note @c ValueType should be @c std::iterator_traits<RandomAccessIterator>::value_type.
         *
         * @pre @c y should contain the elements from the original
         *   arrary, partitioned into bins.  @c tally is the count
         *   array summarizing the data. 
         *
         * @param xs     Iterator indicating start in the original container.
         * @param xe     Iterator indicating end in the original container.
         * @param m      Number of bins 
         * @param y      Temporary array containing partitioned data.
         * @param tally  Count array summarizing the data.
         * @param comp   Comparator for sorting.
         */
        template <typename RandomAccessIterator,
                  typename ValueType,
                  typename Compare >
        void repack_and_subsort( RandomAccessIterator xs,
                                 RandomAccessIterator xe,
                                 size_t m,
                                 ValueType* y,
                                 const size_t tally[MAX_BINS][MAX_BINS],
                                 Compare comp ) {

            // Compute column sums of tally, forming the running sum of bin sizes.
            size_t col_sum[MAX_BINS];

            // Working around a bug to get this code to compile using
            // Cilk Plus GCC.  Hopefully this bug will be fixed soon...
            // Also, work around CLANG's lack of array notation.
#if CILKPLUS_GCC || CILKPLUS_CLANG 
            for (size_t q = 0; q < m; ++q) {
                col_sum[q] = 0;
                for ( size_t i = 0; i < m; ++i ) {
                    col_sum[q] += tally[i][q];
                }
            }
#else
            col_sum[0u:m] = 0;
            for( size_t i=0; i<m; ++i )
                col_sum[0u:m] += tally[i][0u:m];
#endif
            assert( col_sum[m-1]==xe-xs );
            
            // Copy buckets into their bins and do the subsorts
            size_t block_size = ((xe-xs)+m-1)/m;
            cilk_for( size_t j=0; j<m; ++j ) {
                RandomAccessIterator x_bin = xs + (j==0 ? 0 : col_sum[j-1]);
                RandomAccessIterator x = x_bin;
                for( size_t i=0; i<m; ++i ) {
                    size_t js = j==0 ? 0 : tally[i][j-1];
                    size_t n = tally[i][j]-js;
                    for( size_t k=0; k<n; ++k )
                        move_destroy( x[k], y[i*block_size+js+k] );
                    x += n;
                }
                parallel_quicksort(x_bin, x, comp);
            }
        }

        /**
         * Holds a buffer of raw memory and automatically deletes it.
         */
        template<typename T>
        class raw_buffer {
            std::pair<T*,ptrdiff_t> m_buf;
        public:
            raw_buffer( ptrdiff_t n ) {
                m_buf = std::get_temporary_buffer<T>(n);
            }
            ~raw_buffer() {
                std::return_temporary_buffer(m_buf.first);
            }
            T* get() const {return m_buf.first;}
            ptrdiff_t size() const {return m_buf.second;}
        };

        template <typename RandomAccessIterator, typename Compare>
        void parallel_samplesort( RandomAccessIterator xs,
            RandomAccessIterator xe,
            Compare comp )
        {
            ptrdiff_t n = xe-xs;
            if( n > SAMPLESORT_QSORT_CUTOFF ) {
                typedef typename std::iterator_traits<RandomAccessIterator>::value_type T;
                // Allocate temporary space before commencing, so that we are sure it is available.
                raw_buffer<T> y(n); 
                if( y.size()>=n ) {
                    raw_buffer<bindex_type> bindex(n);
                    if( bindex.size()>=n ) {
                        size_t m = choose_number_of_bins(xe-xs);
                        size_t tally[MAX_BINS][MAX_BINS];
                        bin(xs, xe, m, y.get(), tally, comp, bindex.get() );
                        repack_and_subsort(xs, xe, m, y.get(), tally, comp);
                        return;
                    }
                }
            }
            // Fall back on quicksort when sort is too short for samplesort to pay off, 
            // or there is not enough temporary space available.
            parallel_quicksort(xs, xe, comp);
        }


        // End of area for Doxygen to ignore.
        //@endcond
    }; // namespace internal

    /**
     * @brief Sorts the data in [@c begin, @c end) using the given comparator.
     *
     * The compare function object is used for all comparisons between
     * elements during sorting.
     *
     * @pre @c comp(*i,*j) must return true if *i should precede *j, false otherwise.
     */
    template <typename RandomAccessIterator, typename Compare>
    inline void cilk_sort(RandomAccessIterator begin,
                          RandomAccessIterator end,
                          Compare comp)
    {
        internal::parallel_samplesort(begin, end, comp);
    }

    /**
     * @brief Sorts the data in [ @c begin, @c end) with a default
     * comparator @c std::less<RandomAccessIterator>.
     */
    template<typename RandomAccessIterator>
    inline void cilk_sort(RandomAccessIterator begin,
                          RandomAccessIterator end) {
        internal::parallel_samplesort( 
            begin,
            end,
            std::less< typename std::iterator_traits<RandomAccessIterator>::value_type >() );
    }

    /**
     * @brief In-place sort of the data in [@c begin, @c end) using
     * the given comparator.
     *
     * The compare function object is used for all comparisons between
     * elements during sorting.
     *
     * @pre @c comp must define a bool operator() function.
     */
    template <typename RandomAccessIterator, typename Compare>
    inline void cilk_sort_in_place(RandomAccessIterator begin,
                                   RandomAccessIterator end,
                                   Compare comp)
    {
        internal::parallel_quicksort(begin, end, comp);
    }

    /**
     * @brief In-place sort of the data in [ @c begin, @c end) with a
     * default comparator @c std::less<RandomAccessIterator>.
     */
    template<typename RandomAccessIterator>
    inline void cilk_sort_in_place(RandomAccessIterator begin,
                                   RandomAccessIterator end) {
        internal::parallel_quicksort( 
            begin,
            end,
            std::less< typename std::iterator_traits<RandomAccessIterator>::value_type >() );
    }

    /**
     * @brief Returns true if the range [@c begin , @c end) is sorted
     * according to comparator @c comp.
     */ 
    template <typename RandomAccessIterator, typename Compare>
    bool cilk_is_sorted(RandomAccessIterator first,
                        RandomAccessIterator last,
                        Compare comp)
    {
        typedef typename std::iterator_traits<RandomAccessIterator>::difference_type difference_type;
        difference_type n = last - first;
        volatile bool array_in_order = true;

        cilk_for( difference_type i = 0; i < n-1; ++i) {
            if (comp(first[i+1], first[i])) {
                // Race condition, because we have concurrent writes
                // to this variable.  But it is a benign race, 
                // because all paths write the same value.  cilk_for provides
                // the necessary memory fence before reading the value.
                // 
                // TBD: Insert Cilkscreen's fake lock around this
                // variable so that Cilkscreen does not report this
                // access as a race.
                array_in_order = false;
            }
        }
        return array_in_order;
    }

    /**
     * @brief Returns true if the range [@c begin , @c end) is sorted
     * according to <.
     */ 
    template <typename RandomAccessIterator>
    bool cilk_is_sorted(RandomAccessIterator first,
                        RandomAccessIterator last)
    {
        return cilk_is_sorted(first,last,std::less<typename std::iterator_traits<RandomAccessIterator>::value_type>());
    }

}  // namespace cilkpub
    
#endif // !defined(__CILKPUB_SORT_H_)
