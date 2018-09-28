/* dotmix_generic.h                 -*-C++-*-
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
 * @file dotmix_generic.h
 *
 * @brief Internal implementation DotMixGeneric class.
 *
 * This file defines methods that are (mostly) the same for all DotMix
 * types.
 *
 * \defgroup DotMix DotMix DPRNG
 *
 * @author   Tao B. Schardl
 * @author   Jim Sukha
 */


#ifndef __DOTMIX_GENERIC_H_
#define __DOTMIX_GENERIC_H_


namespace cilkpub {


    template <int RNG_TYPE, int NUM_MIX_ITER>
    DotMixGeneric<RNG_TYPE, NUM_MIX_ITER>::DotMixGeneric()
        : m_table_length(0)
        , m_seed(0x8c679c168e6bf733ul)
        , m_scope()
    {
        // Construct the table and fill it in.
        m_table = (uint64_t*)malloc(sizeof(uint64_t) * MAX_TABLE_LENGTH);
        init_seed(m_seed);
    }


    template <int RNG_TYPE, int NUM_MIX_ITER>
    DotMixGeneric<RNG_TYPE, NUM_MIX_ITER>::DotMixGeneric(uint64_t seed)
        : m_table_length(0)
        , m_seed(seed)
        , m_scope()
    {
        // Construct the table and fill it in.
        m_table = (uint64_t*)malloc(sizeof(uint64_t) * MAX_TABLE_LENGTH);
        init_seed(m_seed);
    }
    

    template <int RNG_TYPE, int NUM_MIX_ITER>
    DotMixGeneric<RNG_TYPE, NUM_MIX_ITER>::~DotMixGeneric()
    {
        assert(m_table);
        free(m_table);
    }

    template <int RNG_TYPE, int NUM_MIX_ITER>
    void DotMixGeneric<RNG_TYPE, NUM_MIX_ITER>::init_seed(uint64_t seed)
    {
        m_seed = seed;
        m_table_length = MAX_TABLE_LENGTH;

        // Windows complains about switching on a constant value.
        // TBD: I'd like to find a clean way to do this, e.g., with
        // template specialization, but I haven't been able to figure
        // it out.
#ifdef _WIN32    
#pragma warning(disable:280)
#endif
        // Fill in the table.  Call different methods based on RNG_TYPE.
        switch (RNG_TYPE) {
        case DotMixRNGType::DMIX_PRIME:
        case DotMixRNGType::F_DMIX_PRIME:
            for (int i = 0; i < m_table_length; ++i) {
                m_table[i] = DotMixUtil::mix_mod_p<NUM_MIX_ITER>(seed + i);
            }
            m_X = DotMixUtil::mix_mod_p<NUM_MIX_ITER>(seed + m_table_length);
            break;

        case DotMixRNGType::DMIX:
        case DotMixRNGType::F_DMIX:
        default:
            for (int i = 0; i < m_table_length; ++i) {
                m_table[i] = DotMixUtil::mix<NUM_MIX_ITER>(seed + i);
            }
            m_X = DotMixUtil::mix<NUM_MIX_ITER>(seed + m_table_length);
        }
    }

#ifdef _WIN32    
#pragma warning(enable:280)
#endif    
    template <int RNG_TYPE, int NUM_MIX_ITER>
    void DotMixGeneric<RNG_TYPE, NUM_MIX_ITER>::init_scope(const pedigree_scope& scope)
    {
        m_scope = scope;
    }

    template <int RNG_TYPE, int NUM_MIX_ITER>
    void DotMixGeneric<RNG_TYPE, NUM_MIX_ITER>::init(uint64_t seed,
                                                     pedigree_scope& scope)
    {
        this->init_seed(seed);
        this->init_scope(scope);
    }

    template <int RNG_TYPE, int NUM_MIX_ITER>
    uint64_t DotMixGeneric<RNG_TYPE, NUM_MIX_ITER>::get() {
        // Compress the pedigree, then mix the result.
        int ped_length;
        uint64_t result =
            internal::DotMixRgen<NUM_MIX_ITER>::get_compressed_pedigree(*this,
                                                                        &ped_length);
        // We are still going to mix normally (without the mod p
        // calculation), even in the case of DMIX_PRIME and
        // F_DMIX_PRIME, because we still want to return a random
        // value in [0, 2^64).
        // It probably does not matter much either way...
        return DotMixUtil::mix<NUM_MIX_ITER>(result);
    }


    template <int RNG_TYPE, int NUM_MIX_ITER>
    void
    DotMixGeneric<RNG_TYPE, NUM_MIX_ITER>::fill_buffer(uint64_t* output_buffer,
                                                       int n)
    {
        // For filling a buffer with random numbers, we first
        // partially compress the current pedigree.
        //
        // Then, for each term in the buffer, we update the compressed
        // pedigree to add in an extra term (corresponding to the
        // index of that term in the buffer), and then mix the result.
        
        int ped_length;
        uint64_t prefix_result
            = internal::DotMixRgen<NUM_MIX_ITER>::get_compressed_pedigree(*this,
                                                                          &ped_length);
        assert((ped_length > 0) && (ped_length < this->m_table_length));
        uint64_t tab_term = this->m_table[ped_length];

        // Disable warning for switch on constant type for Windows.
#ifdef _WIN32    
#pragma warning(disable:280)
#endif
        // Fill in the table.  Call different methods based on RNG_TYPE.
        switch (RNG_TYPE) {
        case DotMixRNGType::DMIX_PRIME:
        case DotMixRNGType::F_DMIX_PRIME:
            DotMixUtil::update_mod_p_and_fill_buffer<NUM_MIX_ITER>(output_buffer,
                                                                   n,
                                                                   prefix_result,
                                                                   tab_term);
            break;

        case DotMixRNGType::DMIX:
        case DotMixRNGType::F_DMIX:
        default:
            DotMixUtil::update_and_fill_buffer<NUM_MIX_ITER>(output_buffer,
                                                             n,
                                                             prefix_result,
                                                             tab_term);
        }
#ifdef _WIN32    
#pragma warning(enable:280)
#endif
    }

    
};  // namespace cilkpub

#endif // !defined(__DOTMIX_GENERIC_H_)
