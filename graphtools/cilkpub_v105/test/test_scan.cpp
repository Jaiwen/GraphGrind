/* test_scan.cpp              -*-C++-*-
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
 * @file test_scan.cpp
 *
 * @brief Simple test method for parallel scan.
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

#include "cilktest_harness.h"
#include "cilktest_timing.h"



const int HASH_REPS = 250;

// A moderately expensive hash function.
int rand_hash(int x) {
    for (int j = 0; j < HASH_REPS; ++j) {
        x = (x+1)*(2*x-1);
    }
    return x;
}


/**
 *@brief Functor for reduce operation in our blocked scan.
 */
template <typename T>
class TestBlockedScanReduceFunctor {
    T* m_in;
    bool* m_touched;

public:
    TestBlockedScanReduceFunctor(T* in,
                                 bool *touched)
        : m_in(in)
        , m_touched(touched)
    { }

    // Reduce function.
    size_t operator() ( size_t i, size_t n ) {
        size_t sum = 0;
        for( size_t j=i; j<i+n; ++j ) {
            sum += m_in[j];
            assert(!m_touched[j]);
            m_touched[j] = true;
        }
        return sum;
    }
};

/**
 *@brief Functor for combine operation in blocked scan.
 */
template <typename T>
class TestBlockedScanScanFunctor {
    T* m_in;
    T* m_out;
public:
    TestBlockedScanScanFunctor(T* in, T* out)
        : m_in(in)
        , m_out(out)
    { }

    // Combine function. 
    void operator() ( size_t i, size_t n, T initial ) {
        for( size_t j=i; j<i+n; ++j ) {
            initial += m_in[j];
            m_out[j] = initial;
        }
    }
};


template<typename Scanner>
void TestBlockedScan() {
    typedef typename Scanner::value_type T;
    static const size_t max_n = 4096;
    T in[max_n];
    T out[max_n];
    bool touched[max_n];

    // Create functors for the scan.  We could also use C++11 and
    // lambdas, but we are avoiding C++ in the test suite.
    TestBlockedScanReduceFunctor<T> reduce(in,
                                           touched);
    TestBlockedScanScanFunctor<T> scan(in, out);
    
    for( size_t n=0; n<max_n; ++n ) {
        for( size_t i=0; i<n; ++i ) {
            in[i] = (i+1)*(i+2)/2;
            out[i] = T(-1);
            touched[i] = false;
        }
        T initial = 1<<n;

        // Do the scan.
        Scanner::template
            parallel_scan( n, initial, 128,
                           reduce,
                           std::plus<T>(),
                           scan );

        // Check the answer.
        T sum=initial;
        for( size_t i=0; i<n; ++i ) {
            sum += in[i];
            assert(sum==out[i]);
            assert(touched[i]);
        }
    }
}



// Alas C++ does not allow "template template function parameters".  
// So the function templates are wrapped in classes.

struct CilkPlusScan {
    typedef int value_type;
    template<typename T, typename R, typename C, typename S>
    static void parallel_scan( size_t n, T initial, size_t tilesize, R reduce, C combine, S scan ) {
	cilkpub::parallel_scan(n,initial,tilesize,reduce,combine,scan);
    }
};


struct ArraySumReduceFunctor {
    int* m_a;
    int* m_b;
    ArraySumReduceFunctor(int* a, int* b) : m_a(a), m_b(b) { }

    // Reduce function for sum. 
    int operator() (size_t s, size_t m) {
        int sum = 0;
        for (size_t i = s; i < s+m; ++i) {
            m_b[i] = rand_hash(m_a[i]);
            sum += m_b[i];
        }
        return sum;
    }
};

// This function could also be the functor std::plus<int>().
int ArraySumCombineFunction(int left, int right) {
    return left + right;
}

struct ArraySumScanFunctor {
    int* m_b;
    ArraySumScanFunctor(int* b) : m_b(b) { }

    // Scan function for sum.
    void operator() (size_t s, size_t m, int initial_val) {
        for (size_t i = s; i < s+m; ++i) {
            initial_val += m_b[i];
            m_b[i] = initial_val;
        }
    }
};



void test_simple_array_sum(size_t n, int tilesize) {
  int* a = new int[n];
  int* b = new int[n];
  int* b_expected = new int[n];
  int initial_val = 42;

  TEST_ASSERT(a);
  TEST_ASSERT(b);
  TEST_ASSERT(b_expected);

  int sum = initial_val;
  // Generate random data and compute expected answer.
  for (size_t i = 0; i < n; ++i) {
    a[i] = (i+1)*(i-1) + 1;
    sum += rand_hash(a[i]);
    b_expected[i] = sum;
  }

  ArraySumReduceFunctor reduce(a, b);
  ArraySumScanFunctor scan(b);

  cilkpub::parallel_scan(n,
			 initial_val,
			 tilesize,
                         reduce, 
                         ArraySumCombineFunction,
                         scan);

  CILKTEST_PRINTF(4, "Scan(%d), tilesize=%d:  final sum: %d\n",
		  n, tilesize,
		  b[n-1]);
  // Check that we computed the expected value.
  cilk_for (size_t i = 0; i < n; ++i) {
    TEST_ASSERT_EQ(b[i], b_expected[i]);
  }

  delete[] a;
  delete[] b;
  delete[] b_expected;
}

// Test parallel prefix scan over a range. 
void test_sum_scan() {

    // Exhaustive scan test, for small n and small tilesize.  Why not?
    // This should hopefully cover most of the corner cases...
    for (int test_n = 0; test_n < 50; test_n++) {
        CILKTEST_PRINTF(2, "Scan test_n=%d: ",  test_n);
        for (int tile_size = 1; tile_size < 75; ++tile_size) {
            test_simple_array_sum(test_n, tile_size);
            CILKTEST_PRINTF(2, ".");
	}
        CILKTEST_PRINTF(2, " PASSED\n");
    }

    // Larger test cases.
    for (int test_n = 100; test_n < 100*1024; test_n*=2) {
        CILKTEST_PRINTF(2, "Scan test_n=%d: ",  test_n);
        for (int z = 0; z < 5; ++z) {
            for (int tile_size = 1; tile_size < 10; ++tile_size) {
                test_simple_array_sum(test_n + z, tile_size);
                CILKTEST_PRINTF(2, ".");
            }
        }
        CILKTEST_PRINTF(2, " PASSED\n");
    }
}



int main(int argc, char* argv[]) {
    CILKTEST_PARSE_TEST_ARGS(argc, argv);
    CILKTEST_BEGIN("scan");

    CILKTEST_REMARK(2, "Cilk Plus scan...\n");
    TestBlockedScan<CilkPlusScan>();
    
    test_sum_scan();

    return CILKTEST_END("scan");
}
