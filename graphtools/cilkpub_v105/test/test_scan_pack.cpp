/* test_scan_pack.cpp              -*-C++-*-
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
 * @file test_scan_pack.cpp
 *
 * @brief Test pack method in scan module.
 *
 *
 * This test is adapted from the test code for scan implemented for
 * the book <a href = http://parallelbook.com>Structured Parallel
 * Programming</a> by Michael McCool, James Reinders, and Arch
 * Robison.
 *
 */


#include <cilkpub/scan.h> /* Put header to be tested first to improve
			     odds of detecting include errors. */

#define NOMINMAX /* Required on Windows to avoid breaking std::min */

#include <cassert>
#include <cstddef>
#include <cstdio>

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

#include "cilktest_harness.h"
#include "cilktest_timing.h"


// Serial pack method.
template<typename T, typename Pred> 
size_t serial_pack( const T a[], size_t n, T b[], Pred p ) {
    size_t j=0;
    // Hoist the predicate function out of the inner loop.
    // This predicate had better be idempotent for the
    // parallel pack anyway.
    const T* a_local = a;
    T* b_local = b;
    Pred p_local = p;
    for( size_t i=0; i<n; ++i )
        if( p_local(a_local[i]) )
            b_local[j++] = a_local[i];
    return j;
}



// Functor, which returns true if (i%K) == 0.
template <typename IntT>
class KModPred {
private:
    IntT K;
public:
    KModPred(IntT K_) : K(K_) { }
    bool operator() ( IntT i ) {
        return ((i % K) == 0);
    }
};


template <typename IntT>
void test_pack(size_t n, size_t tilesize)  {
    const int K = 3;
    IntT* a = new IntT[n];
    IntT* b = new IntT[n];
    IntT* b_expected = new IntT[n];
    IntT initial_val = 42;

    cilk_for(size_t i = 0; i < n; ++i) {
        a[i] = (i+1) * (2*i+1)* 7;
    }

    KModPred<IntT> pred(K);
    size_t expected_count = 0;
    for (size_t i = 0; i < n; ++i) {
        if (pred(a[i]))
            b_expected[expected_count++] = a[i];
    }
    size_t count = cilkpub::pack(a, n, b, pred);

    TEST_ASSERT_EQ(count, expected_count);
    cilk_for(size_t j = 0; j < count; ++j) {
        TEST_ASSERT_EQ(b[j], b_expected[j]);
    }

    CILKTEST_PRINTF(3,
                    "test_pack(%u, %u) PASSED\n",
                    unsigned(n),
                    unsigned(tilesize));

    delete[] a;
    delete[] b;
    delete[] b_expected;
}

template <typename IntT>
void test_pack_all(void) {
    const int M = 9;
    size_t nVals[M] = {1, 2, 3, 4, 7, 8, 16, 255, 10000};
    for (int j = 0; j < M; ++j) {
        size_t n = nVals[j];
        for (size_t tilesize = 1; tilesize <= 10; ++tilesize) {
            test_pack<IntT>(n, tilesize);
        }
    }
    CILKTEST_PRINTF(2, "test_pack PASSED\n");
}




enum Kind {Serial,Cilk};


// Functor for test predicate.
// TBD: We could use a normal function instead of a functor, 
// but the compiler I was testing with seemed
// to optimize this version better.  (Inlining?)
template <typename T>
class SampleTestPred {
public:
   SampleTestPred() {}
   inline bool operator() ( T x ) {
       return x % 3;
   }
};

template <typename T>
inline bool SampleTestPredFunc( T x ) {
    return x % 3;
}


template<typename T>
void TimePack( Kind kind, size_t N,
               int num_packings,
               int trials) {
    double total_time = 0.0;
    double min_time = 0.0;

    // Figure out which version we are running.
    const char* kind_string;
    {
        switch (kind) {
        case Serial:
            kind_string = "Serial";
            break;
        case Cilk:
            kind_string = "Cilk Plus";
            break;
        default:
            kind_string = "Unknown";
            break;
        }
    }

    CILKTEST_PRINTF(2, "-------------------------------\n");
    CILKTEST_PRINTF(2,
                    "%s time (%s)\n",
                    kind_string,
                    "seconds");
    for (int trial_num = 0; trial_num < trials; ++trial_num) {
        T* a = new T[N];
        T* b = new T[N];

        for( size_t k=0; k<N; ++k )
            a[k] = (k%3)*10*k;
        size_t m;
        double t;

        unsigned long long t0, t1;
        const size_t tilesize = 10000;

        // Predicate functor testing for testing pack.
	SampleTestPred<T> test_pred;

        for( int i=-1; i<num_packings; ++i ) {
            if( i==0 )
                t0 = CILKTEST_GETTICKS();
            switch( kind ) {
            case Serial:
                m = serial_pack( a, N, b, test_pred );
                break;
            case Cilk:
                m = cilkpub::pack<T, tilesize>( a, N, b, test_pred );
                break;
            }
        }
        t1 = CILKTEST_GETTICKS();
        t = (t1 - t0) * 1.0e-3;


        CILKTEST_PRINTF(2, "%10s : %g\n", kind_string, t);

        // Update average and min time statistics.
        total_time += t;
        if (trial_num == 0) {
            min_time = t;
        }
        else {
            if (t < min_time) {
                min_time = t;
            }
        }

        size_t j=0;
        for( size_t k=0; k<N; ++k )
            if( test_pred(a[k]) ) {
                assert( b[j]==a[k] );
                ++j;
            }

        delete[] a;
        delete[] b;
    }

    double average_time = total_time / trials;
    CILKTEST_PRINTF(2, "%s Average time: %g seconds\n",
                    kind_string,
                    average_time);
    CILKTEST_PRINTF(2,
                    "%s Minimum time: %g seconds\n",
                    kind_string,
                    min_time);
    CILKTEST_PRINTF(2, "-------------------------------\n");


    // Report average time.
    {
        int P = __cilkrts_get_nworkers();
        char input_string[100];
#ifdef _WIN32
	_snprintf_s(input_string, 100, 100,
		    "N=%llu, Packings=%d",
		    (unsigned long long)N,
		    num_packings);
#else
        snprintf(input_string, 100,
                 "N=%llu, Packings=%d",
                 (unsigned long long)N,
                 num_packings);
#endif
        CILKPUB_PERF_REPORT_TIME(stdout,
                                 kind_string,
                                 P,
                                 average_time,
                                 input_string,
                                 NULL);
    }
}


int main(int argc, char* argv[]) {
    CILKTEST_PARSE_TEST_ARGS(argc, argv);
    CILKTEST_BEGIN("scan_pack");

    // Regression tests if we aren't testing for performance.
    if (!CILKTEST_PERF_RUN()) {
        test_pack_all<int>();
        test_pack_all<size_t>();
        test_pack_all<char>();
        test_pack_all<long long>();
    }

    // Time and verify the pack algorithms

    size_t N = 20000000;
    int num_packings = 25;
    int trials = 10;
    CILKTEST_PRINTF(2, "# Timing %d packings of %u %s\n",
                    num_packings,
                    unsigned(N), 
                    "int");

    // Run serial version only if we aren't checking for performance.
    if (!CILKTEST_PERF_RUN()) {
        TimePack<int>(Serial, N, num_packings, trials);
    }
    TimePack<int>(Cilk, N, num_packings, trials);

    return CILKTEST_END("scan_pack");
}
