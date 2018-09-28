/* dotmix_test_rgen_kernels.h                  -*-C++-*-
 *
 *************************************************************************
 *
 * Copyright (C) 2012-13 Intel Corporation
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
 * @file dotmix_test_rgen_kernels.h
 *
 * @brief Defines methods for generating random numbers in a parallel
 * computation, using a DotMix DPRNG.
 */

#ifndef __DOTMIX_TEST_RGEN_KERNELS_H_
#define __DOTMIX_TEST_RGEN_KERNELS_H_

#include "dotmix_test_file_buffer.h"


// If we want to print out some initial pedigrees for debugging.
// static int PRINT_PEDIGREES_FIRST_LOOP = 0;

/**
 * @brief Generate n random numbers, using a single @c cilk_for loop,
 *        with one call to @c get in each loop.
 */
template <typename DPRNG_TYPE>
void dotmix_rgen_test_loop(int64_t n,
                           const char* file_prefix,
                           uint64_t seed)
{
    DPRNG_TYPE rng(seed);
    DPRNGFile file(file_prefix, seed);
    cilkpub::pedigree_scope cs = cilkpub::pedigree_scope::current();
    rng.init_scope(cs);

    // Bump loop rank hack.
    // Ignore the first iteration.  Depending on whether the compiler
    // or the user is adding in the bump loop rank call, the first
    // pedigree might be different.  But we've already hard-coded the
    // comparison code to expect the loop rank term in the pedigree to
    // start at "1".  Blah.
    //
    // The right answer is to eventually rerun the Dieharder tests to
    // generate new "gold" hashes for random number files.  But
    // hacking around it this way is easier for now...
    int64_t initial_offset = 1;
    #pragma cilk grainsize=1
    cilk_for(int64_t i = 0; i < n + initial_offset; ++i) {
        if (i >= initial_offset) {
            uint64_t ans;
            ans = rng.get();
            file.add_num(ans);

            // if (PRINT_PEDIGREES_FIRST_LOOP < 10) {
            //     cilkpub::pedigree cp = cilkpub::pedigree::current();
            //     fprintf(stderr, "Loop: i = %lld, ans = %llu\n", i, ans);
            //     cp.fprint(stderr, "Current_pedigree");
            //     PRINT_PEDIGREES_FIRST_LOOP++;
            // }
        }        
        __cilkrts_bump_loop_rank();
    }
}


// static int PRINT_PEDIGREES_SECOND_LOOP = 0;

/**
 * @brief Generate @c n random numbers, using a @c cilk_for loop,
 *        generating 256 numbers at a time.
 */
template <typename DPRNG_TYPE, int B>
void dotmix_rgen_test_loop_buffer(int64_t n,
                                  const char* file_prefix,
                                  uint64_t seed)
{
    DPRNG_TYPE rng(seed);
    DPRNGFile file(file_prefix, seed);

    cilkpub::pedigree_scope cs = cilkpub::pedigree_scope::current();
    rng.init_scope(cs);

    // Same bump-loop-rank hack.
    
    int initial_offset = 1;
    #pragma cilk grainsize=1
    cilk_for(int64_t i = 0;
             i < n/B + initial_offset;
             ++i) {
        // Ignore the first iteration.  Depending on whether the
        // compiler or the user is adding in the bump loop rank call,
        // the first pedigree might be different.  We've already
        // hard-coded the test to expect the loop rank term in the
        // pedigree to start at "1".
        if (i >= initial_offset) {
            uint64_t buffer[B];
            int start = (i-1) * B;
            int end = i * B;
            if (end > n) {
                end = n;
            }

            // if (PRINT_PEDIGREES_SECOND_LOOP < 10) {
            //     cilkpub::pedigree cp = cilkpub::pedigree::current();
            //     cp.fprint(stderr, "Current_pedigree");
            //     fprintf(stderr, "\n");
            //     bar++;
            // }

            int size = end - start;
            if (size > 0) {
                rng.fill_buffer(buffer, size);
                for (int j = 0; j < size; ++j) {
                    file.add_num(buffer[j]);
                }
            }
        }
        __cilkrts_bump_loop_rank();
    }
}


/// Recursive helper method.
template <typename DPRNG_TYPE>
void dotmix_rgen_test_ternary_tree_helper(int current_depth,
                                          DPRNG_TYPE& rng,
                                          DPRNGFile& file)
{
    // Add a number to the file.
    uint64_t num = rng.get();
    file.add_num(num);
    if (current_depth < 1) {
        return;
    }

    cilk_spawn dotmix_rgen_test_ternary_tree_helper(current_depth-1,
                                                    rng,
                                                    file);
    cilk_spawn dotmix_rgen_test_ternary_tree_helper(current_depth-1,
                                                    rng,
                                                    file);
    dotmix_rgen_test_ternary_tree_helper(current_depth-1,
                                         rng,
                                         file);
    cilk_sync;
}


/**
 * @brief Generates random numbers using a balanced ternary tree of
 *  depth @c tree_depth.
 */
template <typename DPRNG_TYPE>
void dotmix_rgen_test_ternary_tree(int64_t tree_depth,
                                   const char* file_prefix,
                                   uint64_t seed)
{
    assert((tree_depth >= 0) && (tree_depth <= 20));
    DPRNG_TYPE rng(seed);
    DPRNGFile file(file_prefix, seed);
    rng.init_scope(cilkpub::pedigree_scope::current());
    dotmix_rgen_test_ternary_tree_helper(tree_depth, rng, file);
}


/**
 * @brief Recursive helper method that computes fib, and add random
 * numbers to the file in the process.
 */
template <typename DPRNG_TYPE, int B>
int dotmix_rgen_test_fib_buffer_helper(int n,
                                       DPRNG_TYPE& rng,
                                       DPRNGFile& file)
{
    // Add a number to the file.
    uint64_t num = rng.get();
    file.add_num(num);
    if (n < 2) {
        // In the base case, fill with numbers from a buffer.
        uint64_t buffer[B];
        rng.fill_buffer(buffer, B);
        for (int j = 0; j < n; ++j) {
            file.add_num(buffer[j]);
        }
        return n;
    }

    int x = cilk_spawn dotmix_rgen_test_fib_buffer_helper<DPRNG_TYPE, B>(n-1,
                                                                         rng,
                                                                         file);
    int y = dotmix_rgen_test_fib_buffer_helper<DPRNG_TYPE, B>(n-2,
                                                              rng,
                                                              file);
    cilk_sync;
    return x+y;
}


/**
 * @brief Generates random numbers using fib, with of calls to @c get
 * and @c fill_buffer.
 */
template <typename DPRNG_TYPE, int B>
void dotmix_rgen_test_fib_buffer(int64_t n,
                                 const char* file_prefix,
                                 uint64_t seed)
{
    assert((n >= 0) && (n <= 50));
    DPRNG_TYPE rng(seed);
    DPRNGFile file(file_prefix, seed);
    rng.init_scope(cilkpub::pedigree_scope::current());
    dotmix_rgen_test_fib_buffer_helper<DPRNG_TYPE, B>((int)n, rng, file);
}

#endif  // !defined(__DOTMIX_TEST_RGEN_KERNELS_H_)
