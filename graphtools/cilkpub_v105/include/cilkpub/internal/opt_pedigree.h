/* opt_pedigree.h                  -*-C++-*-
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
 * @file opt_pedigree.h
 *
 * @brief Implementation of opt_pedigree class.
 *
 *
 * @author Jim Sukha
 *
 * @see Pedigrees
 */

#include <cilk/cilk_api.h>

namespace cilkpub {

    template <int STATIC_PED_LENGTH>
    opt_pedigree<STATIC_PED_LENGTH>::opt_pedigree()
        : m_length(0)
        , m_rev_ped_full((uint64_t*)m_rev_ped)
    {
    }

    template <int STATIC_PED_LENGTH>
    opt_pedigree<STATIC_PED_LENGTH>::opt_pedigree(uint64_t* buffer,
                                                  int d,
                                                  bool is_reversed)
        : m_length(d)
        , m_rev_ped_full((uint64_t*)m_rev_ped)
    {
        // Figure out which array we are storing into based on d.
        uint64_t* pedbuf;
        if (d > STATIC_PED_LENGTH) {
            // Malloc an array if the pedigree is long.
            // Technically, we are wasting the space in the static buffer.
            // But we waste space for at most "STATIC_PED_LENGTH" terms,
            // and a pedigree stored contiguously is easier to work with.
            m_rev_ped_full = (uint64_t*)malloc(sizeof(uint64_t) * d);
            pedbuf = m_rev_ped_full;
        }
        else {
            pedbuf = (uint64_t*)m_rev_ped;
        }

        // Fill the reverse pedigree buffer from the buffer.
        // Calculate whether to traverse the buffer forward or
        // backward.
        int start = (is_reversed ? 0 : d-1);
        int delta = (is_reversed ? 1 : -1);

        // Use array notations to copy over the entire array. :)
        pedbuf[0:d:1] = buffer[start:d:delta];
    }

    // Copy constructor. 
    template <int STATIC_PED_LENGTH>
    opt_pedigree<STATIC_PED_LENGTH>::opt_pedigree(const opt_pedigree<STATIC_PED_LENGTH>& ped)
        : m_length(ped.m_length)
        , m_rev_ped_full((uint64_t*)m_rev_ped)
    {
        // If the pedigree we are copying from is longer than our
        // static buffer size, then allocate a buffer on the heap.
        if (m_length > STATIC_PED_LENGTH) {
            assert(ped.m_rev_ped_full != (uint64_t*)ped.m_rev_ped);
            m_rev_ped_full = (uint64_t*)malloc(sizeof(uint64_t) * m_length);
        }
        // Copy over terms from ped's array.
        m_rev_ped_full[0:m_length] = ped.m_rev_ped_full[0:m_length];
    }

    template <int STATIC_PED_LENGTH>
    opt_pedigree<STATIC_PED_LENGTH>::~opt_pedigree()
    {
        // Free the buffer if we allocated it from the heap.
        if (m_rev_ped_full != (uint64_t*)m_rev_ped) {
            free(m_rev_ped_full);
        }
    }

