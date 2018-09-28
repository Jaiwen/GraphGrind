/* detred_iview_update.cpp                 -*-C++-*-
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

#include "detred_debug.h"

namespace cilkpub {

    template <typename T>
    void DetRedIview<T>::update_active_range_group(uint64_t new_rank, const T& val)
    {

        CILKPUB_DBG_PRINTF(3, "Calling update leaf_val: new_rank = %lu\n", new_rank);
        rank_range_tag& last_tag = last_tagged_elem().tag;
        if (last_tag.end() == new_rank) {
            last_tagged_elem().val += val;
        }
        else {
            CILKPUB_DETRED_DBG_ASSERT(1, last_tag.end() < new_rank);

            // Move the ranges in the list up to "new_rank-1".
            // All these ranges can be fully collapsed. 
            TaggedElemStack<T>::advance_active_range_group(m_Rstack,
                                                           m_index_stack,
                                                           new_rank-1);

            #if CILKPUB_DBG_LEVEL(3)
            this->validate();
            #endif

            #if CILKPUB_DBG_LEVEL(4)
                CILKPUB_DBG_PRINTF(4, "After advance: \n");
                __test_DetRedIview::tfprint_iview(stderr, *this);
                CILKPUB_DBG_PRINTF(4, "\n");
            #endif            

            TaggedElemStack<T>::push_active_elem(m_Rstack,
                                                 m_index_stack,
                                                 TaggedElem<T>(val,
                                                               rank_range_tag(new_rank)));
        }
        CILKPUB_DBG_PRINTF(3, "Finishing update leaf_val: new_rank = %lu\n", new_rank);
    }
    

    template <typename T>
    void DetRedIview<T>::terminate_active_range_group() 
    {
        TaggedElemStack<T>::terminate_active_range_group(m_Rstack,
                                                         m_index_stack);
    }


    template <typename T>
    void DetRedIview<T>::update_with_pedigree(const pedigree& ped, const T& v) 
    {
    #if CILKPUB_DBG_LEVEL(3)
        CILKPUB_DBG_PRINTF(3, "Starting Update with pedigree: ped = ");
        ped.fprint(stderr, "\n");
        CILKPUB_DBG_PRINTF(3, "\nThis view: ");
        __test_DetRedIview::tfprint_iview(stderr, *this);
        CILKPUB_DBG_PRINTF(3, "\n");
    #endif
        
        if (this->m_index_stack.size() <= 1) {
            // Special case of this view starting with an empty
            // pedigree.  Initialize as though we were constructing
            // this object.
            init_from_pedigree(ped, v, false);
            return;
        }
        
        pedigree active_ped = get_active_pedigree();
        int common_prefix_terms = this->active_pedigree_common_prefix_length(ped);
        
    #if CILKPUB_DBG_LEVEL(3)
        CILKPUB_DBG_PRINTF(3, ", \nActive pedigree: ");
        active_ped.fprint(stderr, "\n");
        CILKPUB_DBG_PRINTF(3, "\n");
        CILKPUB_DBG_PRINTF(3, "Update pedigree: ");
        ped.fprint(stderr, "\n");
        CILKPUB_DBG_PRINTF(3, "\n");

        pedigree start_ped = get_start_pedigree();
        CILKPUB_DBG_PRINTF(3, "Start pedigree: ");
        start_ped.fprint(stderr, "\n");
        CILKPUB_DBG_PRINTF(3, "\n");
    #endif

        // Sanity check.
        // We can't update at a pedigree that is before our active one.
        CILKPUB_DETRED_DBG_ASSERT(2, ped >= active_ped);
        CILKPUB_DETRED_DBG_ASSERT(2, common_prefix_terms == ped.common_prefix_length(active_ped));

        // Count how many terms between ped and the active pedigree
        // differ.  We want to correct these terms on the active
        // pedigree before pushing additional terms on.
        int terms_differing_with_ped = this->active_pedigree_length() - common_prefix_terms;

        // Number of terms we can pop from right stack.
        int active_distinct_terms = this->active_pedigree_distinct_terms();
        int x = 0;

        // Pop terms on the right stack below the common range.
        while ((x < terms_differing_with_ped-1) &&
               (x < active_distinct_terms)) {
            pop_and_merge_right_range(true);
            x++;
        }
        // The remaining terms to pop must be above the common range.
        while (x < terms_differing_with_ped-1) {
            pop_and_move_common_range();
            x++;
        }

        // Remember the current rank on the leaf range.
        uint64_t last_rank = last_tagged_elem().tag.end();
        
        // Update the last term that differs between ped and active
        // pedigree.  Instead of popping this term we update it
        // instead, and increment the length of the common prefix.
        //
        // This update won't occur if the active pedigree is an
        // ancestor of ped (and thus terms_differing_with_ped == 0).
        if (x < terms_differing_with_ped) {
            last_rank = ped[common_prefix_terms];
            this->update_active_range_group(last_rank, T());
            common_prefix_terms++;
        }

        // At this point, the active pedigree is now an ancestor of
        // ped.  Push new terms onto the right stack until we match
        // ped.

        for (int new_idx = common_prefix_terms;
             new_idx < ped.length();
             ++new_idx)
        {
            this->push_new_right_range(ped[new_idx], T());
            last_rank = ped[new_idx];
        }

        // Sanity check: the rank of the active element had better
        // match the final rank of the pedigree.
        CILKPUB_DETRED_DBG_ASSERT(1, last_rank == ped[ped.length()-1]);
        CILKPUB_DETRED_DBG_ASSERT(1, last_tagged_elem().tag.start() == last_rank);

        // Finally, now that we've updated the active pedigree to
        // match "ped", update with the term we passed in.
        last_tagged_elem().val += v;
    }

};  // namespace cilkpub
