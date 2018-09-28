/* dotmix.h                  -*-C++-*-
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
 * @file dotmix.h
 *
 * @brief DotMix, a deterministic parallel random-number generator (DPRNG) for Cilk.
 *
 * \defgroup DotMix DotMix DPRNG
 *
 * @author   Tao B. Schardl
 * @author   Jim Sukha
 * @version  1.0
 * @req      @ref Pedigrees

 *
 * DotMix generates pseudorandom numbers deterministically by hashing
 * pedigrees.  This implementation is derived from following paper:
 *
 *   -  <I>Deterministic parallel random-number generation for dynamic-multithreading platforms</I> 
 *      by Charles E. Leiserson, Tao B. Schardl, and Jim Sukha.
 *
 * See @ref pedigree_refs for more details.
 *
 * This module implements 4 different variants of DotMix:
 *  - \c DotMix             : Computes a dot product of the reverse pedigree with entries from a table of random values. 
 *  - \c DotMixPrime        : Same as DotMix, except all computations are performed mod p = 2^64 - 59. 
 *  - \c ForwardDotMix      : Same as DotMix, except the dot product uses the in-order pedigree. 
 *  - \c ForwardDotMixPrime : Same as ForwardDotMix, except all computations are performed mod p = 2^64 - 59. 
 *  .
 *
 * \c DotMix is expected to be the fastest variant, since a dot
 * product with the reverse pedigree can be computed without needing
 * to store the entire pedigree, and computations mod @c p are more
 * expensive than arithmetic mod 2^64.
 *
 * The results described in the paper refer to an implementation of \c
 * DotMixPrime, although thus far we have observed similar
 * performance results for all variants.
 *
 * @warning DotMix is *not* designed to provide randomness for
 * cryptography.  Do not use this random-number generator for any
 * applications such as cryptographic hashing that require strong
 * guarantees on randomness.
 *
 * For more information about DotMix and pedigrees, see @ref pedigree_refs.
 *
 *
 * @todo Test random-number quality for DotMix variants.
 * @todo Quantify more precisely some of the tradeoffs between quality and performance of the DotMix variants.  In particular some of
 *     these variants might be eliminated.
 * @todo Implement other deterministic RNGs based on pedigrees.
 *
 */

#ifndef __CILKPUB_DOTMIX_H_
#define __CILKPUB_DOTMIX_H_

#include <cilkpub/pedigrees.h>

namespace cilkpub {

    /**
     * @brief Enum for the variants of DotMix.
     */ 
    class DotMixRNGType {
    public:
        enum {
            DMIX,             ///< DotMix [Default version].  
            DMIX_PRIME,       ///< DotMix, but with all computations mod @c p
            F_DMIX,           ///< Forward DotMix.  
            F_DMIX_PRIME,     ///< Forward DotMix, but with all computations mod @c p.
        };
    };

    /**
     *@brief Utility methods for DotMix implementations.
     *
     * The methods in this class should be usable independent of
     * pedigrees.
     */
    class DotMixUtil {
    public:
        /**
         * @brief Fixed prime value: p = 2^64 - 59
         *
         * This value is the largest prime less than 2^64.
         */
        static const uint64_t PRIME = (uint64_t)(-59);

        /// 2^64 mod p
        static const uint64_t TWO_POW64_MOD_P = 59;

        /// Mask for low 32 bits of a @c uint64_t
        static const uint64_t LOW32_MASK = (uint64_t)(-1) >> 32;
        /// Swap the high and low 32 bits of @c x. 
        static inline uint64_t swap_halves(uint64_t x);

        /**
         *@brief RC6 Mixing of @c x.
         *
         * Mixes @c x for @c NUM_MIX_ITER iterations.
         */
        template <int NUM_MIX_ITER>
        static inline uint64_t mix(uint64_t x);

        /**
         *@brief RC6 Mixing of @c x, with all calculations performed mod @c p.
         *
         * Mixes @c x for @c NUM_MIX_ITER iterations.
         */
        template <int NUM_MIX_ITER>
        static inline
        uint64_t mix_mod_p(uint64_t x);

        /**
         * @brief Compute <tt>(a+b) mod p</tt>, where @c a, @c b in <tt>[0, p)</tt>.
         *
         * @pre We must have @c a in <tt>[0, p)</tt> and @c b in <tt>[0, p)</tt>
         */
        static inline
        uint64_t sum_mod_p(uint64_t a, uint64_t b);

        /**
         * @brief Partial computation of  <tt> result += a*x mod p </tt>.
         *
         * More precisely, consider the following intermediate 32-bit variables:
         *   - @c ah : The high 32 bits of @c a
         *   - @c al : The low 32 bits of  @c a
         *   - @c xh : The high 32 bits of @c x
         *   - @c xl : The low 32 bits of  @c x
         *   .
         *
         * Then, this method performs the following computation into 3 intermediate @c uint64_t values:
         * - <tt> result[0]</tt>:  <tt> (ah*xh) mod p </tt>
         * - <tt> result[1]</tt>:  <tt> (ah*xl + al*xh) mod p </tt>
         * - <tt> result[2]</tt>:  <tt> (al*xl) mod p </tt>
         * .
         *
         * These 3 intermediate values can be collapsed into a single
         * @c uint64_t using @c finalize_dotprod_update_mod_p.
         */ 
        static inline
        void dotprod_update_mod_p(uint64_t a,
                                  uint64_t x,
                                  uint64_t* result);