    // Assignment operator.
    template <int STATIC_PED_LENGTH>
    opt_pedigree<STATIC_PED_LENGTH>&
    opt_pedigree<STATIC_PED_LENGTH>::operator=(const opt_pedigree<STATIC_PED_LENGTH>& ped)
    {
        int new_length = ped.length();

        // First, we want to create enough space in current buffer.
        // Consider all cases comparing new_length,
        // STATIC_PED_LENGTH, and m_length.
        //
        // 1. new_length < STATIC_PED_LENGTH.  
        //    Then, our static-sized buffer is good enough.
        //    Just clear what we have.
        //
        // 2. STATIC_PED_LENGTH <= new_length <= m_length.
        //    Our current buffer is big enough, and is already malloced.
        //    We can just reuse the existing buffer.
        //        
        // 3. new_length >= STATIC_PED_LENGTH, and new_length > m_length
        //    We need to resize the existing buffer to be large enough for the new
        //    pedigree. 

        if (new_length <= STATIC_PED_LENGTH) {
            // Case 1.  Clear the buffer we have.  This operation
            // might need to free a buffer allocated from the heap.
            this->clear();
        }
        else {
            // We are maintaining the invariant that we always use the
            // static buffer if we can.
            if (new_length <= m_length) {
                // Case 2: STATIC_PED_LENGTH < new_length <= m_length.
                // Just reuse the existing buffer.  We don't need to
                // anything more here.
                // But since m_length > STATIC_PED_LENGTH, verify that
                // we aren't using the static buffer.
                assert(m_rev_ped_full != (uint64_t*)m_rev_ped);
            }
            else {
                // Case 3: Need to resize the existing buffer we are using, because
                // new_length > STATIC_PED_LENGTH and new_length > m_length.
                if (m_rev_ped_full == (uint64_t*)m_rev_ped) {
                    // 3(a): We were using the static size buffer from before.
                    // Allocate a new buffer off the heap.
                    assert(m_length <= STATIC_PED_LENGTH);
                    m_rev_ped_full = (uint64_t*)malloc(sizeof(uint64_t) * new_length);
                }
                else {
                    // 3(b): We were using a buffer off the heap
                    //       already, but it wasn't large enough.
                    //       Resize the buffer.
                    //       We want to  use reallocf because we don't care
                    //       about the old contents.  But since it does not
                    //       seem to be standard, I'll use realloc for now.
                    assert(m_length > STATIC_PED_LENGTH);
                    m_rev_ped_full = (uint64_t*)realloc(m_rev_ped_full,
                                                        sizeof(uint64_t) * new_length);
                }
                assert(m_rev_ped_full);
            }
        }
        // Copy the contents over from ped.
        m_length = new_length;
        m_rev_ped_full[0:m_length] = ped.m_rev_ped_full[0:m_length];
        return *this;
    }

    template <int STATIC_PED_LENGTH>
    opt_pedigree<STATIC_PED_LENGTH>
    opt_pedigree<STATIC_PED_LENGTH>::current()
    {
        opt_pedigree<STATIC_PED_LENGTH> c;
        c.get_current_pedigree();
        return c;
    }

    template <int STATIC_PED_LENGTH>
    opt_pedigree<STATIC_PED_LENGTH>
    opt_pedigree<STATIC_PED_LENGTH>::current(const opt_pedigree_scope<STATIC_PED_LENGTH>& scope) 
    {
        opt_pedigree<STATIC_PED_LENGTH> c;
        c.get_current_scoped_pedigree(scope);
        return c;
    }

    template <int STATIC_PED_LENGTH>
    inline int opt_pedigree<STATIC_PED_LENGTH>::length() const
    {
        return m_length;
    }

    // Get the kth term of this pedigree.
    // We must have 0 <= k < this->size();
    template <int STATIC_PED_LENGTH>
    const uint64_t& opt_pedigree<STATIC_PED_LENGTH>::operator[](int k) const
    {
        // Recall that the array is stored in reverse order.
        uint64_t* rbuf = this->get_rev_ped_array();
        return rbuf[this->length() -1 - k];
    }


    // Returns the number of terms in the common prefix of this
    // pedigree and b.
    template <int STATIC_PED_LENGTH>
    int opt_pedigree<STATIC_PED_LENGTH>::common_prefix_length(const opt_pedigree<STATIC_PED_LENGTH>& b) const
    {
        int a_length = this->length();
        int b_length = b.length();
        int min_length = (a_length < b_length) ? a_length : b_length;
        
        // Look for the first term in the pedigrees that differ, or
        // when we run out of one or the other.
        int current_length = 0;
        while ((current_length < min_length) &&
               ((*this)[current_length] == b[current_length]))
        {
            current_length++;
        }
        return current_length;
    }
    
