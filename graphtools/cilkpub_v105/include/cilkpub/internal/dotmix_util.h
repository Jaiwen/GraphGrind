/* dotmix_util.h                 -*-C++-*-
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
 * @file dotmix_util.h
 *
 * @brief Common math routines for DotMix DPRNG.
 *
 * @author Tao B. Schardl
 * @author Jim Sukha
 */

/**
 * @file dotmix_util.h
 *
 * @brief Implementation of utility methods for DotMix.
 *
 * The methods in this class should be usable independent of
 * pedigrees.
 * 
 * \defgroup DotMix DotMix DPRNG
 *
 * @author   Tao B. Schardl
 * @author   Jim Sukha
 */

#include <cilk/cilk.h>

namespace cilkpub {

    uint64_t DotMixUtil::swap_halves(uint64_t x)
    {
        return (x >> (4 * sizeof(uint64_t))) | (x << (4 * sizeof(uint64_t)));
    }

    template <int NUM_MIX_ITER>
    uint64_t DotMixUtil::mix(uint64_t x) {
        for (int j = 0; j < NUM_MIX_ITER; ++j)
        {
            x = x*(2*x + 1);
            x = swap_halves(x);
        }
        return x;
    }


    template <int NUM_MIX_ITER>
    uint64_t DotMixUtil::mix_mod_p(uint64_t x) 
    {
        x = mix<NUM_MIX_ITER>(x);
        return x - (PRIME & -(x >= PRIME));
    }

    uint64_t DotMixUtil::sum_mod_p(uint64_t a, uint64_t b)
    {
        uint64_t z = a + b;
        if ((z < a) || (z >= PRIME)) {
            z -= PRIME;
        }
        return z;     
    }

    void DotMixUtil::dotprod_update_mod_p(uint64_t a,
                                          uint64_t x,
                                          uint64_t* result)
    {
        uint64_t ah = a >> 32;
        uint64_t al = (a & LOW32_MASK);
        uint64_t xh = (x >> 32) & LOW32_MASK;
        uint64_t xl = (x & LOW32_MASK);
        
        // Each of these products is guaranteed to be less than p
        // because (2^32-1)*(2^32-1) = 2^64 - 2^33 + 1 < p.
        result[0] = sum_mod_p(result[0], ah * xh);
        result[1] = sum_mod_p(result[1], ah * xl);
        result[1] = sum_mod_p(result[1], al * xh);
        result[2] = sum_mod_p(result[2], al * xl);
    }

    uint64_t DotMixUtil::finalize_dotprod_mod_p(uint64_t res,
                                                uint64_t* tmp_result)
    {
        uint64_t y0 = tmp_result[1] & LOW32_MASK;
        uint64_t y1 = tmp_result[1] >> 32;
        uint64_t x0 = tmp_result[0] & LOW32_MASK;
        uint64_t x1 = tmp_result[0] >> 32;
        uint64_t alpha = x1 * TWO_POW64_MOD_P;
        uint64_t alpha_0 = alpha & LOW32_MASK;
        uint64_t alpha_1 = alpha >> 32;

        // Calculate each value.
        uint64_t r1 = sum_mod_p(tmp_result[2], y0 << 32);
        uint64_t r2 = sum_mod_p(y1 * TWO_POW64_MOD_P,
                                x0 * TWO_POW64_MOD_P);
        uint64_t r3 = sum_mod_p(alpha_0 << 32,
                                alpha_1 * TWO_POW64_MOD_P);

        // While we want to add r1, r2, and r3 together modulo P, we don't
        // care about result itself being modulo P.
        res += sum_mod_p(r1, sum_mod_p(r2, r3));
        return res;
    }

    template <int NUM_MIX_ITER>
    void DotMixUtil::update_and_fill_buffer(uint64_t* output_buffer,
                                            int n,
                                            uint64_t prefix_result,
                                            uint64_t table_term)
    {
        cilk_for(int i = 0; i < n; ++i) {
            uint64_t compressed_ped_val;
            compressed_ped_val = prefix_result + (i+1) * table_term;
            output_buffer[i] = mix<NUM_MIX_ITER>(compressed_ped_val);
        }
    }

    template <int NUM_MIX_ITER>
    void DotMixUtil::update_mod_p_and_fill_buffer(uint64_t *output_buffer,
                                                  int n,
                                                  uint64_t prefix_result,
                                                  uint64_t table_term)
    {
        cilk_for(int i = 0; i < n; ++i)
        {
            uint64_t compressed_ped_val;
            uint64_t tmp_result[3];
            tmp_result[:] = 0;
            dotprod_update_mod_p((i+1),
                                 table_term,
                                 tmp_result);
            compressed_ped_val = finalize_dotprod_mod_p(prefix_result,
                                                        tmp_result);
            output_buffer[i] = mix<NUM_MIX_ITER>(compressed_ped_val);
        }
    }

                                 
    
}; // namespace cilkpub
