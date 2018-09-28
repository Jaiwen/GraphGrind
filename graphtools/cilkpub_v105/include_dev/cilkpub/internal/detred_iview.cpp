/* detred_iview.cpp                 -*-C++-*-
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

#define DETRED_DEBUG 0

namespace cilkpub {
    
    template <typename T>
    const TaggedElem<T>& DetRedIview<T>::last_tagged_elem(void) const {
        return m_Rstack.back();
    }

    template <typename T>
    TaggedElem<T>& DetRedIview<T>::last_tagged_elem(void) {
        return m_Rstack.back();
    }

    // The number of pedigree terms that start and active pedigree
    // have in common, EXCEPT for the deepest term.
    //
    // The deepest pedigree term is treated as "distinct" in this
    // count, because it is always stored on the right stack.
    template <typename T>
    int DetRedIview<T>::common_stem_length() const
    {
        return m_Roffset - m_Coffset;
    }

    // Return the number of terms in the starting pedigree.
    template <typename T>
    int DetRedIview<T>::start_pedigree_length() const
    {
//       assert(m_Coffset-1 == (common_stem_length() + m_Loffset
//                              + ((m_index_stack.size() -1) > m_Roffset)));
//        assert(m_Coffset-1 == (common_stem_length() + m_Loffset) + 1);
       return m_Coffset - 1;
    }

    template <typename T>
    int DetRedIview<T>::start_pedigree_distinct_terms() const
    {
        return m_Loffset;
    }
   
    // Return the number of terms in the active pedigree.
    template <typename T>
    int DetRedIview<T>::active_pedigree_length() const
    {
        return m_index_stack.size()-1  - m_Coffset;

        // The conditional below is a correction for the case where
        // there are no terms on the right stack except in the common
        // prefix.  In that case, we've moved the active term to the
        // left side.
        //   ((m_index_stack.size() - 1) ==  m_Roffset);
    }

    // Return the number of terms in the active pedigree.
    template <typename T>
    int DetRedIview<T>::active_pedigree_distinct_terms() const
    {
        return m_index_stack.size()-1  - m_Roffset - 1;
    }


    template <typename T>
    int DetRedIview<T>::active_pedigree_common_prefix_length(const pedigree& ped)
    {
        int active_length = active_pedigree_length();
        int ped_length = ped.length();
        int common_length = 0;
        pedigree::const_iterator it = ped.begin();
        uint64_t active_term;
       
        for (int q = m_Coffset; q < m_Roffset; ++q)
        {
            int s = m_index_stack[q+1].starting_offset-1;
            assert(s < (int)m_Rstack.size());

            const rank_range_tag& ctag = m_Rstack[s].tag;
            assert(ctag.is_leaf());
            active_term = ctag.end();

            if ((it == ped.end()) || (active_term != *it)) {
                goto finish;
            }
            it++;
            common_length++;
        }

        for (int q = m_Roffset; q < (int)m_index_stack.size()-1; ++q) {
            int s = m_index_stack[q+1].starting_offset - 1;
            assert(s < (int)m_Rstack.size());
            const rank_range_tag& ctag = m_Rstack[s].tag;
            active_term = ctag.end();

            if ((it == ped.end()) || (active_term != *it)) {
                goto finish;
            }
            it++;
            common_length++;
        }
        
    finish:

        assert(common_length == ped.common_prefix_length(get_active_pedigree()));
        return common_length;
    }
    
    
    template <typename T>
    void TaggedElem<T>::fprint(FILE *f) const
    {
        fprintf(f, "{");
        this->tag.fprint(f);
        fprintf(f, ": ");
        tfprint(f, this->val);
        fprintf(f, "}");
    }


    // Convert an index offset into a pedigree depth.
    template <typename T>
    int DetRedIview<T>::calc_ped_depth_from_idx_offset(int q) const
    {
        if (q >= m_Coffset) {
            return q - m_Coffset;
        }
        else {
            return start_pedigree_length() -1  - q;
        }
    }
    
    template <typename T>
    DetRedIview<T>::DetRedIview()
        : m_index_stack()
        , m_Rstack()
        , m_Lstack()
        , m_Loffset(0)
        , m_Coffset(0)
        , m_Roffset(0)
        , m_initial_value(T())
    {
    }

    template <typename T>
    DetRedIview<T>::~DetRedIview()  { }


    template <typename T>
    void DetRedIview<T>::init_from_pedigree(const pedigree& ped,
                                            const T& val,
                                            bool called_from_constructor)
    {
        int L = ped.length();
        m_Roffset = 0;

        if (!called_from_constructor) {
            m_Loffset = 0;
            m_Rstack.clear();
            m_Lstack.clear();
            m_index_stack.clear();
        }

        // Push placeholder pedigree values (in reverse) onto left stack. 
        for (int i = L-1; i >=0; --i) {
            m_index_stack.push_back(PedStackElem<T>(0, NULL));
        }
        m_index_stack.push_back(PedStackElem<T>(m_Lstack.size(), NULL));

        // Now push the pedigree in order onto the right stack.
        m_Coffset = m_index_stack.size();
        for (int i = 0; i < L; ++i) {
            m_index_stack.push_back(PedStackElem<T>(m_Rstack.size(),
                                                    NULL));
            m_Rstack.push_back(TaggedElem<T>(T(),
                                             rank_range_tag(ped[i])));
        }

        if (L > 0) {
            // Update the active value.
            m_Rstack.back().val = val;
            m_index_stack.push_back(PedStackElem<T>(m_Rstack.size(), NULL));
            m_Roffset = (int)m_index_stack.size()-2;

            /// Leave m_initial_value alone.  This field always
            //  stores the value when the object is created.
        }
        else {
            // When we initialize from an empty pedigree, val also
            // gets stored in "m_initial_value".
            m_initial_value += val;
        }
    }


    template <typename T>
    DetRedIview<T>::DetRedIview(const T& val)
        : m_index_stack()
        , m_Rstack()
        , m_Lstack()
        , m_Loffset(0)
        , m_Coffset(0)
        , m_Roffset(0)
        , m_initial_value(val)
    {
    }
    

    template <typename T>
    DetRedIview<T>::DetRedIview(const T& val, const pedigree& ped)
        : m_index_stack()
        , m_Rstack()
        , m_Lstack()
        , m_Loffset(0)
        , m_initial_value(T())
    {
        // The initial value of the view is stored in m_initial_val.
        // The pedigree stack starts at 0.
        init_from_pedigree(ped, val, true);
    }


    template <typename T>
    pedigree DetRedIview<T>::get_start_pedigree() const
    {
        // Call the helper method below without correcting the last term. 
        return get_start_pedigree_helper(false);
    }

    template <typename T>
    pedigree DetRedIview<T>::get_active_pedigree() const
    {
        uint64_t* ped_array;
        int active_ped_size = active_pedigree_length();
        if (active_ped_size <= 0)
            return pedigree();

        // Otherwise, we have a nontrivial pedigree.
        ped_array = (uint64_t*)alloca(sizeof(uint64_t) * active_ped_size);

        assert(m_Roffset <= (int)m_index_stack.size()-1);
        for (int q = m_Coffset; q < m_Roffset; ++q) {
            int s = m_index_stack[q+1].starting_offset - 1;
            assert(s < (int)m_Rstack.size());
            const rank_range_tag& ctag = m_Rstack[s].tag;
            assert(ctag.is_leaf());
            ped_array[q-m_Coffset] = ctag.end();
        }

        // TBD: Do we really want to allow the noncommon terms on the
        // right stack to be empty?  It is messy. 
        //  if (m_Roffset < m_index_stack.size() - 1) { ... }
        if (1) {
            for (int q = m_Roffset; q < (int)m_index_stack.size()-1; ++q) {
                int s = m_index_stack[q+1].starting_offset - 1;
                assert(s < (int)m_Rstack.size());
                const rank_range_tag& ctag = m_Rstack[s].tag;
                // This assert should hold because the last element in
                // every range on the right stack should be "active",
                // i.e., it can't be merged with anything else yet,
                // because we might still be updating it.
                assert(ctag.is_leaf());
                ped_array[q-m_Coffset] = ctag.end();
            }
        }
        // else {
        //     // Rare case where we have only common prefix terms on the
        //     // right stack, but no
        //     // This case should happen only in a funny
        //     // intermediate state, only after we have called
        //     // pop_and_move_common_range() with a "false" parameter.

        //     // In this case, we better have at least one term on the
        //     // left stack.  The pedigree term we want is the last term
        //     // on the end of the left stack.
        //     assert(m_Loffset >= 1);
        //     int s = m_index_stack[m_Loffset].starting_offset - 1;
        //     assert(s == m_Lstack.size()-1);
        //     ped_array[m_Roffset - m_Coffset] = m_Lstack.back().tag.end();
        // }
        
        return pedigree(ped_array, active_ped_size, false);
    }


    template <typename T>
    inline pedigree DetRedIview<T>::get_start_pedigree_helper(bool minus_one) const
    {
        int start_ped_length = start_pedigree_length();
        if (start_ped_length <= 0)
            return pedigree();

        // Otherwise, we have a nontrivial pedigree.
        uint64_t* ped_array;
        ped_array = (uint64_t*)alloca(sizeof(uint64_t) * (start_ped_length));
        int x = 0;

        // Read in the pedigree, in order.

        // Correct for the "+1" in the last pedigree term.
        // Intuitively, all the ranges stored on the right stack are
        // the ones we expect to execute next, once we pop up to that
        // level.  So if our initial pedigree is <1, 3, 2, 5>,
        // the right stack actually starts with 
        // common prefix <2, 4, 3> and value <5>.
        //
        // Similarly, every range we move over from right to left has
        // this offset, except the first range which started with "5".

        // Grab the common prefix.
        for (int q = m_Coffset; q < m_Roffset; ++q) {
            int s = m_index_stack[q].starting_offset;
            ped_array[x] = m_Rstack[s].tag.start();
            x++;
        }

        // Grab the term from the common range, stored on the right
        // stack.  Subtract 1 because any ranges on the right stack
        // start at 1 greater than the corresponding range in the
        // start pedigree, except in the case where there is nothing
        // on the left stack.  But we account for that below.
        if (m_Roffset < (int)m_index_stack.size()-1) {
            int common_s = m_index_stack[m_Roffset].starting_offset;
            ped_array[x] = m_Rstack[common_s].tag.start() - 1;
            x++;
        }

        // Grab any sections stored on the left stack.  Ranges on the
        // left stack are one more than the starting pedigree, except
        // maybe the term on the bottom.
        for (int q = m_Loffset-1; q >= 0; --q) {
            int s = m_index_stack[q].starting_offset;
            assert((s >= 0) && (s < (int)m_Lstack.size()));
            ped_array[x] = m_Lstack[s].tag.start() - 1;
            x++;
        }

        if (minus_one) {
            // We already had an extra "-1" for the deepest term in
            // our traversal above.

            // We just need to check if we underflowed to "-1".  If
            // so, pop off a pedigree term.
            if ((x > 0) && (ped_array[x-1] == (uint64_t)(-1))) {
                x--;
                start_ped_length--;
            }
        }
        else {
            // Take away the extra "-1" for the deepest term in the
            // initial pedigree.  It is usually from the left, except when
            // the left stack is empty.
            if (x > 0) {
                ped_array[x-1]++;
            }
        }

        CILKPUB_DETRED_DBG_ASSERT(1, x == start_ped_length);
        return pedigree(ped_array, start_ped_length, false);
    }



    // Validate all the ranges
    template <typename T>
    inline void DetRedIview<T>::validate_value_range(int pedstack_idx) const {
        int verbose = 0;
        int num_errors = 0;
        assert(pedstack_idx >= 0);
        assert(pedstack_idx < (int)m_index_stack.size());
        const std::vector<TaggedElem<T> >& my_stack = (pedstack_idx >= m_Coffset ? m_Rstack : m_Lstack);
        // int local_idx = pedstack_idx;
        // if (local_idx >= m_Coffset) {
        //     local_idx -= m_Coffset;
        // }

        int start_s = m_index_stack[pedstack_idx].starting_offset;
        int stop_s =  m_index_stack[pedstack_idx+1].starting_offset-1;
        for (int s = start_s; s < stop_s; ++s) {
            num_errors += ((my_stack[s].tag.end() + 1) != my_stack[s+1].tag.start());
            if (num_errors) {
                fprintf(stderr, "pedstack_idx=%d, my_stack[%d].tag.end() is %lld, start is %lld\n",
                        pedstack_idx,
                        s,
                        (long long)my_stack[s].tag.end(),
                        (long long)my_stack[s+1].tag.start());
                assert(0);
            }
            // Check that all but the last two are not mergeable.
            if (s < (stop_s - 1)) {
                // fprintf(stderr, "Compare at s = %d: ", s);
                // m_value_stack[s].tag.fprint(stderr);
                // fprintf(stderr, "  s+1 = %d   ... ", s+1);
                // m_value_stack[s+1].tag.fprint(stderr);
                // fprintf(stderr, "\n");
                num_errors += my_stack[s].tag.is_left_sibling_of(my_stack[s+1].tag);

                if (num_errors) {
                    fprintf(stderr, "pedstack_idx = %d\n", pedstack_idx);
                    fprintf(stderr, "Compare at s = %d: ", s);
                    my_stack[s].tag.fprint(stderr);
                    fprintf(stderr, "  s+1 = %d   ... ", s+1);
                    my_stack[s+1].tag.fprint(stderr);
                    fprintf(stderr, "\n");
                    assert(0);
                }
            }

            if (num_errors) {
                fprintf(stderr, "ERROR at pedstack idx = %d, s = %d\n", pedstack_idx, s);
            }
            assert(num_errors == 0);
        }
    }

    template <typename T>    
    inline void DetRedIview<T>::validate(void) const {
        for (int idx = 0; idx < (int)m_index_stack.size()-1; ++idx) {
            validate_value_range(idx);
        }
    }

    template <typename T>
    inline bool DetRedIview<T>::tags_equal(const DetRedIview<T>& other) const 
    {
        if (this->m_index_stack.size() != other.m_index_stack.size()) {
            return false;
        }

        if (this->m_Lstack.size() != other.m_Lstack.size()) {
            return false;
        }

        if (this->m_Rstack.size() != other.m_Rstack.size()) {
            return false;
        }

        for (size_t s = 0; s < this->m_Lstack.size(); ++s) {
            if (this->m_Lstack[s].tag != other.m_Lstack[s].tag) {
                return false;
            }
        }
        for (size_t s = 0; s < this->m_Rstack.size(); ++s) {
            if (this->m_Rstack[s].tag != other.m_Rstack[s].tag) {
                return false;
            }
        }
        return true;
    }
    
    
    template <typename T>
    T DetRedIview<T>::get_value() const 
    {
        T val = m_initial_value;
        for (size_t s = 0; s < m_Lstack.size(); ++s) {
            val += m_Lstack[s].val;
        }
        for (size_t s = 0; s < m_Rstack.size(); ++s) {
            val += m_Rstack[s].val;
        }
        return val;
    }

    template <typename T>
    T DetRedIview<T>::get_active_elem_val() const
    {
        if (!m_Rstack.empty()) {
            CILKPUB_DETRED_DBG_ASSERT(1, m_Rstack.back().tag.is_leaf());
            return m_Rstack.back().val;
        }
        else {
            return T();
        }
    }

    template <typename T>
    inline uint64_t DetRedIview<T>::active_group_start_rank() const {
        int s = m_index_stack[m_index_stack.size() - 2].starting_offset;
        return m_Rstack[s].tag.start();
    }

    template <typename T>
    inline uint64_t DetRedIview<T>::active_group_active_rank() const {
        return last_tagged_elem().tag.end();
    }

    template <typename T>
    const TaggedElem<T>* DetRedIview<T>::get_leaf_range_array(int* length_ptr) const
    {
        int sz = this->m_index_stack.size();
        int right_offset = this->m_index_stack[sz-2].starting_offset;
        *length_ptr = (this->m_index_stack[sz-1].starting_offset -
                       right_offset);
        return &this->m_Rstack[right_offset];
    }




    
    template <typename T>
    void DetRedIview<T>::push_new_right_range(uint64_t new_rank, const T& v)
    {
        assert(m_index_stack.back().starting_offset == (int)m_Rstack.size());
        // Push a new range [0] onto the index stack.
        m_Rstack.push_back(TaggedElem<T>(T(), rank_range_tag(0)));
        m_index_stack.push_back(PedStackElem<T>(m_Rstack.size(), NULL));
        // Now update the leaf range to the new rank.
        update_active_range_group(new_rank, v);
    }


    template <typename T>
    void DetRedIview<T>::pop_and_merge_right_range(bool advance_leaf_term)
    {
        int leaf_depth = m_index_stack.size() - 1;

        // Make sure we are not the common range.
        assert(leaf_depth > m_Roffset);

        // Terminate our range. Any right range had better begin with
        // a 0 (both before and after we terminate).
        this->terminate_active_range_group();

        assert(last_tagged_elem().tag.start() == 0);
        assert(m_Rstack.size() == m_index_stack[m_index_stack.size()-1].starting_offset);

        int last_idx = m_Rstack.size()-1;
        int parent_rank = m_Rstack[last_idx-1].tag.end();

        // The parent range should be a single value.
        assert(m_Rstack[last_idx-1].tag.start() == parent_rank);
        
        // Merge the last value into the one before it.
        m_Rstack[last_idx-1].val += m_Rstack[last_idx].val;
        // Pop that last range and value. 
        m_index_stack.pop_back();
        m_Rstack.pop_back();

        assert(m_Rstack.size() == m_index_stack[m_index_stack.size()-1].starting_offset);

        if (advance_leaf_term) {
            // Do the increment of the parent.
            this->update_active_range_group(parent_rank+1, T());
        }
    }

    template <typename T>
    void DetRedIview<T>::pop_and_move_common_range()
    {
        // We almost always want to call this method and advance the
        // pedigree.  The only time we don't is when we are merging
        // ranges, because the next pedigree is already in the range
        // to the right.
        pop_and_move_common_range_helper(true);
    }


    template <typename T>
    inline void DetRedIview<T>::pop_and_move_common_range_helper(bool advance_pedigree)
    {
        assert((m_index_stack.size()-2) == m_Roffset);

        // The offset in the pedigree of the common term between the
        // start and active pedigrees.
        int common_term_offset = common_stem_length()-1;
        // Make sure left and right stacks agree on how many common
        // terms they have.
        assert(m_Loffset + 1 + common_stem_length() == start_pedigree_length());

//        fprintf(stderr, "Here: l_offset = %d, startnig_offset = %d, Lstack size = %zd\n",
//                m_Loffset, m_index_stack[m_Loffset].starting_offset, m_Lstack.size());
//        fprintf(stderr, "Here: l_offset = %d, startnig_offset = %d, Lstack size = %zd\n",
//                m_Loffset+1, m_index_stack[m_Loffset+1].starting_offset, m_Lstack.size());
        // Make sure that the end of the left stack is sane.
        assert(m_index_stack[m_Loffset].starting_offset == m_Lstack.size());
        

        int num_elems;
        const TaggedElem<T>* common_range_array = get_leaf_range_array(&num_elems);
        assert(num_elems >= 1);
        
        // Push each of these terms into the left stack.
        for (int i = 0; i < num_elems; ++i) {
            m_Lstack.push_back(common_range_array[i]);
        }
        m_Loffset++;
        m_index_stack[m_Loffset].starting_offset = m_index_stack[m_Loffset-1].starting_offset + num_elems;

//        fprintf(stderr, "After here: l_offset = %d, startnig_offset = %d, Lstack size = %zd\n",
//                m_Loffset, m_index_stack[m_Loffset].starting_offset, m_Lstack.size());
        assert(m_index_stack[m_Loffset].starting_offset == m_Lstack.size());

        // Erase the last "num_elems" elements from the right stack
        // now.  This pops off the common range.
        m_Rstack.erase(m_Rstack.end()-num_elems, m_Rstack.end());
        m_index_stack.pop_back();
        
        // fprintf(stderr, "After erase: m_index_stack.back().startnig_offset = %d, m_Rstack.size() = %zd\n",
        //         m_index_stack.back().starting_offset, m_Rstack.size());
        // this->fprint(stderr);
        assert(m_index_stack.back().starting_offset == m_Rstack.size());

        // Move the next term on the common prefix "down" to the
        // active part of the right stack.
        m_Roffset--;
        
        // Check to advance the pedigree.
        if (advance_pedigree) {
            // Update the common prefix rank to add 1.
            m_Rstack.back().tag.inc_rank(1);
        }
        else {
            // If we aren't advancing the pedigree, then we just pop
            // that term off the right stack.
            // fprintf(stderr, "Aren't advancing... here m_m_index_stack.size before pop is %zd, rstack before pop is %zd, m_Roffset=%d\n",

            //         m_index_stack.size(),
            //         m_Rstack.size(),
            //         m_Roffset);
            // this->fprint(stderr);
            // fprintf(stderr, "\n");
                    
            m_index_stack.pop_back();
            m_Rstack.pop_back();
            assert(m_index_stack.back().starting_offset == m_Rstack.size());

            // Now we no longer have anything terms which are not part
            // of the common prefix on the right stack.
            assert(m_index_stack.size()-1 == m_Roffset);
        }
    }

    template <typename T>
    void DetRedIview<T>::copy_left_range_to_Rstack(DetRedIview<T>* right_view,
                                                   int right_l_offset)
    {
        TaggedElemStack<T>::append_groups_and_append_index(this->m_Rstack,
                                                           this->m_index_stack,
                                                           this->m_index_stack.size()-2,
                                                           right_view->m_Lstack,
                                                           right_view->m_index_stack,
                                                           right_l_offset,
                                                           right_l_offset+1);
    }
    
    template <typename T>
    void DetRedIview<T>::append_right_ranges_to_Rstack(DetRedIview<T>* right_view,
                                                       int start_offset,
                                                       int stop_offset)
    {
        TaggedElemStack<T>::append_groups_and_append_index(this->m_Rstack,
                                                           this->m_index_stack,
                                                           this->m_index_stack.size()-2,
                                                           right_view->m_Rstack,
                                                           right_view->m_index_stack,
                                                           start_offset,
                                                           stop_offset);
    }


    template <typename T>
    void DetRedIview<T>::append_left_ranges_to_Lstack(DetRedIview<T>* other,
                                                      int start_offset,
                                                      int stop_offset)
    {
        assert(start_offset >= 0);
        assert(stop_offset <= other->m_Loffset);
        int final_idx =
            TaggedElemStack<T>::append_groups_and_overwrite_index(this->m_Lstack,
                                                                  this->m_index_stack,
                                                                  this->m_Loffset,
                                                                  other->m_Lstack,
                                                                  other->m_index_stack,
                                                                  start_offset,
                                                                  stop_offset);
        this->m_Loffset = final_idx;
    }



    template <typename T>
    void DetRedIview<T>::pop_common_prefix_ranges_from_Rstack(int num_ranges_to_pop)
    {
        // Check invariant, that we don't have any right or common
        // ranges on this rstack.
        {
            int final_index = this->m_index_stack.size()-1;
            assert(m_Roffset == final_index);
            assert(m_Rstack.size() ==
                   m_index_stack[final_index].starting_offset);
        }
        
        assert((int)m_Rstack.size() >= num_ranges_to_pop);
        // Pop specified number of ranges from the rstack.
        m_Rstack.erase(m_Rstack.end() - num_ranges_to_pop,
                       m_Rstack.end());

        // Also pop the same number of ranges from the index stack.
        // Since we are getting rid of common prefix terms, each range
        // should have only one term.
        m_index_stack.erase(m_index_stack.end() - num_ranges_to_pop,
                            m_index_stack.end());
        m_Roffset -= num_ranges_to_pop;

        // Check the invariants again.
        {
            int final_index2 = this->m_index_stack.size()-1;
            assert(m_Roffset == final_index2);
            assert(m_Rstack.size() ==
                   m_index_stack[final_index2].starting_offset);
        }
    }


    // Test class that provides print methods.
    class __test_DetRedIview {
    public:        
        // Print the contents of a view to file.
        template <typename T>
        static void tfprint_iview(FILE* f, const DetRedIview<T>& iview);

        // Adds random values to this view for testing purposes.
        template <typename T>
        static T inject_random_values(DetRedIview<T>& iview);


    private:
        template <typename T>
        static void fprint_value_stack_helper(FILE* f,
                                              const DetRedIview<T>& iview,
                                              bool right_side,
                                              int start, int stop);
    };

    


    template <typename T>
    void __test_DetRedIview::tfprint_iview(FILE *f, const DetRedIview<T>& iview)
    {
        const char indent_string[] = "    ";
        fprintf(f, "\n%s******* BEGIN of Iview %p *********\n",
                indent_string,
                &iview);
        fprintf(f, "\nInitial value: ");
        tfprint(f, iview.m_initial_value);
        fprintf(f, "\n");

        fprintf(f, "%sLStack_size=%llu, RStack_size=%llu, m_Loffset=%d, m_Coffset = %d, m_Roffset=%d),  %zu index entries\n",
                indent_string, 
                iview.m_Lstack.size(),
                iview.m_Rstack.size(),
                iview.m_Loffset,
                iview.m_Coffset,
                iview.m_Roffset,
                iview.m_index_stack.size());
        
        // Print the portions of the stack, from left to right.
        fprintf(f, "%sLeft stack: \n", indent_string);
        fprint_value_stack_helper(f, iview,
                                  false, 0, iview.m_Loffset);
        fprintf(f, "%sLeft placeholders: [%d, %d): \n",
                indent_string,
                iview.m_Loffset, iview.m_Coffset);
        fprint_value_stack_helper(f, iview,
                                  false, iview.m_Loffset, iview.m_Coffset);

        fprintf(f, "%sCommon prefix: \n", indent_string);
        fprint_value_stack_helper(f, iview,
                                  true, iview.m_Coffset, iview.m_Roffset);
        fprintf(f, "%sRight stack: \n", indent_string);
        fprint_value_stack_helper(f, iview,
                                  true,
                                  iview.m_Roffset,
                                  iview.m_index_stack.size()-1);
        fprintf(f, "%s******* END   of Iview %p *********\n", indent_string, &iview);
    }


    template <typename T>
    void __test_DetRedIview::fprint_value_stack_helper(FILE *f,
                                                       const DetRedIview<T>& iview,
                                                       bool right_side,
                                                       int start, int stop)
    {
        const char indent_string[] = "    ";
        for (int q = start; q < stop; ++q) {
            int ped_depth;
            if (right_side) {
                ped_depth = q - iview.m_Coffset;
            }
            else {
                ped_depth = iview.start_pedigree_length() - 1 - q;
            }
            
            fprintf(f, "%s%s, q = %d, Depth=%d, offsets [%d, %d):  ",
                    indent_string,
                    (right_side ? "R"            : "L"),
                    q, 
                    ped_depth,
                    iview.m_index_stack[q].starting_offset,
                    iview.m_index_stack[q+1].starting_offset);

            if (right_side || (q < iview.m_Loffset)) {
                for (int j = iview.m_index_stack[q].starting_offset;
                     j < iview.m_index_stack[q+1].starting_offset;
                     ++j) {
                    if (right_side) {
                        iview.m_Rstack[j].fprint(f);
                    }
                    else {
                        iview.m_Lstack[j].fprint(f);
                    }
                    fprintf(f, "  ");
                }
            }
            fprintf(f, "  \n");
        }
    }

    template <typename T>
    T __test_DetRedIview::inject_random_values(DetRedIview<T>& iview)
    {
        // Check whether m_Roffset and m_Loffset are valid.
        CILKPUB_DETRED_DBG_ASSERT(0, (iview.m_Roffset >= 0));
        CILKPUB_DETRED_DBG_ASSERT(0, (iview.m_Roffset < (int)iview.m_index_stack.size()));
        CILKPUB_DETRED_DBG_ASSERT(0, (iview.m_Loffset >= 0));
        CILKPUB_DETRED_DBG_ASSERT(0, (iview.m_Loffset < (int)iview.m_index_stack.size()));

        int rstart, lstop;
        rstart = iview.m_index_stack[iview.m_Roffset].starting_offset;
        lstop = iview.m_index_stack[iview.m_Loffset].starting_offset;

        CILKPUB_DETRED_DBG_ASSERT(0, rstart < (int)iview.m_Rstack.size());
        CILKPUB_DETRED_DBG_ASSERT(0, rstart >= 0);
        CILKPUB_DETRED_DBG_ASSERT(0, lstop == (int)iview.m_Lstack.size());

        // Inject a new initial value.
        T new_sum = T(0);
        T test_val = T(42);
        T delta = T(7);

        iview.m_initial_value += test_val;
        new_sum += test_val;
        test_val+= delta;


        // Inject values on the right side.
        for (size_t q = rstart; q < iview.m_Rstack.size(); ++q) {
            iview.m_Rstack[q].val += test_val;
            new_sum += test_val;
            test_val += delta;
            
        }

        // Inject values on the left stack.
        for (size_t q = 0; q < iview.m_Lstack.size(); ++q) {
            iview.m_Lstack[q].val += test_val;
            new_sum += test_val;
            test_val += delta;
        }
        return new_sum;
    }
    
};  // namespace cilkpub


// Separate the implementations of update and merge into different
// files.  These methods still operate on the same data structures for
// the DetRedIview class, but they logically represent different (and
// independent) operations.

// Implementation of update for a deterministic reducer.
#include "detred_iview_update.cpp"

// Implementation of merge.  We include merge after update, because
// merge logically depends on update.  
#include "detred_iview_merge.cpp"