    // Returns -1 for this < b
    // Returns  0 for this == b
    // Returns  1 for this > b
    template <int STATIC_PED_LENGTH>
    int opt_pedigree<STATIC_PED_LENGTH>::compare(const opt_pedigree<STATIC_PED_LENGTH>& b) const
    {
        int a_length = this->length();
        int b_length = b.length();

        // Start checking at the term after the common prefix.
        int current_length = common_prefix_length(b);

        // First, consider the case when there are no terms left in a
        // after removing the common prefix.
        if (current_length >= a_length) {
            // By definition f the common prefix length, we should
            // have exactly a_length terms.
            assert(current_length == a_length);
            if (current_length >= b_length) {
                // If we are also out of terms in b, then all terms are equal.
                assert(current_length == b_length);
                return 0;
            }
            else {
                // Otherwise, b has more terms, and a < b.
                return -1;
            }
        }

        // Otherwise, if there is a term in a, but no terms left in b,
        // then a must be larger.
        if (current_length >= b_length) {
            return 1;
        }

        // Otherwise, we have terms left in both a and b after
        // removing the common prefix.  Then, the comparison depends
        // on the first remaining term.
        uint64_t a_term = (*this)[current_length];
        uint64_t b_term = b[current_length];
        if (a_term < b_term)
            return -1;
        if (a_term == b_term)
            return 0;
        return 1;
    }

    // For operators <, <=, >, and >=, use the compare function.

    template <int STATIC_PED_LENGTH>    
    inline bool operator<(const opt_pedigree<STATIC_PED_LENGTH>& a, const opt_pedigree<STATIC_PED_LENGTH>& b)
    {
        return (a.compare(b) < 0);
    }

    template <int STATIC_PED_LENGTH>    
    inline bool operator<=(const opt_pedigree<STATIC_PED_LENGTH>& a, const opt_pedigree<STATIC_PED_LENGTH>& b)
    {
        return (a.compare(b) <= 0);
    }

    template <int STATIC_PED_LENGTH>    
    inline bool operator>(const opt_pedigree<STATIC_PED_LENGTH>& a, const opt_pedigree<STATIC_PED_LENGTH>& b)
    {
        return (a.compare(b) > 0);
    }

    template <int STATIC_PED_LENGTH>    
    inline bool operator>=(const opt_pedigree<STATIC_PED_LENGTH>& a, const opt_pedigree<STATIC_PED_LENGTH>& b)
    {
        return (a.compare(b) >= 0);
    }


    // For operator == and !=, we can be slightly more efficient
    // because we can quit early.
    
    template <int STATIC_PED_LENGTH>    
    bool operator==(const opt_pedigree<STATIC_PED_LENGTH>& a, const opt_pedigree<STATIC_PED_LENGTH>& b)
    {
        if (a.length() != b.length()) {
            return false;
        }
        int L = a.length();
        uint64_t* a_buf = a.get_rev_ped_array();
        uint64_t* b_buf = b.get_rev_ped_array();
        for (int q = 0; q < L; ++q) {
            if (a_buf[q] != b_buf[q]) {
                return false;
            }
        }
        return true;
    }

    template <int STATIC_PED_LENGTH>
    bool operator!=(const opt_pedigree<STATIC_PED_LENGTH>& a, const opt_pedigree<STATIC_PED_LENGTH>& b) 
    {
        return !(a == b);
    }


    template <int STATIC_PED_LENGTH>
    bool opt_pedigree<STATIC_PED_LENGTH>::is_prefix_of(const opt_pedigree<STATIC_PED_LENGTH>& ped) const
    {
        int a_length = this->length();
        int current_length = common_prefix_length(ped);
        // a is a prefix of b if there are no terms left in a after
        // ignoring the common prefix.
        return (current_length >= a_length);
    }

    template <int STATIC_PED_LENGTH>
    bool opt_pedigree<STATIC_PED_LENGTH>::in_scope_of(const opt_pedigree<STATIC_PED_LENGTH>& ped) const
    {
        int a_length = this->length();
        int b_length = ped.length();
        int common_length = this->common_prefix_length(ped);

        if (common_length >= b_length) {
            // If we are out of terms in b, then b is a prefix of a.
            // Then, a is in the scope of b.
            return true;
        }

        // If we are out of terms in a, then a is a prefix of b, and
        // thus a can't be in the scope of b.
        if (common_length >= a_length) {
            return false;
        }

        // Otherwise, to be in scope, the differing term must be the
        // last term of b.
        return ((common_length == (b_length-1)) &&
                (ped[common_length] < (*this)[common_length]));
    }


