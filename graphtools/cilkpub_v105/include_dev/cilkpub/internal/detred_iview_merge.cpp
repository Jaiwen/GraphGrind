/* detred_iview_merge.cpp                 -*-C++-*-
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
    void DetRedIview<T>::merge_active_range_groups(DetRedIview<T>* right_view) {
        int right_length;
        const TaggedElem<T>* right_elems = right_view->get_leaf_range_array(&right_length);
        int retval = TaggedElemStack<T>::merge_array_into_active_range_group(m_Rstack,
                                                                             m_index_stack,
                                                                             right_elems,
                                                                             right_length);
        CILKPUB_DETRED_DBG_ASSERT(1, retval == 0);
    }



    template <typename T>
    void DetRedIview<T>::merge_helper_update_to_right_active_ped(DetRedIview<T>* right_view)
    {
        // Stage 1: update this view to the pedigree immediately
        // before the start pedigree of right_view.
        pedigree rstart_actual = right_view->get_start_pedigree_helper(false);
        
    #if CILKPUB_DBG_LEVEL(3)
        pedigree my_active_ped = get_active_pedigree();
        __test_DetRedIview::tfprint_iview<T>(stderr, *this);
        fprintf(stderr, "Starting merge: my_active_ped is ");
        my_active_ped.fprint(stderr, "\n");
        fprintf(stderr, "rstart actual is ");
        rstart_actual.fprint(stderr, "\n");
        CILKPUB_DETRED_DBG_ASSERT(1, my_active_ped < rstart_actual);
    #endif
        
        // Update this range to the pedigree immediately before the
        // start of right_view.
        
        // WARNING: We should be careful about how we deal initial
        // values here when a range has empty pedigree.
        // 
        // Make sure the initial value is not automatically merged
        // into the 0 value and made the active element on the stack.
        // Otherwise, we'll accidentally pop the term off below.
        
        this->update_with_pedigree(rstart_actual, T());

    #if CILKPUB_DBG_LEVEL(3)
        fprintf(stderr, "Update to rstart_actual: this is ");
        __test_DetRedIview::tfprint_iview<T>(stderr, *this);
        fprintf(stderr, "\n");
    #endif

        int popped_deepest_term;
        // Get rid of the last term.  This term should be an empty
        // leaf.  (If not, we have other problems...)  Remember if we
        // popped off the deepest term in the active pedigree in doing
        // this correction.
        CILKPUB_DETRED_DBG_ASSERT(2, this->last_tagged_elem().val == T());
        CILKPUB_DETRED_DBG_ASSERT(2, this->last_tagged_elem().tag.is_leaf());
        popped_deepest_term = TaggedElemStack<T>::pop_active_elem(this->m_Rstack,
                                                                  this->m_index_stack);
    #if CILKPUB_DBG_LEVEL(3)
        fprintf(stderr, "Leaf popped. Now we have: \n");
        __test_DetRedIview::tfprint_iview<T>(stderr, *this);
        fprintf(stderr, "\n");
        fprintf(stderr, "Right view is: \n");
        __test_DetRedIview::tfprint_iview<T>(stderr, *right_view);
        fprintf(stderr, "\n");
    #endif
    }


    template <typename T>
    void DetRedIview<T>::merge_helper_merge_left_groups_from_right(DetRedIview<T>* right_view)
    {
        int active_distinct_terms = this->active_pedigree_distinct_terms();
        int left_ranges_processed = 0;

        // Only happens if right_view has left groups.
        if (right_view->m_Loffset > 0) {
            int left_depth = this->calc_ped_depth_from_idx_offset(this->m_index_stack.size()-2);
            int right_depth = right_view->calc_ped_depth_from_idx_offset(left_ranges_processed);

            // Left, step 1: 
            // If the initial depths don't line up, then the left
            // should be missing one term that the right has.  Copy
            // one left group over on the right stack over and append
            // to this rstack.
            if (left_depth != right_depth) {
                CILKPUB_DBG_PRINTF(3,
                                   "Left, step 1. left_ranges_processed = %d\n",
                                   left_ranges_processed);
                CILKPUB_DETRED_DBG_ASSERT(0, (left_depth+1) == right_depth);
                
                this->copy_left_range_to_Rstack(right_view, 0);
                active_distinct_terms++;

                // Yes, active_distinct_terms can be -1 before the
                // increment.
                if (active_distinct_terms > 0) {
                    this->pop_and_merge_right_range(false);
                }
                else {
                    CILKPUB_DETRED_DBG_ASSERT(0, 0 == active_distinct_terms);
                    this->pop_and_move_common_range_helper(false);
                }
                left_ranges_processed++;

            #if CILKPUB_DBG_LEVEL(3)
                fprintf(stderr, "Left view after step 1:");
                __test_DetRedIview::tfprint_iview<T>(stderr, *this);
                fprintf(stderr, "\n");
                fprintf(stderr, "Right view after step 1:");
                __test_DetRedIview::tfprint_iview<T>(stderr, *right_view);
                fprintf(stderr, "\n");
            #endif
            }

            // Left, step 2:
            // Merge left groups with right groups on this stack. 
            int stop_boundary = right_view->m_Loffset;
            if (stop_boundary > active_distinct_terms) {
                stop_boundary = active_distinct_terms;
            }
            while (left_ranges_processed < stop_boundary) {
                int x = left_ranges_processed;
                int s = right_view->m_index_stack[x].starting_offset;
                int t = right_view->m_index_stack[x+1].starting_offset;
                TaggedElem<T>* current_rv_array = &right_view->m_Lstack[s];

                CILKPUB_DETRED_DBG_ASSERT(1, (t-s) >= 1);
                CILKPUB_DBG_PRINTF(3,
                                   "Left, step 2. left_ranges_processed = %d. t-s=%d\n",
                                   left_ranges_processed, t-s);

                TaggedElemStack<T>::merge_array_into_active_range_group(m_Rstack,
                                                                        m_index_stack,
                                                                        current_rv_array,
                                                                        t-s);
                this->pop_and_merge_right_range(false);
                left_ranges_processed++;
            #if CILKPUB_DBG_LEVEL(3)
                fprintf(stderr, "Left view after step 2, left_ranges_processed=%d:", left_ranges_processed);
                __test_DetRedIview::tfprint_iview<T>(stderr, *this);
                fprintf(stderr, "\n");
                fprintf(stderr, "Right view after step 2, left_ranges_processed=%d", left_ranges_processed);
                __test_DetRedIview::tfprint_iview<T>(stderr, *right_view);
                fprintf(stderr, "\n");
            #endif
            }

            // Left step 3: If there is a common range, deal with it.
            if ((left_ranges_processed < right_view->m_Loffset) &&
                (left_ranges_processed == active_distinct_terms)) {
                CILKPUB_DBG_PRINTF(3,
                                   "Left, starting step 3. left_ranges_processed = %d, active_distinct_terms=%d\n",
                                   left_ranges_processed, active_distinct_terms);
            #if CILKPUB_DBG_LEVEL(3)
                fprintf(stderr, "Left view startnig step 3, left_ranges_processed=%d:", left_ranges_processed);
                __test_DetRedIview::tfprint_iview<T>(stderr, *this);
                fprintf(stderr, "\n");
                fprintf(stderr, "Right view after step 2, left_ranges_processed=%d", left_ranges_processed);
                __test_DetRedIview::tfprint_iview<T>(stderr, *right_view);
                fprintf(stderr, "\n");
            #endif

                CILKPUB_DETRED_DBG_ASSERT(1, left_ranges_processed == active_distinct_terms);
                int x = left_ranges_processed;
                int s = right_view->m_index_stack[x].starting_offset;
                int t = right_view->m_index_stack[x+1].starting_offset;
                TaggedElem<T>* current_rv_array = &right_view->m_Lstack[s];
                CILKPUB_DETRED_DBG_ASSERT(1, (t-s) >=1);
                CILKPUB_DETRED_DBG_ASSERT(1, (last_tagged_elem().tag.end() + 1) == current_rv_array[0].tag.start());
                TaggedElemStack<T>::merge_array_into_active_range_group(m_Rstack,
                                                                        m_index_stack,
                                                                        current_rv_array,
                                                                        t-s);
                this->pop_and_move_common_range_helper(false);
                left_ranges_processed++;

            #if CILKPUB_DBG_LEVEL(3)
                fprintf(stderr, "Left view after step 3, left_ranges_processed=%d:", left_ranges_processed);
                __test_DetRedIview::tfprint_iview<T>(stderr, *this);
                fprintf(stderr, "\n");
                fprintf(stderr, "Right view after step 3, left_ranges_processed=%d", left_ranges_processed);
                __test_DetRedIview::tfprint_iview<T>(stderr, *right_view);
                fprintf(stderr, "\n");
            #endif
            }

            // Left, step 4: copy over any remaining right ranges.
            if (left_ranges_processed < right_view->m_Loffset) {

            #if CILKPUB_DBG_LEVEL(3)
                fprintf(stderr, "Left step 4: left_ranges_processed=%d, active_distinct_terms=%d, left_depth=%d\n",
                        left_ranges_processed,
                        active_distinct_terms,
                        left_depth);
                fprintf(stderr, "Starting with left: \n");
                __test_DetRedIview::tfprint_iview<T>(stderr, *this);
                fprintf(stderr, "\n");
            #endif

                // Nothing left on this right stack.  Transfer the
                // range directly from right's m_Lstack to our
                // m_Lstack and pop off the common prefix term.
                this->append_left_ranges_to_Lstack(right_view,
                                                   left_ranges_processed,
                                                   right_view->m_Loffset);
                int num_ranges_copied = right_view->m_Loffset - left_ranges_processed;
                this->pop_common_prefix_ranges_from_Rstack(num_ranges_copied);

            #if CILKPUB_DBG_LEVEL(3)
                fprintf(stderr, "After step 4:\n");
                __test_DetRedIview::tfprint_iview<T>(stderr, *this);
                fprintf(stderr, "\n");
            #endif
            }
        }

#if CILKPUB_DBG_LEVEL(3)
        CILKPUB_DBG_PRINTF(3, "AFTER left ranges processed %d: \n", left_ranges_processed);
        __test_DetRedIview::tfprint_iview<T>(stderr, *this);
        CILKPUB_DBG_PRINTF(3, "\n");
#endif
    }

    template <typename T>
    void DetRedIview<T>::merge_helper_merge_right_groups_from_right(DetRedIview<T>* right_view)
    {
        // Stage 3: Now move the right stack over.
        // 
        // If we didn't pop the common range on this->m_Rstack in the
        // previous two stages, then we should merge the top right
        // range in right_view's stack into the leaf range on
        // this->m_Rstack, and append the remaining right ranges onto
        // this stack.
        //
        // Otherwise, if the common range has already been reached,
        // then we just need to append all the right ranges onto this
        // stack.
        int next_offset = right_view->m_Roffset;
        int left_offset = this->calc_ped_depth_from_idx_offset(this->m_index_stack.size()-2);
        int right_offset = right_view->calc_ped_depth_from_idx_offset(right_view->m_Roffset);

        CILKPUB_DBG_PRINTF(4, "HERE: left_offset = %d.  right_offset = %d.  righT_view->m_Roffset is %d\n",
                           left_offset, right_offset, right_view->m_Roffset);

        if (left_offset == right_offset)
        {
            // Merge into the common range on this stack.
            int top_s = right_view->m_index_stack[right_view->m_Roffset].starting_offset;
            int top_t = right_view->m_index_stack[right_view->m_Roffset+1].starting_offset;
            TaggedElem<T>* top_rv_right_array = &right_view->m_Rstack[top_s];

            TaggedElemStack<T>::merge_array_into_active_range_group(m_Rstack,
                                                                    m_index_stack,
                                                                    top_rv_right_array,
                                                                    top_t - top_s);
            next_offset++;
        }
        else {
            // If depths don't line up, we should only be off by one.
            CILKPUB_DETRED_DBG_ASSERT(1, left_offset+1 == right_offset);
        }

        // Now copy over all the remaining ranges on the right view's
        // right stack into this view's right stack.
        this->append_right_ranges_to_Rstack(right_view,
                                            next_offset,
                                            right_view->m_index_stack.size() - 1);
    }
        
    
    template <typename T>
    void DetRedIview<T>::merge(DetRedIview<T>* right_view)
    {
        this->m_initial_value += right_view->m_initial_value;

        // Stage 1: update this view to the pedigree immediately
        // before the start pedigree of right_view.
        merge_helper_update_to_right_active_ped(right_view);
        
        // Stage 2: Merge left groups from right_view into our stack.
        merge_helper_merge_left_groups_from_right(right_view);
        
        // Stage 3: Now move the right stack over.
        merge_helper_merge_right_groups_from_right(right_view);

#if CILKPUB_DBG_LEVEL(3)
        CILKPUB_DBG_PRINTF(3, "FINAL VIEW\n");
        __test_DetRedIview::tfprint_iview<T>(stderr, *this);
        CILKPUB_DBG_PRINTF(3, "\n");
        this->validate();
#endif
    }
    
}; // namespace cilkpub
