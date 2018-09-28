/* detred_range_groups.h                  -*-C++-*-
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

#ifndef __DETRED_RANGE_GROUPS_H_
#define __DETRED_RANGE_GROUPS_H_

#include <stdint.h>
#include <vector>
#include <cilk/cilk_api.h>
#include <cilkpub/rank_range_tag.h>


// Functions for manipulating an array of elements, each labeled with
// rank_range_tag.
//

namespace cilkpub {

    template <typename T>
    struct TaggedElem {
        T val;
        rank_range_tag tag;
        inline void fprint(FILE* f) const;

        TaggedElem(const T& v_, const rank_range_tag& tag_)
            : val(v_), tag(tag_) { }
    };

    template <typename T>
    struct PedStackElem {
        int starting_offset;
        const __cilkrts_pedigree* ped_parent;

        PedStackElem() : starting_offset(0), ped_parent(NULL)  { }

        PedStackElem(int starting_offset_,
                     const __cilkrts_pedigree* ped_parent_)
            : starting_offset(starting_offset_)
            , ped_parent(ped_parent_) { }
    };


    template <typename T>
    class TaggedElemStack {
        // This class manipulates a "tagged elem stack" --- a pair of
        // a "value stack" and "index stack".
        //
        // A value stack is a vector of TaggedElem<T> objects, with
        // consecutive elements partitioned to groups.
        //
        // The index stack is a corresponding vector of
        // PedStackElem<T> objects, with each object storing an index
        // into the the value stack for the beginning of each group.
        //
        // If the value stack contains K groups, the index stack
        // contains K+1 elements.  More precisely, 
        //   [index_stack[k].starting_offset, index_stack[k+1].starting_offset)
        // is the range of indices in value_stack for group k.
        //

    public:
        // Add a new tagged element TaggedElem(val, tag) to the end of
        // the value stack, and then updates the index stack
        // accordingly.
        // inline static
        // void push_new_elem(std::vector<TaggedElem<T> >& value_stack,
        //                    std::vector<PedStackElem<T> >& index_stack,
        //                    const T& val,
        //                    const rank_range_tag& tag);

        // Same as above, except shorthand.
        inline static
        void push_active_elem(std::vector<TaggedElem<T> > & value_stack,
                              std::vector<PedStackElem<T> >& index_stack,
                              const TaggedElem<T>& telem);

        // Pops the last element in the last group off of value_stack.
        // If that last group is now empty, also pops off the group.
        // 
        // Returns a nonzero value if a now-empty group was popped.
        // Otherwise, returns 0.
        inline static
        int pop_active_elem(std::vector<TaggedElem<T> >& value_stack,
                            std::vector<PedStackElem<T> >& idx_stack);


        // If the last group in this stack has at least two elements,
        // then try to combine the last element with the previous
        // element, and update the value stack and index accordingly.
        // 
        // Returns true if an element was combined, and false if
        // nothing changed.
        inline static
        bool combine_active_elem(std::vector<TaggedElem<T> >& value_stack,
                                 std::vector<PedStackElem<T> >& idx_stack);

        // Combines as many elements in the last group as possible.
        //
        // Equivalent to repeated invocations of "combine_last_elem",
        // until the last group either has one element left, or the
        // last element can not be combined with the previous one.
        //
        // Returns true if any combinations occurred, and false
        // otherwise.
        inline static
        bool combine_in_active_range_group(std::vector<TaggedElem<T> >& value_stack,
                                           std::vector<PedStackElem<T> >& idx_stack);

        /**
         * Terminate the active range group, disallowing further
         * updates.
         *
         * This operation combines as many elements in the active
         * range group as possible.  Unlike
         * "combine_last_range_group", this method appends as many
         * identity elements on the end as necessary to keep
         * combining.
         *
         * Note that the group is not guaranteed to be combined into a
         * single element unless the starting rank in the group is 0.
         *
         * After terminating the group, no more updates should be made
         * to the group.
         *
         * @param value_stack   The stack of values for all range groups.
         * @param idx_stack     The index separating the value stack into range groups.
         */
        inline static
        void terminate_active_range_group(std::vector<TaggedElem<T> >& value_stack,
                                          std::vector<PedStackElem<T> >& idx_stack);

        // Call to push_leaf_elem, followed by combine_last_group.
        inline static
        bool push_active_elem_and_combine_in_active_range_group(std::vector<TaggedElem<T> >& value_stack,
                                                                std::vector<PedStackElem<T> >& idx_stack,
                                                                const TaggedElem<T>& telem);

        // Advance the active rank of the last element in the leaf
        // group rightmost_rank, collapsing as many nodes as possible.
        // After calling this method, this view can only accept
        // updates at a rank > rightmost_rank.
        inline static
        void advance_active_range_group(std::vector<TaggedElem<T> >& value_stack,
                                        std::vector<PedStackElem<T> >& idx_stack,
                                        uint64_t rightmost_rank);

        // Merge the range grouprepresented by the array into the
        // current leaf group on the stack.  Returns 0 if merge was
        // successful, or 1 if there was an error.
        static 
        int merge_array_into_active_range_group(std::vector<TaggedElem<T> >& value_stack,
                                                std::vector<PedStackElem<T> >& idx_stack,
                                                const TaggedElem<T>* right_elems,
                                                int right_length);


        /****************************************************************/
        // Methods for copying ranges between indexed stacks.

        // Copy the groups in the range [source_start, source_stop)
        // from source_val_stack/source_idx_stack and add them to
        // value_stack/idx_stack.
        //
        // This method appends new values to value_stack, and new
        // groups to idx_stack.
        //
        // starting_idx is the index in idx_stack where we begin
        // adding index entries.
        // This method returns an index one past the last entry in
        // idx_stack that was modified.
        static 
        int append_groups_and_append_index(std::vector<TaggedElem<T> >& value_stack,
                                           std::vector<PedStackElem<T> >& idx_stack,
                                           int starting_idx,
                                           const std::vector<TaggedElem<T> >& source_val_stack,
                                           const std::vector<PedStackElem<T> >& source_idx_stack,
                                           int source_start,
                                           int source_stop);
        
        // Copy the groups in the range [source_start, source_stop)
        // from source_val_stack/source_idx_stack and add them to
        // value_stack/idx_stack.
        //
        // This method appends to value_stack, but will overwrite the
        // corresponding indices in "idx_stack". (It assumes enough
        // placeholders already exist for the ranges we are about to
        // add.)
        //
        // starting_idx is the index in idx_stack where we begin
        // overwriting index entries.
        //
        // This method returns an index one past the last entry in
        // idx_stack that was modified.
        static
        int append_groups_and_overwrite_index(std::vector<TaggedElem<T> >& value_stack,
                                              std::vector<PedStackElem<T> >& idx_stack,
                                              int starting_idx,
                                              const std::vector<TaggedElem<T> >& source_val_stack,
                                              const std::vector<PedStackElem<T> >& source_idx_stack,
                                              int source_start,
                                              int source_stop);
    };
    
};  // namespace cilkpub


#include "internal/detred_range_groups.cpp"

#endif
