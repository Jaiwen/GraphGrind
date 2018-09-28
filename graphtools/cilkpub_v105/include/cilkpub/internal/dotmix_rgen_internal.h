/* dotmix_rgen_internal.h                 -*-C++-*-
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

#ifndef __DOTMIX_RGEN_INTERNAL_H_
#define __DOTMIX_RGEN_INTERNAL_H_

/**
 * @file dotmix_rgen_internal.h
 *
 * @brief Internal implementation of methods in DotMix that vary based
 * on the DotMix RNG type.
 *
 * @author Tao B. Schardl
 * @author Jim Sukha
 */

namespace cilkpub {

    namespace internal {

        template <int NUM_MIX_ITER>
        class DotMixRgen {
        protected:
            // Get method that is called from DotMixGeneric's get.
            friend DotMixGeneric<DotMixRNGType::DMIX,         NUM_MIX_ITER>;
            friend DotMixGeneric<DotMixRNGType::DMIX_PRIME,   NUM_MIX_ITER>;
            friend DotMixGeneric<DotMixRNGType::F_DMIX,       NUM_MIX_ITER>;
            friend DotMixGeneric<DotMixRNGType::F_DMIX_PRIME, NUM_MIX_ITER>;
        
            /**
             * The @c get_compressed_pedigree method reads in the current
             * pedigree, and performs the computation to compress the
             * value into a single @c uint64_t, and then bumps the worker
             * rank.
             *
             * This method is a helper that is used by @c get and @c get_buffer.
             *
             * After this method completes, *ped_length is the length of
             * the pedigree we compressed. In terms of implementation, we
             * have used the first @c *ped_length entries of the table.
             */
            static inline uint64_t
            get_compressed_pedigree(DotMixGeneric<DotMixRNGType::DMIX,         NUM_MIX_ITER>& dprng,
                                    int* ped_length);
            static inline uint64_t
            get_compressed_pedigree(DotMixGeneric<DotMixRNGType::DMIX_PRIME,   NUM_MIX_ITER>& dprng,
                                    int* ped_length);
            static inline uint64_t
            get_compressed_pedigree(DotMixGeneric<DotMixRNGType::F_DMIX,       NUM_MIX_ITER>& dprng,
                                    int* ped_length);
            static inline uint64_t
            get_compressed_pedigree(DotMixGeneric<DotMixRNGType::F_DMIX_PRIME, NUM_MIX_ITER>& dprng,
                                    int* ped_length);

        }; // class DotMixRgen


        template <int NUM_MIX_ITER> 
        uint64_t
        DotMixRgen<NUM_MIX_ITER>::
        get_compressed_pedigree(DotMixGeneric<DotMixRNGType::DMIX, NUM_MIX_ITER>& dprng,
                                int* ped_length)
        {
            uint64_t result = dprng.m_X;
            int d = 0;

            __cilkrts_pedigree pednode = __cilkrts_get_pedigree();
            const __cilkrts_pedigree *ped = &pednode;
            const __cilkrts_pedigree stop_node = dprng.m_scope.get_stop_node();

            // Compute the reverse dot product.
            while ((NULL != ped) &&
                   (stop_node.parent != ped->parent) &&
                   (d < dprng.m_table_length)) {
                result += (ped->rank+1) * dprng.m_table[d];
                ped = ped->parent;
                d++;
            }

            // Die if we ran out of pedigree terms or table before finding
            // stop_node.parent.
            if (stop_node.parent != ped->parent) {
                if (NULL == ped) {
                    fprintf(stderr,
                            "ERROR: ran out of pedigree terms. ped=%p, stop_node.parent=%p.  Pedigree may be out of scope?\n",
                            ped,
                            stop_node.parent);
                }
                else {
                    fprintf(stderr,
                            "ERROR: pedigree too long. ped=%p, d=%d, m_table_length=%d\n",
                            ped,
                            d,
                            dprng.m_table_length);
                }
                assert(0);
            }

            assert(stop_node.parent == ped->parent);
            assert(stop_node.rank <= ped->rank);
            assert(d < dprng.m_table_length);

            // Handle the last term.
            // Correct for the scoped term, by subtracting the product off.
            result += ((ped->rank+1) - stop_node.rank) * dprng.m_table[d];
        
            // Increment worker rank.
            __cilkrts_bump_worker_rank();

            // For buffered get's, the next term we can use in the table
            // is at index d+1.
            *ped_length = d+1;
        
            return result;
        }

