/* sample_reducer_vector.cpp                  -*-C++-*-
 *
 *************************************************************************
 *
 * Copyright (C) 2013 Intel Corporation
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
 * @file sample_reducer_vector.cpp
 *
 * @brief Sample program illustrating the use of a reducer_vector.
 */

#include <cstdio>
#include <vector>

#include <cilk/cilk.h>
#include <cilkpub/reducer_vector.h>

// Global vector reducer to store numbers.
cilk::reducer< cilkpub::op_vector<int> > red;

// Minimum value of fib to keep. 
const int threshhold = 46368;

// Calculate fib and store some intermediate results into reducer
// vector.
int fib_reducer_vector(int n) {
    if (n < 2) {
        return n;
    }
    else {
        int x, y;
        x = cilk_spawn fib_reducer_vector(n-1);
        y = fib_reducer_vector(n-2);
        cilk_sync;

        // Append sums which are larger than the threshhold.
        if ((x+y) >= threshhold) {
            red->push_back(x+y);
        }
        return x+y;
    }
}

int main(void) {
    int n = 35;

    // Calculate the answer and store output into reducer.
    int ans = fib_reducer_vector(n);
    std::printf("fib(%d) = %d\n", n, ans);

    // Move the result into a normal vector.
    std::vector<int> final_vec;
    red.move_out(final_vec);

    std::printf("Final vector: size = %llu\n",
		(unsigned long long)final_vec.size());
    for (size_t i = 0; i < final_vec.size(); ++i) {
        std::printf("%d ", final_vec[i]);
    }
    std::printf("\n");
    return 0;
}
