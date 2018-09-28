/* sample_scan.cpp                  -*-C++-*-
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
 * @file sample_scan.cpp
 *
 * @brief Sample program illustrating use of scan.
 */

#include <cilkpub/scan.h>

#include <assert.h>
#include <cstdio>
#include <cstdlib>

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>


// Flag checking whether we compile assuming we have C++ lambdas.
// For lambdas, you may need to compile with -std=c++0x on Linux, or
// /Qstd=c++0x on Windows

#if !defined(USE_CPP_LAMBDAS)
#    define USE_CPP_LAMBDAS 0
#endif

// Idempotent function that does dummy work to has a value x.
int dummy_work(int x) {
    for (int j = 0; j < 1000; ++j) {
        x = x*(2*x-1);
    }
    return x;
}

#if !(USE_CPP_LAMBDAS)
/// If we aren't using C++ lambdas, create functors.
struct ArraySumReduceFunctor {
    int* m_a;
    int* m_b;
    ArraySumReduceFunctor(int* a, int* b) : m_a(a), m_b(b) { }

    /**
     * Reduce function for sum.
     * 
     * After a call to reduce for a tile, for all
     * i in [s, s+m), 
     *    b[i] should store dummy_work(a[i])
     *
     * This function should return
     *   \sum_{i=s}^{s+m-1} (dummy_work(a[i]))
     */
    int operator() (size_t s, size_t m) {
        int sum = 0;
        for (size_t i = s; i < s+m; ++i) {
            // Compute work value, store into slot into m_b.
            m_b[i]= dummy_work(m_a[i]);
            sum += m_b[i];
        }
        return sum;
    }
};


// Combine cumulative sums togeter.
// This function could also be the functor std::plus<int>().
int ArraySumCombineFunction(int left, int right) {
    return left + right;
}

struct ArraySumScanFunctor {
    int* m_b;
    ArraySumScanFunctor(int* b) : m_b(b) { }

    /**
     * Scan function for sum.
     *
     * For all i in a tile [s, s+m), this function computes
     *
     *  m_b[i] = \sum_{j = s}^{i-1} (initial_val + m_b[j])
     *
     * initial_val in this case will store the cumulative sum
     * of dummy_work(m_a[j]) over all j in [0, s).
     *
     */ 
    void operator() (size_t s, size_t m, int initial_val) {
        for (size_t i = s; i < s+m; ++i) {
            initial_val += m_b[i];
            m_b[i] = initial_val;
        }
    }
};

#endif // !(USE_CPP_LAMBDAS)



int main(int argc, char* argv[]) {
    int n = 400000;
    int* a = new int[n];
    int* b = new int[n];
    int* b_expected = new int[n];

    int initial_val = 43;

    srand(2);
    int sum = initial_val;

    // For all i, b_expected[i] should store
    // cumulative sum of
    //  \sum_{j=0}^{i} dummy_work(a[j]).
    for (int i = 0; i < n; ++i) {
        a[i] = rand();
        sum += dummy_work(a[i]);
        b_expected[i] = sum;
    }

    std::printf("Sample scan with n = %d... ", n);
    
//    int tilesize = 10000;
    int tilesize = 50;
#if USE_CPP_LAMBDAS
    cilkpub::parallel_scan(n,
                           initial_val,
                           tilesize,
                           [a]( size_t s, size_t m ) -> int {
                               int sum = 0;
                               for (size_t i = s; i < s+m; ++i) {
                                   sum += a[i];
                               }
                               return sum;
                           },
                           std::plus<int>(),
                           [a, b]( size_t s, size_t m, int initial ) {
                               for (size_t i = s; i < s+m; ++i) {
                                   initial += a[i];
                                   b[i] = initial;
                               }
                           });

#else
    ArraySumReduceFunctor reduce(a, b);
    ArraySumScanFunctor scan(b);
    cilkpub::parallel_scan(n,
                           initial_val,
                           tilesize,
                           reduce, 
                           ArraySumCombineFunction,
                           scan);
#endif    

    // Check output. 
    for (int i = 0; i < n; ++i) {
        assert(b[i] == b_expected[i]);
    }

    std::printf("PASSED\n");

    delete[] a;
    delete[] b;
    delete[] b_expected;
    
    return 0;
}