        template <int NUM_MIX_ITER> 
        uint64_t
        DotMixRgen<NUM_MIX_ITER>::
        get_compressed_pedigree(DotMixGeneric<DotMixRNGType::DMIX_PRIME, NUM_MIX_ITER>& dprng,
                                int* ped_length)
        {
            uint64_t result = dprng.m_X;
            int d = 0;

            __cilkrts_pedigree pednode = __cilkrts_get_pedigree();
            const __cilkrts_pedigree *ped = &pednode;
            const __cilkrts_pedigree stop_node = dprng.m_scope.get_stop_node();

            // 3 terms for storing the result.
            uint64_t tmp_result[3];
            tmp_result[:] = 0;
        
            // Walk up the pedigree and accumulate the reverse dot
            // product. 
            while ((NULL != ped) &&
                   (stop_node.parent != ped->parent) &&
                   (d < dprng.m_table_length)) {
                uint64_t ped_term = ped->rank+1;
                uint64_t tab_term = dprng.m_table[d];
                DotMixUtil::dotprod_update_mod_p(ped_term,
                                                 tab_term,
                                                 tmp_result);
                ped = ped->parent;
                d++;
            }

            // Die if we ran out of pedigree terms or table before finding
            // stop_node.parent.
            if (stop_node.parent != ped->parent) {
                if (NULL == ped) {
                    fprintf(stderr,
                            "ERROR: ran out of pedigree terms. ped=%p, stop_node.parent=%p.  Pedigree may be out of scope?\n",
                            ped,
                            stop_node.parent);
                }
                else {
                    fprintf(stderr,
                            "ERROR: pedigree too long. ped=%p, d=%d, m_table_length=%d\n",
                            ped,
                            d,
                            dprng.m_table_length);
                }
                assert(0);
            }

            // Handle the last term.  It should be corrected by the scope.
            assert(stop_node.parent == ped->parent);
            assert(stop_node.rank <= ped->rank);
            assert(d < dprng.m_table_length);

            uint64_t ped_term = ped->rank+1 - stop_node.rank;
            uint64_t tab_term = dprng.m_table[d];
            DotMixUtil::dotprod_update_mod_p(ped_term,
                                             tab_term,
                                             tmp_result);
            result = DotMixUtil::finalize_dotprod_mod_p(result,
                                                        tmp_result);
            // Increment worker rank.
            __cilkrts_bump_worker_rank();

            // For buffered get's, the next term we can use in the table
            // is at index d+1.
            *ped_length = d+1;

            return result;
        }


        // Forward DotMix.  Computes a dot product with the pedigree,
        // stored in forward order.  Requires us to extract the entire
        // pedigree before computing dot product.
    
        template <int NUM_MIX_ITER> 
        uint64_t
        DotMixRgen<NUM_MIX_ITER>::
        get_compressed_pedigree(DotMixGeneric<DotMixRNGType::F_DMIX, NUM_MIX_ITER>& dprng,
                                int* ped_length)
        {
            uint64_t result = dprng.m_X;
            // Grab the current pedigree.
            pedigree sped = pedigree::current(dprng.m_scope);
            int d = sped.length();
            assert(d < dprng.m_table_length);

            pedigree::const_iterator it = sped.begin();
            for (int i = 0; i < d; ++i) {
                uint64_t ped_term = (*(it + i)) + 1;
                result += ped_term * dprng.m_table[i];
            }
        
            __cilkrts_bump_worker_rank();

            // For buffered get's, the next term we can use in the table
            // is at index d.
            *ped_length = d;

            return result;
        }


        // Forward DotMix, with all computations executed mod p.
        template <int NUM_MIX_ITER> 
        uint64_t
        DotMixRgen<NUM_MIX_ITER>::
        get_compressed_pedigree(DotMixGeneric<DotMixRNGType::F_DMIX_PRIME, NUM_MIX_ITER>& dprng,
                                int* ped_length)
        {
            uint64_t result = dprng.m_X;

            // Grab the current pedigree.
            pedigree sped = pedigree::current(dprng.m_scope);
            int d = sped.length();
            assert(d < dprng.m_table_length);

            // Die for now if the table isn't large enough.  We should
            // really find a reasonable way to resize the table...
            if (d >= dprng.m_table_length) {
                fprintf(stderr, "ERROR: pedigree too long. Pedigree length=%d, m_table_length=%d\n",
                        d, dprng.m_table_length);
                assert(0);
            }

            // 3 terms for storing the result.
            uint64_t tmp_result[3];
            tmp_result[:] = 0;

            pedigree::const_iterator it = sped.begin();
            // Accumulate the terms of the dot product.
            for (int i = 0; i < d; ++i) {
                uint64_t ped_term = (*(it + i)) + 1;
                uint64_t tab_term = dprng.m_table[i];
                DotMixUtil::dotprod_update_mod_p(ped_term,
                                                 tab_term,
                                                 tmp_result);
            }

            result = DotMixUtil::finalize_dotprod_mod_p(result,
                                                        tmp_result);
            // Increment worker rank.
            __cilkrts_bump_worker_rank();

            // For buffered get's, the next term we can use in the table
            // is at index d.
            *ped_length = d;

            return result;
        }

    } // namespace internal
    
};  // namespace cilkpub

#endif // !defined(__DOTMIX_RGEN_INTERNAL_H_)
