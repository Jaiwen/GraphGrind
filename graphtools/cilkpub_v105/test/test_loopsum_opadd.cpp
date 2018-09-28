/* test_loopsum_opadd.cpp                  -*-C++-*-
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

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cilk/reducer_opadd.h>

#include <cilkpub/detred_opadd.h>

#include "cilktest_harness.h"
#include "cilktest_timing.h"

typedef cilkpub::det_reducer_opadd<double> DetRedDouble;


template <typename T>
static bool within_tol(T sum, T expected) {
    return ((sum - expected) * (sum - expected)) < 1.0e-12;
}

inline void print_timing_output_for_script(const char* filename,
					   int P,
					   double total_time_in_ms,
                                           int n,
                                           int B, 
					   double ans) {
    std::printf("%f, %d, %s, %d, %d, %g\n",
		total_time_in_ms * 1.0e-3,
		P,
		filename,
                n, B, 
		ans);
}

double f(int i) {
    return (1 / (1.0*(i+1)));
}


double inner_loop_body(int B, int i)
{
    double temp_sum = 0.0;
    for (int j = 0; j < B; ++j) {
        temp_sum += f(B*i + j);
    }
    return temp_sum;
}


double loopsum_normal(int n, int B)
{
    cilk::reducer_opadd<double> loopsum(0);
#pragma cilk grainsize = 1    
    _Cilk_for(int i = 0; i < n/B; ++i) {
        loopsum += inner_loop_body(B, i);
    }
    return loopsum.get_value();
}


double loopsum_dreducer(int n, int B)
{
    DetRedDouble loopsum(0);
#pragma cilk grainsize = 1    
    _Cilk_for(int i = 0; i < n/B; ++i) {
        loopsum += inner_loop_body(B, i);
    }
    return loopsum.get_value();
}


class TestType {
public:
    enum {
        ALL = 0,
        NORMAL_RUN = 1,
        DET_REDUCER = 2
    };
};

int cilk_main(int argc, char* argv[]) {
    int P = __cilkrts_get_nworkers();
    int n = 1000000;
    int B = 100;
    int num_reps = 100;
    double expected_ans = 0.0;
    double ans = 0.0;
    unsigned long long start, end;
    unsigned test_type = TestType::ALL;
    
    if (argc >= 2) {
	n = std::atoi(argv[1]);
    }
    if (argc >= 3) {
	B = std::atoi(argv[2]);
    }
    if (argc >= 4) {
        test_type = std::atoi(argv[3]);
    }

    // Round n up to a multiple of B.
    n = (n/B) * B + ((n % B) > 0) * B;


    if ((test_type == TestType::ALL) ||
        (test_type == TestType::NORMAL_RUN)) {
        // Timing loop.
        start = CILKTEST_GETTICKS();
        for (int k = 0; k < num_reps; k++) {
            expected_ans += loopsum_normal(n, B);
        }
        end = CILKTEST_GETTICKS();

        print_timing_output_for_script("loopsum_normal",
                                       P,
                                       (end - start),
                                       n,
                                       B, 
                                       expected_ans);
    }

    if ((test_type == TestType::ALL) ||
        (test_type == TestType::DET_REDUCER)) {
        // Timing loop.
        start = CILKTEST_GETTICKS();
        for (int k = 0; k < num_reps; k++) {
            ans += loopsum_dreducer(n, B);
        }
        end = CILKTEST_GETTICKS();

        print_timing_output_for_script("loopsum_det",
                                       P,
                                       (end - start),
                                       n,
                                       B,
                                       ans);
    }

    if (test_type == TestType::ALL) {
        assert(within_tol(expected_ans, ans));
    }

    return 0;
}

int main(int argc, char* argv[]) {
    int res = cilk_main(argc, argv);
    __cilkrts_end_cilk();
    return res;
}

