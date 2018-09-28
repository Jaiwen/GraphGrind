/* detred_range_groups.cpp                  -*-C++-*-
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

namespace cilkpub {
    
    template <typename T>
    void TaggedElemStack<T>::push_active_elem(std::vector<TaggedElem<T> >& value_stack,
                                            std::vector<PedStackElem<T> >& idx_stack,
                                            const TaggedElem<T>& telem)
    {
        value_stack.push_back(telem);
        idx_stack.back().starting_offset++;
    }


    template <typename T>
    int TaggedElemStack<T>::pop_active_elem(std::vector<TaggedElem<T> >& value_stack,
                                            std::vector<PedStackElem<T> >& idx_stack)                                            {
        int s = idx_stack.size() - 1;
        // Sanity-checks on the last element.
        assert(value_stack.size() >= 1);
        assert(s >= 0);     

        // Pop the last leaf element.
        value_stack.pop_back();
        idx_stack[s].starting_offset--;

        // Pop the index if this last group is now empty.
        if ((s >= 1) &&
            (idx_stack[s-1].starting_offset >= idx_stack[s].starting_offset)) {
            idx_stack.pop_back();
            return 1;
        }
        return 0;
    }                                       

    template <typename T>
    bool TaggedElemStack<T>::combine_active_elem(std::vector<TaggedElem<T> >& value_stack,
                                                 std::vector<PedStackElem<T> >& idx_stack)
    {
        int s = idx_stack.size()-1;
        // Check that we have at least one group and two elements in
        // this group.
        if ((s >= 1) &&
            ((idx_stack[s].starting_offset - idx_stack[s-1].starting_offset) >= 2)) {

            // If yes, check whether the last two elements are
            // siblings.
            int* size_ptr = &(idx_stack[s].starting_offset);
            if (value_stack[*size_ptr - 2].tag.is_left_sibling_of(value_stack[*size_ptr -1].tag)) {
                // Merge the last value into the one before it.
                value_stack[*size_ptr - 2].val += value_stack[*size_ptr - 1].val;
                value_stack[*size_ptr - 2].tag.change_to_parent();
                value_stack.pop_back();
                // Also update the starting offset in the group.
                (*size_ptr)--;
                return true;
            }
        }
        return false;
    }

    template <typename T>
    bool TaggedElemStack<T>::combine_in_active_range_group(std::vector<TaggedElem<T> >& value_stack,
                                                      std::vector<PedStackElem<T> >& idx_stack)
    {
        bool has_changed = false;
        int s = idx_stack.size()-1;
        // Check that we have at least one group and two elements in
        // this group.
        if (s >= 1) {
            int* size_ptr = &(idx_stack[s].starting_offset);
            int left_boundary = idx_stack[s-1].starting_offset;

            while (((*size_ptr - 1) > left_boundary) &&
                   (value_stack[*size_ptr-2].tag.is_left_sibling_of(value_stack[*size_ptr-1].tag))) {
                // Merge the last value into the one before it.
                value_stack[*size_ptr - 2].val += value_stack[*size_ptr - 1].val;
                value_stack[*size_ptr - 2].tag.change_to_parent();

                // Get rid of the last element we just merged.
                value_stack.pop_back();
                (*size_ptr)--;
                has_changed = true;
            }
        }
        return has_changed;
    }

    template <typename T>
    void TaggedElemStack<T>::terminate_active_range_group(std::vector<TaggedElem<T> >& value_stack,
                                                          std::vector<PedStackElem<T> >& idx_stack)
    {
        bool has_changed = false;
        do {
            has_changed = false;

            // If the last element is already a node with start_tag 0,
            // there is nothing to do.
            if (value_stack.back().tag.start() > 0) {
                // Otherwise, if the last element is right child, keep
                // merging with an implicit empty right sibling until
                // we become a right child.
                while (value_stack.back().tag.is_left_child()) {
                    value_stack.back().tag.change_to_parent();
                    has_changed = true;
                }
                
                // Now merge ourselves with the node to the left if
                // possible.
                has_changed = has_changed | combine_in_active_range_group(value_stack,
                                                                     idx_stack);
            }
        } while (has_changed);
    }


    template <typename T>
    bool TaggedElemStack<T>::push_active_elem_and_combine_in_active_range_group(std::vector<TaggedElem<T> >& value_stack,
                                                                         std::vector<PedStackElem<T> >& idx_stack,
                                                                         const TaggedElem<T>& telem)
    {
        int left_boundary = idx_stack[idx_stack.size() -2].starting_offset;
        int* size_ptr = &(idx_stack[idx_stack.size() - 1].starting_offset);

        if (((*size_ptr - 1) >= left_boundary) &&
            (value_stack[*size_ptr-1].tag.is_left_sibling_of(telem.tag))) {
            // If there is a match, just merge.
            value_stack[*size_ptr-1].val += telem.val;
            value_stack[*size_ptr-1].tag.change_to_parent();

            // Keep merging.
            TaggedElemStack<T>::combine_in_active_range_group(value_stack, idx_stack);
            return true;
        }
        else {
            // Otherwise, just push a new element.
            value_stack.push_back(telem);
            idx_stack[idx_stack.size()-1].starting_offset++;
            return false;
        }
    }




    // Advance the leaf range of this pedigree to have
    // end() of the rightmost tagged element >= rightmost_rank.
    template <typename T>
    void TaggedElemStack<T>::advance_active_range_group(std::vector<TaggedElem<T> >& value_stack,
                                                        std::vector<PedStackElem<T> >& idx_stack,
                                                        uint64_t rightmost_rank)
    {
#if DETRED_DEBUG >= 3        
        fprintf(stderr, "Calling advance leaf_val: rightmost_rank = %lu\n", rightmost_rank);
#endif
        rank_range_tag last_tag = value_stack.back().tag;
        if (last_tag.end() > rightmost_rank) {
            // If the last tag is already further than rightmost rank,
            // we should be done already.
            return;
        }

        // If the last rank is <= rightmost rank, then it is safe to
        // merge, because we are definitely finishing last_rank.
        //
        // Note that merging never changes the endpoint of the last
        // tag.
        TaggedElemStack<T>::combine_in_active_range_group(value_stack, idx_stack);
        
        if (last_tag.end() == rightmost_rank) {
            // If the last_tag ended exactly on the correct rank, we
            // are done.
            return;
        }

        // Otherwise, the last rank is strictly less than
        // rightmost_rank.

        // Read the last value again in case it changed.
        last_tag = value_stack.back().tag;

        rank_range_tag prev_tag = last_tag;
        rank_range_tag ances_tag = last_tag.parent();

        while (ances_tag.end() <= rightmost_rank)  {
            // Print / check loop variables.

#if DETRED_DEBUG >= 4            
            fprintf(stderr, "First loop: last_tag  = ");
            last_tag.fprint(stderr);
            fprintf(stderr, ", prev_tag = ");
            prev_tag.fprint(stderr);
            fprintf(stderr, ", ances_tag = ");
            ances_tag.fprint(stderr);
            fprintf(stderr, "\n");
#endif
            if (prev_tag.is_left_child()) {
                // If the element at the end of the list matches the
                // nodes we are looking at, we want to merge the tag
                // at end of the list with an empty right sibling.
                // 
                // Do that by replacing changing the last element's
                // tag to the parent tag.
                if (last_tag == prev_tag) {
                    value_stack.back().tag = ances_tag;
                    last_tag = ances_tag;
//                    fprintf(stderr, "CASE 1\n");
                }
                else {
                    // prev_tag was a left child, but the end of the
                    // list is something else.  Then, add an empty
                    // right sibling of prev_tag.
                    //
                    // This case happens when the end of the list is a
                    // node that is waiting on results that fall
                    // outside this range.
                    //
                    // For example, suppose this range begins with
                    // last_tag == (9, 0).  We can walk up to
                    // prev_tag == (8, 1), ances = (8, 2).  prev_tag
                    // is a left child, and we need to add an empty
                    // subtree (10, 1),
                    // Create a new identity for the right subtree.
                    // Push it on to the end of the list.

                    TaggedElemStack<T>::push_active_elem(value_stack,
                                                       idx_stack,
                                                       TaggedElem<T>(T(),
                                                                     ances_tag.right_child()));
                    last_tag = value_stack.back().tag;
                }
            }
            else {
                assert(prev_tag.is_right_child());
                if (last_tag == prev_tag) {
                    // Right child is at the end of the list, and matches.
                    // We might have to merge this node with the previous
                    // element in the list.
                    if (TaggedElemStack<T>::combine_active_elem(value_stack,
                                                                idx_stack)) {

                        assert(value_stack.back().tag == ances_tag);
                        last_tag = value_stack.back().tag;
                    }
                    else {
                        
                    }
                }
            }
            prev_tag = ances_tag;
            ances_tag = prev_tag.parent();
        }

        // Stage 2. Walk back down.

        // Now we have ances_tag is the LCA element.
        // We are going to walk down the right side of the tree. 
        // We had better have walked up from the left side.
        assert(prev_tag.is_left_child_of(ances_tag));
        assert(ances_tag.contains_rank(rightmost_rank));
        assert(!ances_tag.is_leaf());
        last_tag = ances_tag.right_child();

        while (last_tag.start() <= rightmost_rank) {
#if DETRED_DEBUG >= 4            
            fprintf(stderr, "SECOND Loop: last_tag is ");
            last_tag.fprint(stderr);
#endif
            assert(!last_tag.is_leaf());
            rank_range_tag left_subtree = last_tag.left_child();
            last_tag = last_tag.right_child();
            if (left_subtree.end() <= rightmost_rank) {
                // Push left subtree.
                TaggedElemStack<T>::push_active_elem(value_stack, idx_stack,
                                                   TaggedElem<T>(T(), left_subtree));
            }
            else {
                last_tag = left_subtree;
            }
        }

        assert(value_stack.back().tag.end() >= rightmost_rank);
#if DETRED_DEBUG >= 2       
        fprintf(stderr, "Finish advance leaf_val: rightmost_rank = %lu\n", rightmost_rank);
#endif        
    }


    template <typename T>
    int TaggedElemStack<T>::
    merge_array_into_active_range_group(std::vector<TaggedElem<T> >& value_stack,
                                        std::vector<PedStackElem<T> >& idx_stack,
                                        const TaggedElem<T>* right_elems,
                                        int right_length)
    {
        if (right_length <= 0)
            return 1;

        uint64_t right_min_rank = right_elems[0].tag.start();
        uint64_t right_max_rank = right_elems[right_length-1].tag.end();
        assert(right_min_rank > 0);

        if (value_stack.back().tag.end() < right_min_rank)  {
            // Updating my (left) range so that it is properly padded
            // with enough zeros.
            // TBD: Is there a more efficient way to this operation?
            uint64_t prev_rank = right_min_rank - 1;
            TaggedElemStack<T>::advance_active_range_group(value_stack,
                                                           idx_stack,
                                                           prev_rank);

            // We should already be merged as much as possible on the
            // left.
            assert(!TaggedElemStack<T>::combine_in_active_range_group(value_stack,
                                                                      idx_stack));

            int q = 0;
            // Push over elements until we stop merging.
            while (q < right_length-1) {
                bool did_merge =
                    TaggedElemStack<T>::
                    push_active_elem_and_combine_in_active_range_group(value_stack,
                                                                       idx_stack,
                                                                       right_elems[q]);
                q++;
                if (!did_merge) {
                    break;
                }
            }          

            // From this point on, just move the remaining ranges over
            // left.  This includes the last element, which must be
            // moved over.
            while (q <= right_length-1) {
                TaggedElemStack<T>::push_active_elem(value_stack,
                                                     idx_stack,
                                                     right_elems[q]);
                q++;
            }
            assert(q == right_length);
            return 0;                
        }
        else {
            int s = idx_stack[idx_stack.size() - 2].starting_offset;
            uint64_t min_rank = value_stack[s].tag.start();
            fprintf(stderr, "Error: merging left_range = (%llu, %llu), right = (%llu, %llu)\n",
                    min_rank, 
                    value_stack.back().tag.end(), 
                    right_min_rank, right_max_rank);
            return 1;
        }
    }



    /****************************************************************/
    // Methods for copying ranges between indexed stacks.

    template <typename T>
    int
    TaggedElemStack<T>::append_groups_and_append_index(std::vector<TaggedElem<T> >& value_stack,
                                                       std::vector<PedStackElem<T> >& idx_stack,
                                                       int starting_idx,
                                                       const std::vector<TaggedElem<T> >& source_val_stack,
                                                       const std::vector<PedStackElem<T> >& source_idx_stack,
                                                       int source_start,
                                                       int source_stop)
    {
        // First, copy over the indices from
        // right_view->m_index_stack.
        int current_rstack_size = (int)value_stack.size();
        assert(idx_stack[starting_idx+1].starting_offset == value_stack.size());
        for (int right_q = source_start;
             right_q < source_stop;
             right_q++) {
            current_rstack_size += (source_idx_stack[right_q+1].starting_offset
                                    - source_idx_stack[right_q].starting_offset);
            idx_stack.push_back(source_idx_stack[right_q]);
            idx_stack.back().starting_offset = current_rstack_size;
            starting_idx++;
        }
        
        // Now copy over the actual range values.  These can be copied
        // without modification.
        int rs = source_idx_stack[source_start].starting_offset;
        int rt = source_idx_stack[source_stop].starting_offset;
        value_stack.insert(value_stack.end(),
                           source_val_stack.begin() + rs,
                           source_val_stack.begin() + rt);

        return starting_idx;
    }
    
    template <typename T>
    int
    TaggedElemStack<T>::append_groups_and_overwrite_index(std::vector<TaggedElem<T> >& value_stack,
                                                          std::vector<PedStackElem<T> >& idx_stack,
                                                          int starting_idx,
                                                          const std::vector<TaggedElem<T> >& source_val_stack,
                                                          const std::vector<PedStackElem<T> >& source_idx_stack,
                                                          int source_start,
                                                          int source_stop)
    {
        int current_lstack_size = (int)value_stack.size();
        assert(idx_stack[starting_idx].starting_offset  == current_lstack_size);

        for (int left_q = source_start; left_q < source_stop; left_q++)
        {
            // Copy over the index from source, and update the offset
            // accordingly.
            int s2 = source_idx_stack[left_q+1].starting_offset;
            int s1 = source_idx_stack[left_q].starting_offset;
            current_lstack_size += (s2 - s1);
            idx_stack[starting_idx + 1].starting_offset = current_lstack_size;
            starting_idx++;
        }

        // Now copy over the actual range values.  These can be copied
        // without modification.
        int ls = source_idx_stack[source_start].starting_offset;
        int lt = source_idx_stack[source_stop].starting_offset;
        value_stack.insert(value_stack.end(),
                           source_val_stack.begin() + ls,
                           source_val_stack.begin() + lt);
        return starting_idx;
    }
    
};  // namespace cilkpub
