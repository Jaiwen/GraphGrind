/* dotmix_rgen_driver.cpp                  -*-C++-*-
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
 * @file dotmix_rgen_driver.cpp
 *
 * @brief Test program for generating random number files using the
 * various DotMix RNGs
 *
 * @todo Currently this program must be run with CILK_NWORKERS=1,
 *  because the DPRNGFile class only supports serial additions to the
 *  file.
 */

#include <assert.h>
#include <cstdio>
#include <stdint.h>
#include <stdlib.h>

#include <cilk/cilk.h>
#include <cilkpub/dotmix.h>

#include "dotmix_test_rgen_kernels.h"


/// DOTMIX_TEST_MIX_ITER is the number of mixing iterations to use for
/// the generator.  This parameter can be set at compile time for the
/// test driver.
#ifndef DOTMIX_TEST_MIX_ITER
#    define DOTMIX_TEST_MIX_ITER 4
#endif


class RNG_TYPES {
public:
    static const int num_types = 4;
    enum {
        DMIX         = 0,
        DMIX_PRIME   = 1,
        F_DMIX       = 2,
        F_DMIX_PRIME = 3
    };

    static const char* get_name(int rng_type) {
        switch (rng_type) {
        case DMIX:
            return "dmix";
        case DMIX_PRIME:
            return "dmixPrime";
        case F_DMIX:
            return "fdmix";
        case F_DMIX_PRIME:
            return "fdmixPrime";
        default:
            fprintf(stderr, "ERROR: rng_type %d is not defined...\n", rng_type);
            return NULL;
        }
    }
};

class ALG_TYPES {
public:
    static const int num_types = 2;
    enum {
        LOOP      = 0,
        TREE      = 1,
        LOOP_B256 = 2,
        FIB_B8    = 3,
    };

    static const char* get_name(int alg_type) {
        switch (alg_type) {
        case LOOP:
            return "loop";
        case TREE:
            return "tree";
        case LOOP_B256:
            return "loopB256";
        case FIB_B8:
            return "fibB8";
        default:
            fprintf(stderr, "ERROR: alg_type %d is not defined...\n", alg_type);
            return NULL;
        }
    }
};


/**
 * @brief Creates a unique file name from the specified test parameters.
 *
 * @note This file name format is referenced in other scripts.  If
 * thie file name changes, those scripts may also need to be modified.
 */
static inline void generate_file_name(char* file_prefix,
                                      int buffer_size,
                                      int64_t n,
                                      int alg_type,
                                      int rng_type, 
                                      const char* test_name)
{
    const char* rng_string = RNG_TYPES::get_name(rng_type);
    const char* alg_string = ALG_TYPES::get_name(alg_type);
    assert(rng_string && alg_string);

    // If this format string chagnes, scripts may need to change too.
    const char* FILE_PREFIX_FORMAT = "%s_N%lld_A%s_G%s_R%d";
    
#ifdef _WIN32
    _snprintf_s(file_prefix,
                buffer_size,
                buffer_size,
                FILE_PREFIX_FORMAT, 
                test_name, 
                n,
                alg_string,
                rng_string,
                DOTMIX_TEST_MIX_ITER);
#else
    snprintf(file_prefix,
             buffer_size,
             FILE_PREFIX_FORMAT, 
             test_name, 
             n,
             alg_string,
             rng_string,
             DOTMIX_TEST_MIX_ITER);
#endif  
}


template <typename DPRNG_TYPE>
void execute_rgen_algorithm(int64_t param,
                            const char* file_prefix,
                            uint64_t seed,
                            int alg_type)
{
    switch (alg_type) {
    case ALG_TYPES::LOOP:
        dotmix_rgen_test_loop<DPRNG_TYPE>(param, file_prefix, seed);
        break;
    case ALG_TYPES::TREE:
        dotmix_rgen_test_ternary_tree<DPRNG_TYPE>(param, file_prefix, seed);
        break;
    case ALG_TYPES::LOOP_B256:
        dotmix_rgen_test_loop_buffer<DPRNG_TYPE, 256>(param,
                                                      file_prefix,
                                                      seed);
        break;
    case ALG_TYPES::FIB_B8:
        dotmix_rgen_test_fib_buffer<DPRNG_TYPE, 8>(param,
                                                   file_prefix,
                                                   seed);
        break;
    default:
        fprintf(stderr, "ERROR: alg_type %d is unknown\n", alg_type);
    }
}

/**
 * @brief Wrapper for random number generator test driver.
 *
 * This method parses the command-line input and runs the test.
 * It parses 2 required arguments, and 2 optional arguments.
 *
 * Required arguments: 
 *   argv[0]:   int64_t controlling the # of random numbers created.
 *   argv[1]:   string which is the unique prefix to append to the filenames.
 *
 * Optional arguments:
 *   argv[2]:   uint64_t which is the random seed for the generator.
 *   argv[3]:   int which specifies which algorithm to use to generate random numbers
 *   argv[4]:   int which specifies which RNG to use.
 */
