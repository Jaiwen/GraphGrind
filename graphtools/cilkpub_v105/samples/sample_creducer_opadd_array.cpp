/* sample_creducer_opadd_array.cpp                  -*-C++-*-
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
 * @file sample_creducer_opadd_array.cpp
 *
 * @brief Sample program illustrating use of commutative reducer
 * array.
 */

#include <cstdio>
#include <cstdlib>

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <cilk/reducer_opadd.h>
#include <cilkpub/creducer_opadd_array.h>

const int N=10000;

// Global array we are using.
int A[N];

/// An arbitrary hash of i and rep.
inline unsigned random_hash(int x) {
    unsigned q = x+1;
    for (int j = 0; j < 10; ++j) {
        q = q * (2*q + 1);
    }
    return q;
}


int main(int argc, char* argv[]) {

    int rep_count = 2500;
    // Check total sum.
    cilk::reducer_opadd<int> check_total_sum(0);
    // Initialize the original array, in parallel.
    cilk_for (int i = 0; i < N; ++i) {
        A[i] = 0;
    }

    {
        // Construct the reducer array of size N, from the original
        // array A.
        cilkpub::creducer_opadd_array<int> cred_A(A, N);

        // Alternative:
        //   creducer_opadd_array<int> cred_A(N);
        //   cred_A.move_in(A);

        cilk_for(int j = 0; j < rep_count*N; ++j) {
            unsigned v_idx = random_hash(j) % N;
            assert((v_idx >= 0) && (v_idx < N));
            // Update j to slot at index v_idx.
            cred_A[v_idx] += j;
            // Compute total sum. 
            check_total_sum += j;
        }

        // Move the data back out to original array A.
        cred_A.move_out(A);
    }

    // Test case.
    int total_sum = 0;
    for (int i = 0; i < N; ++i) {
        total_sum += A[i];
    }

    std::printf("N = %d, final sum is %d\n", N, total_sum);
    assert(total_sum == check_total_sum.get_value());
    return 0;
}