        /**
         * @brief Collapses a running sum mod @c p into a single
         * @c uint64_t value.
         *
         * @param res          Initial value.
         * @param tmp_result   Running sum of 3 intermediate @c uint64_t values, as computed by @c dotprod_update_mod_p.
         * @return  <tt>res + tmp_result</tt>
         *
         * @note The final result may be larger than @c p. 
         */
        static inline
        uint64_t finalize_dotprod_mod_p(uint64_t res,
                                        uint64_t* tmp_result);



        /**
         *@brief Fills a buffer with RC6 mixed values, from a common
         * prefix.
         *
         * @param output_buffer   The buffer to fill.
         * @param n               The size of the buffer.
         * @param prefix_result   The common prefix value to use.
         * @param table_term      The multiplier to use for individual terms.
         *
         * @pre @c output_buffer should be a buffer with space for @c n terms.
         * @post After completion, we have
         *   <tt> output_buffer[i] = mix(prefix_result + (i+1)*table_term) </tt>
         */
        template <int NUM_MIX_ITER>
        static 
        void update_and_fill_buffer(uint64_t* output_buffer,
                                    int n,
                                    uint64_t prefix_result,
                                    uint64_t table_term);

        /**
         *@brief Similar to @c update_and_fill_buffer, except the
         * addition is performed mod @c p. 
         *
         * @see update_and_fill_buffer()
         *
         * @param output_buffer   The buffer to fill.
         * @param n               The size of the buffer.
         * @param prefix_result   The common prefix value to use.
         * @param table_term      The multiplier to use for individual terms.
         *
         * @pre @c output_buffer should be a buffer with space for @c n terms.
         * @post After completion, we have
         *   <tt> output_buffer[i] = mix(prefix_result + (i+1)*table_term) </tt>,
         *   with the "+" operation being calculated mod p.
         */
        template <int NUM_MIX_ITER>
        static 
        void update_mod_p_and_fill_buffer(uint64_t *output_buffer,
                                          int n,
                                          uint64_t prefix_result,
                                          uint64_t table_term);
    };


    /// Internal namespace for Cilkpub implementation details.
    namespace internal {
        // Tell Doxygen to ignore undocumented functions here.

        /**
         * @brief Forward declaration of internal implementation of various
         *  methods in DotMix.  This class contains all the methods that
         *  vary by different RNG type.
         */
        template <int NUM_MIX_ITER=4>
        class DotMixRgen;
    }; // namespace internal

    /**
     *@brief The generic template for DotMix DPRNG.
     *
     *@param RNG_TYPE      The variant of DotMix (as defined by @c DotMixRNGType).
     *@param NUM_MIX_ITER  Number of mixing iterations to use.
     */
    template <int RNG_TYPE, int NUM_MIX_ITER=4>
    class DotMixGeneric {
    public:

        /**
         *@brief Constructs a DotMix generator at global scope and a
         * default seed.
         *
         * This constructor allocates a DotMix table.
         */
        DotMixGeneric();

        /**
         *@brief Constructs a DotMix generator at global scope and a
         * specified seed.
         *
         * This constructor allocates a DotMix table.
         */
        DotMixGeneric(uint64_t seed);

        /// Destructor.
        ~DotMixGeneric();

        /// Initialize by setting the seed and scope.
        inline void init(uint64_t seed,
                         pedigree_scope& scope);

        /**
         * @brief Initialize the seed only.
         *
         * Seeding the generator fills in the DotMix table.
         */
        inline void init_seed(uint64_t seed);

        /**
         * @brief Initialize the scope only.
         *
         * Setting the scope does not change the DotMix table.
         */
        inline void init_scope(const pedigree_scope& scope);

        /**
         * @brief Allow class implementing DotMix variants to access
         * generator state.
         */
        friend internal::DotMixRgen<NUM_MIX_ITER>;

        /**
         * @brief Get a random number by hashing the current pedigree.
         *
         * @return  A @c uint64_t chosen roughly uniformly from <tt>[0, 2^64-1)</tt>
         * @post    This method calls @c __cilkrts_bump_worker_rank(),
         * which increments the leaf term in the current pedigree.
         */
        inline uint64_t get();


        /**
         * @brief Fills @c buffer with @c n random numbers. 
         *
         * @param output_buffer  Buffer to fill with random numbers.
         * @param n              Number of elements to fill in the buffer.
         * @post    This method calls @c __cilkrts_bump_worker_rank(),
         * which increments the leaf term in the current pedigree.
         */
        inline void fill_buffer(uint64_t* output_buffer, int n);
        
            
    private:
        /**
         * @brief Maximum size of the DotMix table.
         *
         * If pedigrees have length greater than this value, we are
         * likely to run into spawn depth limits in the Cilk Plus
         * runtime anyway...
         */
        static const int MAX_TABLE_LENGTH = 1024;

        /// Buffer for table. 
        uint64_t* m_table;

        /// Table length.
        int m_table_length;

        /// Initial offset value. 
        uint64_t m_X;

        /// Seed for this generator.
        uint64_t m_seed;

        /// Scope for this generator.
        pedigree_scope m_scope;
    };
    
}; // namespace cilkpub

#include "internal/dotmix_internal.h"

namespace cilkpub {
    /// Convenience typedef for DotMix
    typedef DotMixGeneric<DotMixRNGType::DMIX>         DotMix;

    /// Convenience typedef for DotMix with calculations mod p.
    typedef DotMixGeneric<DotMixRNGType::DMIX_PRIME>   DotMixPrime;

    /// Convenience typedef for Forward DotMix.
    typedef DotMixGeneric<DotMixRNGType::F_DMIX>       ForwardDotMix;

    /// Convenience typedef for Forward DotMix with calculations mod p.
    typedef DotMixGeneric<DotMixRNGType::F_DMIX_PRIME> ForwardDotMixPrime;
};



#endif  // #define __CILKPUB_DOTMIX_H_
