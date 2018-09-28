/* sample_dotmix.cpp                  -*-C++-*-
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
 * @file sample_dotmix.cpp
 *
 * @brief Sample program illustrating how to use DotMix DPRNG.
 */

#include <cstdio>
#include <cstdlib>

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <cilk/reducer_opadd.h>
#include <cilkpub/dotmix.h>

/// Global reducer to add up random numbers.
cilk::reducer_opadd<uint64_t> fib_sum(0);

/// Global DotMix RNG.
cilkpub::DotMix global_rng(234909128);

/**
 *@brief A computation generating random numbers, but with a parallel
 *structure like "fib".
 *
 * @param  n  The input to fib to determine the tree structure.
 * @return The sum of the random numbers generated.
 */
int fib_rng(int n) {
    if (n < 2) {
        uint64_t v = global_rng.get();
        fib_sum += v;
        return v;
    }
    else {
        uint64_t x, y;
        x = cilk_spawn fib_rng(n-1);

        uint64_t* buf = new uint64_t[n];
        global_rng.fill_buffer(buf, n);
        for (int i = 0; i < n; ++i) {
            fib_sum += buf[i];
        }
        y = fib_rng(n-2);
        delete[] buf;
        cilk_sync;
        return x+y;
    }
}


/**
 * @brief A simple parallel loop of n iterations, with each iteration
 * generating random numbers.
 *
 * @param seed        The initial seed for the generator.
 * @param scope_rng   True if we want to scope the rng, false otherwise.
 * @param buffer_size Specifies how many rng's to create. 
 * @param desc        String describing this test.
 *
 * If @c buffer_size <= 0, we generate one random number.
 * If @c buffer_size > 0, we fill in a buffer with that many random numbers.
 *
 * @return           The sum of all the random numbers generated.
 */
uint64_t simple_loop(int n,
                     uint64_t seed,
                     bool scope_rng,
                     int buffer_size,
                     const char* desc)
{
    cilk::reducer_opadd<uint64_t> sum(0);
    
    // Create a RNG. 
    cilkpub::DotMix rng(seed);

    // Scope it to the beginning of this loop.  If you want multiple
    // calls to this function to have the same RNG's, then you need to
    // scope it.
    // If you want distinct numbers, then leave it unscoped.
    
    if (scope_rng) {
        rng.init_scope(cilkpub::pedigree_scope::current());
    }
    
    cilk_for(int i = 0; i < n; ++i) {
        // This call is needed at the beginning of every iteration
        // only if you care about getting the same answer when
        // changing the value of P.
        //
        // Normally, the grainsize of a cilk_for loop varies depending
        // on P, which can affect the pedigrees that are hashed.
        __cilkrts_bump_loop_rank();

        if (buffer_size <= 0) {
            // Get one random number only.
            uint64_t ans = rng.get();
            if (n <= 20) {
                std::printf("Iteration %d: got number %llu\n",
			    i, ans);
            }
            sum += ans;
        }
        else {
            // Get a whole buffer of random numbers.
            uint64_t* buffer = new uint64_t[buffer_size];
            // Initialize to 0 just in case. 
            buffer[0:buffer_size] = 0;

            rng.fill_buffer(buffer, buffer_size);
            for (int j = 0; j < buffer_size; ++j) {
                sum += buffer[j];
            }
            delete[] buffer;
        }
    }

    std::printf("%s\t  RNG: seed = %llu, buffer_size=%d, ans = %llu\n",
		desc,
		seed,
		buffer_size,
		sum.get_value());
    
    return sum.get_value();
}


int main(int argc, char* argv[]) {
    int n = 1000;
    int d = 15;
    if (argc >= 2) {
        n = atoi(argv[1]);
    }
    std::printf("Sample program using Dotmix. n = %d\n", n);
    
    int B = 8;
    for (int buffer_size = 0; buffer_size <= B; buffer_size += B) {
        uint64_t ans[5];

        std::printf("Buffer sizes of %d: \n", buffer_size);
        ans[0] = simple_loop(n, 42, true,  buffer_size, "Scoped, 1st time");
        ans[1] = simple_loop(n, 42, false, buffer_size, "Unscoped, 1st time");
        ans[2] = simple_loop(n, 42, false, buffer_size, "Unscoped, 2nd time");
        ans[3] = simple_loop(n, 42, true,  buffer_size, "Scoped, 2nd time");
        ans[4] = simple_loop(n, 42, true,  buffer_size, "Scoped, 3rd time");
        std::printf("\n");
        
        // All the scoped versions should have the same value.
        assert((ans[0] == ans[3]) &&
               (ans[0] == ans[4]));

        // All the unscoped versions should each be different, and the
        // also be different from the scoped result.  Of course, since we
        // are dealing with random numbers, this there is some small
        // probability that the sums are the same...
        if ((ans[0] == ans[1]) ||
            (ans[0] == ans[2]) ||
            (ans[1] == ans[2])) {
            std::printf("WARNING: Possible bug detected.  It is unlikely that we generate the same sums %llu, %llu, %llu\n",
			ans[0], ans[1], ans[2]);
        }
    }

    // Call fib with a global RNG.
    uint64_t a1, a2;
    a1 = cilk_spawn fib_rng(30);
    a2 = fib_rng(30);
    cilk_sync;
    
    std::printf("fib_rng:  %llu and %llu\n", a1, a2);
    
    if (a1 == a2) {
        std::printf("WARNING: Possible bug detected.  It is unlikely we generate the same sum %llu\n",
		    a1);
    }

    return 0;
}