    // The iterator for terms in the pedigree can be expressed in
    // terms of normal standard iterators (really array pointers).
    
    // Forward iteration of pedigree.
    template <int STATIC_PED_LENGTH>
    opt_pedigree<STATIC_PED_LENGTH>::const_iterator opt_pedigree<STATIC_PED_LENGTH>::begin() const
    {
        return std::reverse_iterator<const uint64_t*> (rend());
    }

    template <int STATIC_PED_LENGTH>
    opt_pedigree<STATIC_PED_LENGTH>::const_iterator opt_pedigree<STATIC_PED_LENGTH>::end() const
    {
        return std::reverse_iterator<const uint64_t*>(rbegin());
    }

    // Reverse iteration of pedigree.
    template <int STATIC_PED_LENGTH>
    opt_pedigree<STATIC_PED_LENGTH>::const_reverse_iterator opt_pedigree<STATIC_PED_LENGTH>::rbegin() const
    {
        return this->get_rev_ped_array();
    }

    template <int STATIC_PED_LENGTH>
    opt_pedigree<STATIC_PED_LENGTH>::const_reverse_iterator opt_pedigree<STATIC_PED_LENGTH>::rend() const
    {
        return this->get_rev_ped_array() + this->length();
    }


    // TBD: For now, I haven't found a compelling use case where I
    // have felt like mutating the pedigree object.
    //
    // template <int STATIC_PED_LENGTH>
    // opt_pedigree<STATIC_PED_LENGTH>::iterator opt_pedigree<STATIC_PED_LENGTH>::begin() 
    // {
    //     return std::reverse_iterator<uint64_t*> (rend());
    // }

    // template <int STATIC_PED_LENGTH>
    // opt_pedigree<STATIC_PED_LENGTH>::iterator opt_pedigree<STATIC_PED_LENGTH>::end() 
    // {
    //     return std::reverse_iterator<uint64_t*>(rbegin());
    // }

    // template <int STATIC_PED_LENGTH>
    // opt_pedigree<STATIC_PED_LENGTH>::reverse_iterator opt_pedigree<STATIC_PED_LENGTH>::rbegin() 
    // {
    //     return this->get_rev_ped_array();
    // }

    // template <int STATIC_PED_LENGTH>
    // opt_pedigree<STATIC_PED_LENGTH>::reverse_iterator opt_pedigree<STATIC_PED_LENGTH>::rend() 
    // {
    //     return this->get_rev_ped_array() + this->length();
    // }
    
    template <int STATIC_PED_LENGTH>    
    int opt_pedigree<STATIC_PED_LENGTH>::get_current_reverse_pedigree(uint64_t* buffer, int d)
    {
        __cilkrts_pedigree pednode = __cilkrts_get_pedigree();
        const __cilkrts_pedigree *ped = &pednode;
        const __cilkrts_pedigree *remainder = NULL;
        int length = 0;

        while ((NULL != ped) && (length < d)) {
            buffer[length] = ped->rank;
            ped = ped->parent;
            length++;
        }

        if (NULL != ped) {
            // We must have stopped at d.  Then, there is at least one
            // more term.  We want to return a value bigger than d.
            // The increment reflects the fact that there is at least
            // one extra term that we haven't stored into the buffer.
            // If we didn't do an increment,
            assert(length == d);
            length++;
        }
        return length;
    }

    template <int STATIC_PED_LENGTH>    
    int opt_pedigree<STATIC_PED_LENGTH>::copy_to_array(uint64_t* buffer, int d) const
    {
        uint64_t* current_ped = get_rev_ped_array();
        int terms_to_copy = (length() < d) ? length() : d;
        int initial_offset = length() - 1;

        // Copy as many terms as we can, starting from the root terms.
        for (int idx = 0; idx < terms_to_copy; ++idx) {
            buffer[idx] = current_ped[initial_offset - idx];
        }
        return terms_to_copy;
    }

