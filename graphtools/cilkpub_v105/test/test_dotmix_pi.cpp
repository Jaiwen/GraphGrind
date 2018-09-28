/* test_dotmix_pi.cpp                  -*-C++-*-
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
 * @file test_dotmix_pi.cpp
 *
 * @brief Test DotMix DPRNG via a Monte-Carlo calculation of pi.
 */

#include <iostream>

#include <cilkpub/dotmix.h>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <cilk/reducer_opadd.h>
#include "cilktest_harness.h"
#include "cilktest_timing.h"

#ifdef _WIN32
// Hackery on Windows to avoid a conflict between
// std::numeric_limits<>max() and min/max macros in windows.h.
// This hack works because these are the last includes. 
#  undef min
#  undef max
#  define _USE_MATH_DEFINES
#endif
#include <limits>
#include <cmath>

/// Debugging flag: set to true to print out random numbers generated.
#define PRINT_RAND_NUMS false

inline void tfprint(FILE* f, cilkpub::pedigree& ped)
{
    ped.fprint(f, "");
}

inline void tfprint(FILE* f, cilkpub::pedigree_scope& scope)
{
    scope.fprint(f, "");
}

void null_func() {
}


// Test the DPRNG using individual calls to "get".
template <typename DPRNG_TYPE>
uint64_t
MonteCarloPi_using_get(uint64_t N, DPRNG_TYPE& dprng,
                       cilk::reducer_opadd<uint64_t>& check_sum,
                       cilkpub::pedigree_scope& scope)
{
    // Number of "hits" inside the circle
    cilk::reducer_opadd<uint64_t> inside(0);
    
    cilk_for (uint64_t i = 0; i < N; ++i) {
        __cilkrts_bump_loop_rank();

        // Get numbers from the DPRNG.
        cilkpub::pedigree p0 = cilkpub::pedigree::current();
        cilkpub::pedigree sp0 = cilkpub::pedigree::current(scope);
        CILKTEST_PRINT(4, "Ped before: ", p0, "\n");
        CILKTEST_PRINT(4, "scoped Ped after x, y: ", sp0, "\n");

        uint64_t x_sample = dprng.get();

        cilkpub::pedigree p1 = cilkpub::pedigree::current();
        cilkpub::pedigree sp1 = cilkpub::pedigree::current(scope);
        CILKTEST_PRINT(4, "Ped after x: ", p1, "\n");
        CILKTEST_PRINT(4, "scoped Ped after x, y: ", sp1, "\n");

        uint64_t y_sample = dprng.get();

        cilkpub::pedigree ped = cilkpub::pedigree::current();
        cilkpub::pedigree sp2 = cilkpub::pedigree::current(scope);
        CILKTEST_PRINT(4, "Ped after x, y: ", ped, "\n");
        CILKTEST_PRINT(4, "scoped Ped after x, y: ", sp2, "\n");
        
        CILKTEST_REMARK(4, "%lu, %lu\n", x_sample, y_sample);
        
        check_sum += (x_sample + y_sample);

        // Convert to double floating point numbers in the closed interval [0, 1]
        double x = (double)(x_sample)/std::numeric_limits<uint64_t>::max();
        double y = (double)(y_sample)/std::numeric_limits<uint64_t>::max();

        if (PRINT_RAND_NUMS)
            std::cerr << "x = " << x << ", y = " << y << std::endl;

        double m = (x*x) + (y*y);

        if(m <= 1) // inside circle
            ++inside;
    }

    return inside.get_value();
}


// Test the DPRNG using calls to fill_buffer.
template <typename DPRNG_TYPE>
uint64_t
MonteCarloPi_using_fill_buffer(uint64_t N, DPRNG_TYPE& dprng,
                               cilk::reducer_opadd<uint64_t>& check_sum,
                               cilkpub::pedigree_scope& scope)
{
    // Number of "hits" inside the circle
    cilk::reducer_opadd<uint64_t> inside(0);
    
    // Number of points we want to generate at a time.
    const int POINT_BLOCK_SIZE = 256;
    
    cilk_for (uint64_t i = 0; i < N/POINT_BLOCK_SIZE; ++i) {
        uint64_t buffer[2*POINT_BLOCK_SIZE];

        // Figure out how many terms in the buffer we want to fill.
        int64_t L = POINT_BLOCK_SIZE;

        if ((POINT_BLOCK_SIZE * i) + L >= N) {
            L = N - (POINT_BLOCK_SIZE * i);
            assert((0 <= L) && (L <= POINT_BLOCK_SIZE));
        }

        // Get numbers from the DPRNG.
        cilkpub::pedigree p0 = cilkpub::pedigree::current();
        cilkpub::pedigree sp0 = cilkpub::pedigree::current(scope);
        CILKTEST_REMARK(4, "fill_buffer block loop: i = %d, L = %d: \n", i, L);
        CILKTEST_PRINT(4, "Ped before: ", p0, "\n");
        CILKTEST_PRINT(4, "scoped Ped after x, y: ", sp0, "\n");
        
        // Fill the buffer.
        dprng.fill_buffer(buffer, 2*L);

        if (PRINT_RAND_NUMS) {
            for (int i = 0; i < L; ++i) {
                std::cerr << "x = " << buffer[2*i] << ", y = " << buffer[2*i+1] << std::endl;
            }
        }

        // Temporary values to accumulate in serial for loop.
        uint64_t check_sum_serial = 0;
        uint64_t inside_serial = 0;
        for (int i = 0; i < L; ++i) {
            uint64_t x_sample = buffer[2*i];
            uint64_t y_sample = buffer[2*i+1];
            check_sum_serial += (x_sample + y_sample);

            // Convert to double floating point numbers in the closed interval [0, 1]
            double x = (double)(x_sample)/std::numeric_limits<uint64_t>::max();
            double y = (double)(y_sample)/std::numeric_limits<uint64_t>::max();
            double m = (x*x) + (y*y);
            if(m <= 1) // inside circle
                ++inside_serial;
        }
        // Update reducers.
        check_sum += check_sum_serial;
        inside += inside_serial;
    }
    
    return inside.get_value();
}


