/* detred_iview.h                 -*-C++-*-
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

#ifndef __DETRED_VIEW_H_
#define __DETRED_VIEW_H_

#include <stdint.h>

#include <vector>

#include <cilk/cilk_api.h>
#include <cilkpub/pedigrees.h>
#include <cilkpub/rank_range_tag.h>
#include <cilkpub/detred_range_groups.h>

namespace cilkpub {

    // The internal view (Iview) of a deterministic reducer
    // is a collection of primitive values (of type T) for a given
    // range of pedigrees.
    //  
    // More specifically, an Iview is represented by a pedigree range
    //  [start, active]
    // 
    // The "start" pedigree of the Iview is the "lefmost" or "first"
    // pedigree in the range, i.e., the pedigree of the first update
    // to this Iview.
    //
    // The "active" pedigree of the Iview is the "rightmost" pedigree
    // in the range so far.
    //
    
    template <typename T>
    class DetRedIview {

    public:
        // Default constructor --- create an empty (identity) view.
        // Not really useful unless you want an array of these
        // these views.
        DetRedIview();

        // Construct with a given initial value, at the CURRENT
        // pedigree.
        DetRedIview(const T& v);

        // Construct with a given initial value, at the specified
        // pedigree.
        DetRedIview(const T& v, const pedigree& ped);

        // Default destructor.
        ~DetRedIview();
        
        // Methods that work on the entire view.
        
        // Get the value of this entire view.
        T get_value() const;

        // Advance the active pedigree to "ped", and then
        // update with value "v".
        //
        // We should have the active pedigree <= ped.
        void update_with_pedigree(const pedigree& ped, const T& v);
        
        // Advance active pedigree to the current pedigree,
        // and then update with value "v".
        //
        // We should have the active pedigree <= ped.
        void update_to_current_pedigree(const T& v);

        // Destructively merge the right view into this one.
        //
        // We should have this->get_active_pedigree() < right_view->start_pedigree().
        void merge(DetRedIview<T>* right_view);

        pedigree get_start_pedigree() const;       
        pedigree get_active_pedigree() const;


        void validate_value_range(int pedstack_idx) const;
        void validate(void) const;

        // Returns true if all the tags in the index stack are equal.
        bool tags_equal(const DetRedIview<T>& other) const;

        // Methods for counting the number of terms in the various
        // pedigrees.  These methods are "simple" to calculate from
        // the layout, but hard to remember.
        
        // The number of pedigree terms that start and active pedigree
        // have in common, EXCEPT for the deepest term.
        //
        // The deepest pedigree term is treated as "distinct" in this
        // count, because it is always stored on the right stack.
        inline int common_stem_length() const;
        
        // Return the number of terms in the starting pedigree.
        inline int start_pedigree_length() const;

        // Returns start_pedigree_length() - common_stem_length();
        inline int start_pedigree_distinct_terms() const;

        // Return the number of terms in the active pedigree.
        inline int active_pedigree_length() const;

        // Returns active_pedigree_length() - common_stem_length()
        inline int active_pedigree_distinct_terms() const;

        // Returns number of terms in common prefix of this pedigree
        // and the active pedigree.
        int active_pedigree_common_prefix_length(const pedigree& ped);
        
        /********************************************************/
        // Methods that work with the active range group.
        // 
        // NOTE: These methods probably won't see much use outside of
        // testing.

        /**
         * Returns the value of the active element in the active range
         * group.
         *
         * @return The value of the active element, or the identity
         *         T() if this view is empty.
         */
        T get_active_elem_val() const;

        /**
         * Returns the starting rank of the active range group.
         */
        uint64_t active_group_start_rank() const;

        /**
         * Returns the active rank of the active range group.
         */
        uint64_t active_group_active_rank() const;
        
        /**
         * Update the active range group with a value tagged with a
         * given rank.
         *
         * @param new_rank   The rank for this update.
         * @param v          The value to add to the view.
         */
        void update_active_range_group(uint64_t new_rank,
                                       const T& v);

        /**
         * Terminate the active range group, disallowing further
         * updates.
         *
         * This operation combines as many elements in the active
         * range group as possible, appending as many identity
         * elements on the end as necessary to keep combining.
         * Note that the group is not guaranteed to be combined into a
         * single element unless the starting rank in the group is 0.
         *
         * After terminating the group, no more updates should be made
         * to the group.
         */
        void terminate_active_range_group();
        
        /**
         * Merge the active range group from right_view into the
         * active range group of the current view.
         *
         * The active range groups of both views should be at the same
         * depth, and the active rank of this view's active range
         * group should be strictly less than the starting rank of the
         * right_view's active range group.
         *
         * @param right_view  The right view.
         */
        void merge_active_range_groups(DetRedIview<T>* right_view);
        

        /********************************************************/
        // Methods for adding and removing new ranges, i.e.,
        // for increasing and decreasing pedigree terms.

        // Pushes a new range of [0, new_rank] onto the right stack,
        // with initial value "v" at rank "new_rank".
        // 
        // The value at "new_rank" is not finished yet, i.e., the user
        // can still call update_leaf_range with "new_rank".
        void push_new_right_range(uint64_t new_rank, const T& v);

        // Terminate the current right range,
        // collapse it into one value, update the end of the
        // parent range with the collapsed value.
        // 
        // If advance_leaf_term is true, increment
        // the end of the parent range by 1.
        //
        // This method assumes:
        //
        // 1. The current leaf range is a right range.  In other words,
        //    it has a parent that is at index offset >= m_Roffset.  This
        //    condition means that the range we are popping is below the 
        //    common ancestor of the start and active pedigrees.
        // 
        //    For example, if start = <1, 2, 4, 8> and active = <1, 2, 6, 4, 3, 2>,
        //    we can pop ranges for 2, 3, and 4, but not 6.
        // 
        // 2. The current leaf range (which should be a right range)
        //    should begin with 0.
        void pop_and_merge_right_range(bool advance_leaf_term);

        // Terminates the common range (the range that is at the level
        // of the common ancestor of the start and active pedigrees).
        //
        // This operation pops the entire common range from the right
        // stack, pushes it onto the left stack.
        // 
        // This method also creates a new common range on the right
        // stack which is one greater than the pedigree term on the
        // stem on the right stack.
        void pop_and_move_common_range();
        
        // For example, suppose we begin with start pedigree = <1, 2, 4, 8>.
        //
        //  Active     Left:          Common Right   Right           Op 
        // <1,2,4,8>    --              1, 2, 4       [8]            Create
        // <1,2,4,9>    --              1, 2, 4       [8-9]          update_leaf_range(9)
        // <1,2,4,11>   --              1, 2, 4       [8-11]         update_leaf_range(11)
        // <1,2,5>     [8-11]           1, 2          [5]            pop_and_move_common_range()
        // <1,2,8>     [8-11]           1, 2          [5-8]          update_leaf_range(8)
        // <1,2,8,4>   [8-11]           1, 2          [5-8], [0-4]   push_new_right_range(4)
        // <1,2,9>     [8-11]           1, 2          [5-9]          pop_and_merge_right_range()
        // <1,3>       [8-11],[5-9]     1             [3]            pop_and_move_common_range()
        // <2>         [8-11],[5-9],[3]               [2]            pop_and_move_common_range()

        
    private:

        // Every view consists of three "stacks":
        //
        // Rstack:  The "active" stack of tagged values, containing all the
        //          intermediate ranges which match the active pedigree.
        //
        // Lstack: The "start" stack, which stores "older" pedigree
        //         ranges (those that are not along the active pedigree)
        //
        // index stack: The index into the value stack, which tells us how to split
        //              Lstack and Rstack into terms organized by pedigree depth.
        //
        typename std::vector<PedStackElem<T> > m_index_stack;  // Index for both Lstack and Rstack.
        typename std::vector<TaggedElem<T> >   m_Rstack;
        typename std::vector<TaggedElem<T> >   m_Lstack;
        int                                    m_Loffset;
        int                                    m_Coffset;   // Offsets into index stack where indices for Rstack begins.
        int                                    m_Roffset;

        T                                      m_initial_value;
                                               // The initial value the reducer is created with.
                                               // The leftmost update to this reducer view will
                                               // incorporate this value as part of its first update.
        
        // New arrangement: (on two stacks)
        // 
        //
        // ValueIdx  LeftStack (tag)   PedIdx  StartingOffset  PedParent
        //
        //   0          8-inf             0          0             L5
        //
        //   1          1                 1          1             L4
        //   2          2-inf
        //
        //   3          7                 2          3             C3   
        //   4          8-11
        //   5          12-inf
        //
        //              *                 3          *             *   <--  m_Loffset == 3
        //
        //              *                 4          *             *
        //
        //              *                 5          *             *
        //
        //                                6          *             *    


        // Idx      RightStack (tag)        
        //
        //   0          1                 7          0             C0   <--  m_Coffset == 7

        //   1          2                 8          1             C1

        //   2          3                 9          2             C2   <--  m_Roffset == 9
        //   3          4-7
        //   4          8
        //   5          9                  
        //
        //   6          0-3              10          6             C3
        //   7          4
        //
        //   8          0                11          8             R4
        //   9          1
        //
        //  10          0-1              12         10             R5
        //  11          2
        //  
        //  12          0-1              13         12             R6
        //  13          2
        //  14          3
        //                               14         15            NULL                                          


        const TaggedElem<T>* get_leaf_range_array(int* length_ptr) const;


        inline const TaggedElem<T>& last_tagged_elem() const;
        inline TaggedElem<T>& last_tagged_elem();

        // Returns the starting pedigree.
        // If "minus_one" is true, we get the pedigree immediately before the start.
        // (either subtract 1 from the last term, or chop off the last
        // term if it was 0).
        inline pedigree get_start_pedigree_helper(bool minus_one) const;

        inline void pop_and_move_common_range_helper(bool advance_pedigree);

        // Convert an index offset into a pedigree depth.
        inline int calc_ped_depth_from_idx_offset(int q) const;



        // Copy the ranges from other->m_Rstack and append to this
        // these ranges to this->m_Rstack.
        //
        // Copies all ranges k in interval [start_offset, stop_offset)
        // Range k consists of the elements in other->m_Rstack in the
        // interval
        //  [other->m_index_stack[k].starting_offset,
        //   other->m_index_stack[k+1].starting_offset)
        void append_right_ranges_to_Rstack(DetRedIview<T>* other,
                                           int start_offset,
                                           int stop_offset);
        
        // Copy the ranges from other->m_Lstack and append to this
        // these ranges to this->m_Lstack.
        //
        // Copies all ranges k in interval [start_offset, stop_offset)
        // Range k consists of the elements in other->m_Lstack in the
        // interval
        //  [other->m_index_stack[k].starting_offset,
        //   other->m_index_stack[k+1].starting_offset)
        //
        void append_left_ranges_to_Lstack(DetRedIview<T>* other,
                                          int start_offset,
                                          int stop_offset);

        void copy_left_range_to_Rstack(DetRedIview<T>* right_view,
                                       int right_l_offset);

        // Pops off the specified number of ranges from the rstack.
        // This rstack should have no common range.         
        void pop_common_prefix_ranges_from_Rstack(int num_ranges);



        // Initialize this view from a pedigree.
        void init_from_pedigree(const pedigree& ped,
                                const T& val,
                                bool called_from_constructor);

        // Private helper methods for merge().
        void merge_helper_update_to_right_active_ped(DetRedIview<T>* right_view);
        void merge_helper_merge_left_groups_from_right(DetRedIview<T>* right_view);
        void merge_helper_merge_right_groups_from_right(DetRedIview<T>* right_view);
        
        // Friend class that has access to internal state for
        // testing and debugging.
        friend class __test_DetRedIview;
    };
    
};  // namespace cilkpub


#include "detred_iview.cpp"

#endif // __DETRED_VIEW_H_
