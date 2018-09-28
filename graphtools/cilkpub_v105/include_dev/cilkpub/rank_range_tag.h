/* rank_range.h                  -*-C++-*-
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

#ifndef __CILK_RANK_RANGE_TAG_H_
#define __CILK_RANK_RANGE_TAG_H_
    
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

namespace cilkpub {

// A rank range {start, lg_size} represents a range of ranks of the
// form [start, start + 2^{lg_size}-1].
//
// We assume start & (start & ((1 << lg_size) - 1))  is 0, i.e., the
// low lg_size bits of start are 0.
//
// Examples:
//   {3, 0} represents [3, 3]
//   {4, 0} represents [4, 4]
//   {4, 1} represents [4, 5]
//   {4, 2} represents [4, 7]
//   {4, 3} is not valid.
//
//
// The universe of all valid simple_rank_range objects forms an implicit
// balanced binary tree.  Children and parent relationships can be
// calculated via bit manipulations.
//
// The proof is by ASCII art. 
//
//
// (0, 0) (1, 0)  (2, 0) (3, 0)  (4, 0) (5, 0)  (6, 0) (7, 0)  (8, 0) (9, 0)   ...
//     \    /         \    /         \    /        \    /         \    /
//     (0, 1)         (2, 1)         (4, 1)        (6, 1)         (8, 1)       ...
//           \        /                   \        /
//             (0, 2)                       (4, 2)                             ...
//                  \                       / 
//                           (0, 3)                                            ...
//                             .
//                             .
//                             .
    struct rank_range_tag {
        // Default constructed range is [0, 0]
        rank_range_tag();

        // Construct a leaf range with starting rank.
        rank_range_tag(uint64_t rank);

        // Constructor with a starting rank and at a particular depth.
        // NOTE: not all ranges are valid.
        rank_range_tag(uint64_t rank, int8_t lg_size_);

        // Check that the range is valid. 
        inline bool is_valid() const;

        // Return the first rank in the range.
        inline uint64_t start() const;
        
        // Return the last rank in this range.
        inline uint64_t end() const;

        // Returns lg of the size of the range.
        inline int8_t lg_size() const;

        // Returns the size of the range.
        inline uint64_t size() const;

        // Returns true if specified rank falls in this range.
        inline bool contains_rank(uint64_t rank) const;

        // Comparison operators.
        inline bool operator ==(const rank_range_tag& other) const;
        inline bool operator !=(const rank_range_tag& other) const;

        // Relationships in the tree. 
        inline rank_range_tag parent() const;
        inline rank_range_tag left_child() const;
        inline rank_range_tag right_child() const;

        // Change this rank into its parent.
        inline void change_to_parent();

        // Increment the rank of this range.
        // Using this method usually results in a bad range unless
        // this range corresponds to a leaf or you know what are
        // doing.
        inline void inc_rank(int64_t delta);

        // Returns true if this range is a leaf (i.e., its size is 1)
        inline bool is_leaf() const;

        inline bool is_left_child() const;
        inline bool is_right_child() const;
        // Check whether I am the left child of "par".
        // Assumes that this and par are valid ranges.
        inline bool is_left_child_of(rank_range_tag& par) const;
        inline bool is_right_child_of(rank_range_tag& par) const;
        
        inline bool is_left_sibling_of(const rank_range_tag& right) const;

        inline void fprint(FILE* f) const;
        inline void print() const;

    private:
        uint64_t m_start;
        int8_t m_lg_size;
    };


};  // namespace cilkpub

#include "internal/rank_range_tag.cpp"

#endif
