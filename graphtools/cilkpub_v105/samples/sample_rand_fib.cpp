/* sample_rand_fib.cpp                  -*-C++-*-
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
 * @file sample_rand_fib.cpp
 *
 * @brief Sample program illustrating use of DotMix on fib.
 */

#include <cstdio>
#include <cilk/cilk.h>
#include <cilk/reducer_opadd.h>
#include <cilkpub/dotmix.h>        // Include file for DotMix

// Global reducer to add up random numbers.
cilk::reducer_opadd<uint64_t> sum(0);

// Create and seed a global DotMix RNG.
cilkpub::DotMix global_rng(234909128);

int rand_fib(int n) {
    if (n < 2) {
        // Generate a random number, add it to sum.
        sum += global_rng.get() * n;
        return n;
    }
    else {
        int x, y;
        sum += global_rng.get() * n;
        x = cilk_spawn rand_fib(n-1);
        sum += global_rng.get() * n;
        y = rand_fib(n-2);
        cilk_sync;
        sum += global_rng.get() * n;
        return x+y;
    }
}

int main(void) {
    int n = 30;
    int ans = rand_fib(n);
    std::printf("fib(%d) = %d\n", n, ans);
    std::printf("sum is %llu\n", sum.get_value());
    return 0;
}