    template <int STATIC_PED_LENGTH>    
    int opt_pedigree<STATIC_PED_LENGTH>::copy_reverse_to_array(uint64_t *buffer, int d) const
    {
        uint64_t* current_ped = get_rev_ped_array();
        // Number of terms to copy is min(m_length, d).
        int terms_to_copy = (length() < d) ? length() : d;

        buffer[0:terms_to_copy] = current_ped[0:terms_to_copy];
        return terms_to_copy;
    }


    /**
     * @warning For debugging only.
     * I hope this method can go away in the future.
     */
    template <int STATIC_PED_LENGTH>
    void opt_pedigree<STATIC_PED_LENGTH>::fprint(FILE* f, const char* header) const
    {
        uint64_t* buf = get_rev_ped_array();

        // Convert the pedigree into a string representation.
        // A uint64_t has maximum size which fits into 22 digits
        // when you consider a decimal representation.
        const int DIGIT_LENGTH = 22;
        int max_output_length = this->m_length * DIGIT_LENGTH + 10;
        char* output_buf = (char*)malloc(max_output_length);
        assert(output_buf);
        int current_len= 0;

        // Correct for Windows vs. Unix/Linux variations in snprintf.
#ifdef _WIN32        
        int len = _snprintf_s(output_buf+current_len,
                              max_output_length-current_len,
                              3,
                              "[ ");
#else
        int len = std::snprintf(output_buf+current_len, 3, "[ ");
#endif
        
        current_len += len;
        assert(current_len < max_output_length);
        for (int i = this->m_length-1; i >= 0; i--) {

#ifdef _WIN32
            len = _snprintf_s(output_buf+current_len,
                              max_output_length - current_len,
                              DIGIT_LENGTH,
                              "%llu ", buf[i]);
#else
            len = std::snprintf(output_buf+current_len,
                                DIGIT_LENGTH,
                                "%llu ", buf[i]);
#endif            
            current_len += len;
            assert(current_len < max_output_length);
        }

#ifdef _WIN32
        len = _snprintf_s(output_buf+current_len,
                          max_output_length - current_len,
                          2,
                          "]");
#else
        len = std::snprintf(output_buf+current_len,
                            2,
                            "]");        
#endif
        
        current_len += len;
        assert(current_len < max_output_length);

        // Print out the string.
        if (header) {
            std::fprintf(f, "%s%s", header, output_buf);
        }
        else {
            std::fprintf(f, "%s", output_buf);
        }
        free(output_buf);
    }


    // Implementation of private methods. 
    template <int STATIC_PED_LENGTH>
    uint64_t* opt_pedigree<STATIC_PED_LENGTH>::get_rev_ped_array() const
    {
        // Debugging check only.  Verify that we are not using the
        // static size buffer if the pedigree is too long, and that we
        // are using the static size buffer when the pedigree fits.
        if (m_length > STATIC_PED_LENGTH) {
            assert(m_rev_ped_full != (uint64_t*)m_rev_ped);
        }
        else {
            assert(m_rev_ped_full == (uint64_t*)m_rev_ped);
        }
        return m_rev_ped_full;
    }

    /// Resets this pedigree to an empty length, freeing any buffer
    /// that might be allocated off the heap.
    template <int STATIC_PED_LENGTH>
    void opt_pedigree<STATIC_PED_LENGTH>::clear()
    {
        m_length = 0;
        if (m_rev_ped_full != (uint64_t*)m_rev_ped) {
            free(m_rev_ped_full);
            m_rev_ped_full = (uint64_t*)m_rev_ped;
        }
    }