template <typename DPRNG_TYPE>
double
MonteCarloPi(uint64_t N, uint64_t* check_sum_ptr, int use_buffered_get)
{
    DPRNG_TYPE dprng(0x8c679c168e6bf733ul);

    // Make sure the Cilk runtime is started before we start timing.
    _Cilk_spawn null_func();
    null_func();
    _Cilk_sync;
    
    cilkpub::pedigree_scope scope = cilkpub::pedigree_scope::current();

    // Set seed and scope of dprng.
    dprng.init_scope(scope);
    CILKTEST_PRINT(3, "MonteCarlo scope: ", scope, "\n");

    // Adding up the sample random numbers, to make sure the result is
    // deterministic.
    cilk::reducer_opadd<uint64_t> check_sum(0);

    // Throw many "darts" are top right quadrant of the circle,
    // recording the ones landing inside the circle
    uint64_t final_count;

    if (use_buffered_get) {
        final_count = MonteCarloPi_using_fill_buffer(N,
                                                     dprng,
                                                     check_sum,
                                                     scope);
    }
    else {
        final_count = MonteCarloPi_using_get(N,
                                             dprng,
                                             check_sum,
                                             scope);
    }
    // Store the return values. 
    *check_sum_ptr = check_sum.get_value();
    double pi_val = 4.0 * static_cast<double>(final_count) / N;

    return pi_val;
}


template <typename DPRNG_TYPE>
void test_monte_carlo_pi(uint64_t N, int P, int use_buffered_get)
{
    unsigned long long t1, t2;
    uint64_t piEst;

    // Number of repetitions. 
    const int NUM_REPS = 5;
    double pi_val[NUM_REPS];
    uint64_t check_sum[NUM_REPS];

    for (int i = 0; i < NUM_REPS; ++i) {
        unsigned long long t_total = 0;
        check_sum[i] = 0;

        t1 = CILKTEST_GETTICKS();
        pi_val[i] = cilk_spawn(MonteCarloPi<DPRNG_TYPE>(N, &check_sum[i], use_buffered_get));
        cilk_sync;
        t2 = CILKTEST_GETTICKS();

        CILKTEST_REMARK(2,
                        "# Monte Carlo Pi, Buffered=%d, Repetition %d. Pi est=%f, Delta=%f, CheckSum=%ld\n",
                        use_buffered_get,
                        i,
                        pi_val[i],
                        (M_PI - pi_val[i]),
                        check_sum[i]);
        t_total += (t2 - t1);
        CILKTEST_REMARK(1, "%d, %f\n", P, t_total/1.0e6);
    }

    for (int i = 1; i < NUM_REPS; ++i) {
        TEST_ASSERT(pi_val[i] == pi_val[0]);
        TEST_ASSERT(check_sum[i] == check_sum[0]);
    }
}

int
main(int argc, char* argv[])
{
    CILKTEST_PARSE_TEST_ARGS(argc, argv);
    CILKTEST_BEGIN("dprng_pi");

    uint64_t N = 20000000; // Num of trials

    int P = __cilkrts_get_nworkers();

    for (int use_buffered_get = 0; use_buffered_get <= 1; ++use_buffered_get) {
        CILKTEST_REMARK(2, "Monte Carlo Pi: Buffered get = %d\n", use_buffered_get);
        
        CILKTEST_REMARK(2, "Monte Carlo Pi: DotMix\n");
        test_monte_carlo_pi<cilkpub::DotMix>(N, P, use_buffered_get);
        CILKTEST_REMARK(2, "Monte Carlo Pi: DotMixPrime\n");
        test_monte_carlo_pi<cilkpub::DotMixPrime>(N, P, use_buffered_get);
        CILKTEST_REMARK(2, "Monte Carlo Pi: ForwardDotMix\n");
        test_monte_carlo_pi<cilkpub::ForwardDotMix>(N, P, use_buffered_get);
        CILKTEST_REMARK(2, "Monte Carlo Pi: ForwardDotMixPrime\n");
        test_monte_carlo_pi<cilkpub::ForwardDotMixPrime>(N, P, use_buffered_get);
    }
    
    return CILKTEST_END("dprng_pi");
}
