/* rank_range_tag.cpp                 -*-C++-*-
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


    // Default constructed range is [0, 0]
    rank_range_tag::rank_range_tag()
        : m_start(0)
        , m_lg_size(0)
    {
    }
    
    // Construct a leaf range with starting rank.
    rank_range_tag::rank_range_tag(uint64_t rank)
        : m_start(rank)
        , m_lg_size(0)
    {
    }

    // Constructor with a starting rank and at a particular depth.
    // NOTE: not all ranges are valid.
    rank_range_tag::rank_range_tag(uint64_t rank, int8_t lg_size_)
        : m_start(rank)
        , m_lg_size(lg_size_)
    {
        assert(is_valid());
    }

    inline bool rank_range_tag::is_valid() const
    {
        return ((m_start & (m_start & ((1 << m_lg_size) - 1))) == 0);
    }

    inline uint64_t rank_range_tag::start() const
    {
        return m_start;
    }

    inline uint64_t rank_range_tag::end() const
    {
        return m_start + ((uint64_t)1 << m_lg_size) - 1;
    }

    inline int8_t rank_range_tag::lg_size() const
    {
        return m_lg_size;
    }

    inline uint64_t rank_range_tag::size() const
    {
        return ((uint64_t)1 << m_lg_size);
    }

    inline bool rank_range_tag::contains_rank(uint64_t rank) const
    {
        return ((rank >= this->m_start) && (rank <= this->end()));
    }

    inline bool rank_range_tag::operator ==(const rank_range_tag& other) const
    {
        return ((other.m_start == this->m_start) &&
                (other.m_lg_size == this->m_lg_size));
    }

    inline bool rank_range_tag::operator !=(const rank_range_tag& other) const
    {
        return ((other.m_start != this->m_start) ||
                (other.m_lg_size != this->m_lg_size));
    }

    inline rank_range_tag rank_range_tag::parent() const
    {
        // To get parent, eliminate next lowest bit, and then
        // increment m_lg_size by 1.
        uint64_t new_rank = (this->m_start >> (this->m_lg_size+1)) << (this->m_lg_size+1);
        return rank_range_tag(new_rank,
                              this->m_lg_size+1);
    }

    inline void rank_range_tag::change_to_parent()
    {
        this->m_lg_size++;
        this->m_start = (this->m_start >> (this->m_lg_size)) << (this->m_lg_size);
    }

    inline void rank_range_tag::inc_rank(int64_t delta)
    {
        this->m_start += delta;
    }

    inline bool rank_range_tag::is_leaf() const
    {
        return (this->m_lg_size == 0);
    }

    inline rank_range_tag rank_range_tag::left_child() const
    {
        assert(this->m_lg_size > 0);
        return rank_range_tag(this->m_start,
                          this->m_lg_size-1);
    }

    inline rank_range_tag rank_range_tag::right_child() const
    {
        assert(this->m_lg_size > 0);
        return rank_range_tag(this->m_start + (1 << (this->m_lg_size-1)),
                                 this->m_lg_size-1);
    }

    inline bool rank_range_tag::is_left_sibling_of(const rank_range_tag& right) const
    {
        return ((this->m_lg_size == right.m_lg_size) &&
                (this->m_start < right.m_start) &&
                ((this->m_start >> (this->m_lg_size+1)) == (right.m_start >> (this->m_lg_size+1))));
    }

    inline bool rank_range_tag::is_left_child() const
    {
        // Check the low order bit.
        return ((this->m_start & (1 << this->m_lg_size)) == 0);
    }

    inline bool rank_range_tag::is_right_child() const
    {
        return ((this->m_start & (1 << this->m_lg_size)) > 0);
    }

    // Check whether I am the left child of "par".
    // Assumes that this and par are valid ranges. 
    inline bool rank_range_tag::is_left_child_of(rank_range_tag& par) const
    {
        return ((this->m_start == par.m_start) && (par.m_lg_size == (this->m_lg_size+1)));
    }
    
    inline bool rank_range_tag::is_right_child_of(rank_range_tag& par) const
    {
        return ((par.m_lg_size == (this->m_lg_size + 1)) &&
                (this->m_start == (par.m_start + (1 << (par.m_lg_size-1)))));
    }

    inline void rank_range_tag::fprint(FILE* f) const {
        fprintf(f, "{%llu, %d}",
                this->m_start,
                (int)this->m_lg_size);
    }

    inline void rank_range_tag::print() const {
        fprint(stderr);
    }

};  // namespace cilkpub