int rgen_driver(int argc, char* argv[])
{
    int64_t n;
    uint64_t seed = 0x908e39a34eb8b33ULL;
    char* test_name;
    int alg_type = 0;
    int rng_type = 0;

    if (argc < 3) {
        fprintf(stderr,
                "Usage: [CILK_NWORKERS=1] %s <n> <filename> [<seed>, <alg_type>, <rng_type>]\n",
                argv[0]);
        exit(-1);
    }

    // We are generating a file of random numbers.  But since the file
    // I/O is probably going to be serialized anyway, it isn't really
    // worth it to generate the numbers in parallel.
    int P = __cilkrts_get_nworkers();
    
    if (P > 1) {
        fprintf(stderr,
                "ERROR: P = %d.  Appending to the file doesn't work yet with P > 1.\n",
                P);
        exit(-1);
    }

    n = atol(argv[1]);
    test_name = argv[2];

    // Parse optional arguments.
    if (argc >= 4) {
#ifdef _WIN32        
        seed = _strtoui64(argv[3], NULL, 10);
#else
        seed = strtoull(argv[3], NULL, 10);
#endif        
    }
    if (argc >= 5) {
        alg_type = atoi(argv[4]);
    }
    if (argc >= 6) {
        rng_type = atoi(argv[5]);
    }

    const char* rng_string = NULL;
    const int BUFFER_SIZE = 200;
    char file_prefix[BUFFER_SIZE];
    // Generate the file name.
    generate_file_name(file_prefix, BUFFER_SIZE,
                       n,
                       alg_type, rng_type,
                       test_name);

    // fprintf(stderr, "File name: %s\n", file_prefix);
    
    // Run the test on the appropriate RNG.
    // The number of mixing iterations for DotMix variants is
    // determined by the DOTMIX_TEST_MIX_ITER compile-time constant.
    switch (rng_type) {
    case RNG_TYPES::DMIX:
        typedef cilkpub::DotMixGeneric<cilkpub::DotMixRNGType::DMIX,
                                       DOTMIX_TEST_MIX_ITER > DMIX_TYPE;
        execute_rgen_algorithm<DMIX_TYPE>(n,
                                          file_prefix,
                                          seed,
                                          alg_type);
        break;
    case RNG_TYPES::DMIX_PRIME:
        typedef cilkpub::DotMixGeneric<cilkpub::DotMixRNGType::DMIX_PRIME,
                                       DOTMIX_TEST_MIX_ITER > DMIXP_TYPE;
        execute_rgen_algorithm<DMIXP_TYPE>(n,
                                           file_prefix,
                                           seed,
                                           alg_type);
        break;
    case RNG_TYPES::F_DMIX:
        typedef cilkpub::DotMixGeneric<cilkpub::DotMixRNGType::F_DMIX,
                                       DOTMIX_TEST_MIX_ITER > FDMIX_TYPE;
        execute_rgen_algorithm<FDMIX_TYPE>(n,
                                           file_prefix,
                                           seed,
                                           alg_type);
        break;
    case RNG_TYPES::F_DMIX_PRIME:
        typedef cilkpub::DotMixGeneric<cilkpub::DotMixRNGType::F_DMIX_PRIME,
                                       DOTMIX_TEST_MIX_ITER > FDMIXP_TYPE;
        execute_rgen_algorithm<FDMIXP_TYPE>(n,
                                            file_prefix,
                                            seed,
                                            alg_type);
        break;
    default:
        fprintf(stderr, "ERROR: unknown rng_type %d\n", rng_type);
    }
    return 0;
}




#if __INTEL_COMPILER < 1300 
void force_cilk_runtime_start() {
    return;
}

int main_wrapper(int argc, char* argv[]) {
    cilk_spawn force_cilk_runtime_start();
    cilk_sync;
    return rgen_driver(argc, argv);
}

int main(int argc, char* argv[])
{
    // We are going to force CILK_NWORKERS=1, since we are writing
    // output to file.  Since we are using this test only to check
    // random number quality, this operation is not worth running in
    // parallel anyway.
    int error = __cilkrts_set_param("nworkers", "1");
    assert(!error);

    return main_wrapper(argc, argv);
}

#else

int main(int argc, char* argv[])
{
    // We are going to force CILK_NWORKERS=1, since we are writing
    // output to file.  Since we are using this test only to check
    // random number quality, this operation is not worth running in
    // parallel anyway.
    int error = __cilkrts_set_param("nworkers", "1");
    assert(!error);
    return rgen_driver(argc, argv);
}
#endif
