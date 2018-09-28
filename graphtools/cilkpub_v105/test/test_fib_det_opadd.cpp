/* test_fib_det_opadd.cpp                  -*-C++-*-
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

/*
 * @File: fib_det_opadd.t.cpp
 *
 * @Brief: Calculates fib using an deterministic floating-point opadd reducer.
 *
 */

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cilk/reducer_opadd.h>

#include <cilkpub/detred_opadd.h>
#include "cilktest_harness.h"
#include "cilktest_timing.h"

typedef cilkpub::det_reducer_opadd<double> DetRedDouble;

inline void print_timing_output_for_script(const char* filename,
					   int P,
					   double total_time_in_ms,
					   int num_reps,
					   int param, 
					   int ans) {
    std::printf("%f, %d, %s, %d, %d, %d\n",
		total_time_in_ms * 1.0e-3,
		P,
		filename,
		num_reps,
		param,
		ans);
}

// Calculate the expected result of fib(n) using a simple, linear,
// serial calculation.
double linear_fib(int n)
{
    if (n < 2) return n;

    double curr = 1, one_before = 1, two_before = 0;
    for (int i = 2; i < n; ++i)
    {
        two_before = one_before;
        one_before = curr;
        curr = two_before + one_before;
    }

    return curr;
}


cilk::reducer_opadd<double> fib_sum(0);

#define EXTRA_UPDATES
#ifdef EXTRA_UPDATES
inline void extra_update(int x) { fib_sum += x; }
#else
inline void extra_update(int x) { }
#endif

void fib_with_reducer_internal(int n) {
  if (n < 2) {
      fib_sum += n;
  }
  else {
      _Cilk_spawn fib_with_reducer_internal(n-1);
      fib_with_reducer_internal(n-2);
      _Cilk_sync;
  }
}

double fib_with_reducer(int n) {
    fib_sum.set_value(0);
    fib_with_reducer_internal(n);
    return fib_sum.get_value();
}




void fib_with_dreducer_internal(DetRedDouble* fib_dsum,
                                int n) {
  if (n < 2) {
      (*fib_dsum) += n;
  }
  else {
      _Cilk_spawn fib_with_dreducer_internal(fib_dsum, n-1);
      fib_with_dreducer_internal(fib_dsum, n-2);
      _Cilk_sync;
  }
}
double fib_with_dreducer(int n) {

    DetRedDouble fib_dsum(0);
    fib_with_dreducer_internal(&fib_dsum, n);
    return fib_dsum.get_value();
}



bool within_tol(double v1, double v2) {
    return ((v1 - v2)*(v1 - v2) < 1.0e-12);
}


int cilk_main(int argc, char* argv[]) {
    int n = 30;
    int P = __cilkrts_get_nworkers();
    int num_reps = 10;
    double expected_ans;
    int ans;
    unsigned long long start, end;

    if (argc >= 2) {
	n = std::atoi(argv[1]);
    }
    if (argc >= 3) {
	num_reps = std::atoi(argv[2]);
    }

    // Spawn an execution of linear fib, to get the expected answer,
    // and also to initialize the runtime (so we aren't measuring
    // repeated entries.)
    expected_ans = _Cilk_spawn linear_fib(n);
    ans = linear_fib(n);
    _Cilk_sync;
    assert(within_tol(expected_ans, ans));
    
    // Timing loop.
    start = CILKTEST_GETTICKS();
    for (int k = 0; k < num_reps; k++) {
	ans = fib_with_reducer(n);
	assert(expected_ans == ans);
    }
    end = CILKTEST_GETTICKS();

    print_timing_output_for_script("fib_opadd_normal",
				   P,
				   (end - start),
				   num_reps, 
				   n,
				   ans);


    // Timing loop.
    start = CILKTEST_GETTICKS();
    for (int k = 0; k < num_reps; k++) {
	ans = _Cilk_spawn fib_with_dreducer(n);
        _Cilk_sync;
	assert(expected_ans == ans);
    }
    end = CILKTEST_GETTICKS();

    print_timing_output_for_script("fib_opadd_det",
				   P,
				   (end - start),
				   num_reps, 
				   n,
				   ans);
    return 0;
}

int main(int argc, char* argv[]) {
    int res = cilk_main(argc, argv);
    __cilkrts_end_cilk();
    return res;
}