    template <int STATIC_PED_LENGTH>
    int opt_pedigree<STATIC_PED_LENGTH>::get_current_pedigree()
    {
        __cilkrts_pedigree pednode = __cilkrts_get_pedigree();
        const __cilkrts_pedigree *ped = &pednode;
        const __cilkrts_pedigree *remainder = NULL;
        this->clear();
        
        // Walk up the tree and store up to STATIC_PED_LENGTH as we
        // terms of the pedigree.
        while ((NULL != ped) && (m_length < STATIC_PED_LENGTH)) {
            m_rev_ped[m_length] = ped->rank;
            m_length++;
            ped = ped->parent;
        }

        // Walk up the remainder of the pedigree and count the length.
        // In the (hopefully) common case, where the pedigree already
        // fits, this loop does not need to execute any more.
        // 
        // Otherwise, we are computing the length to know how large a
        // buffer to allocate.
        remainder = ped;
        while (NULL != ped) {
            m_length++;
            ped = ped->parent;
        }

        // If the pedigree is too long, we will walk up the pedigree a
        // second time, to store the answer in our buffer.
        if (m_length > STATIC_PED_LENGTH) {
            m_rev_ped_full = (uint64_t*)malloc(sizeof(uint64_t) * m_length);
            m_rev_ped_full[0:STATIC_PED_LENGTH] = m_rev_ped[0:STATIC_PED_LENGTH];

            // Walk the remainder again.
            for (int start = STATIC_PED_LENGTH; start < m_length; ++start) {
                m_rev_ped_full[start] = remainder->rank;
                remainder = remainder->parent;
            }

            // We should have the same length pedigree as before...
            assert(NULL == remainder);
        }
        return m_length;
    }

    template <int STATIC_PED_LENGTH>
    int opt_pedigree<STATIC_PED_LENGTH>::
    get_current_scoped_pedigree(const opt_pedigree_scope<STATIC_PED_LENGTH>& scope)
    {
        __cilkrts_pedigree pednode = __cilkrts_get_pedigree();
        const __cilkrts_pedigree *ped = &pednode;
        const __cilkrts_pedigree *remainder = NULL;
        __cilkrts_pedigree stop_node = scope.get_stop_node();
        this->clear();
        
        // Walk up the tree and store up to STATIC_PED_LENGTH as we
        // terms of the pedigree.
        while ((NULL != ped)
               && (stop_node.parent != ped)
               && (m_length < STATIC_PED_LENGTH)) {
            m_rev_ped[m_length] = ped->rank;
            m_length++;
            ped = ped->parent;
        }
        
        // Walk up the remainder of the pedigree and count the length.
        // We are going to walk this part twice unfortunately... once
        // to size the array, and a second time to store the answer.
        remainder = ped;
        while ((NULL != ped)
               && (stop_node.parent != ped)) {
            m_length++;
            ped = ped->parent;
        }
        
        // If we ran out of pedigree terms, we must be reading a
        // pedigree that is not in scope.  Bail out early.
        if ((NULL == ped) &&
            (stop_node.parent != ped)) {
            fprintf(stderr, "WARNING: reading in pedigree out of scope\n");
            m_length = 0;
            return 0;
        }
        assert(ped == stop_node.parent);

        if (m_length > STATIC_PED_LENGTH) {
            m_rev_ped_full = (uint64_t*)malloc(sizeof(uint64_t) * m_length);
            m_rev_ped_full[0:STATIC_PED_LENGTH] = m_rev_ped[0:STATIC_PED_LENGTH];

            // Walk the remainder again.
            for (int start = STATIC_PED_LENGTH; start < m_length; ++start) {
                m_rev_ped_full[start] = remainder->rank;
                remainder = remainder->parent;
            }

            // We should have the same length pedigree as before...
            assert(stop_node.parent == remainder);
        }

        // The stop node should have a smaller rank than the last term
        // of the pedigree.  Otherwise, the current pedigree we are
        // reading is not in scope, and we return an empty pedigree.
        if (stop_node.rank > m_rev_ped_full[m_length-1]) {
            fprintf(stderr, "WARNING: reading in pedigree out of scope\n");
            fprintf(stderr, "stop_node.rank = %llu, last term is %llu\n",
                    stop_node.rank, m_rev_ped_full[m_length-1]);
            m_length = 0;
            return 0;
        }

        // Correct the last term of the pedigree.
        m_rev_ped_full[m_length-1] -= stop_node.rank;

        return m_length;
    }
    
}; // namespace cilkpub
