/* test_rank_range_tag.cpp                  -*-C++-*-
 *
 *************************************************************************
 *
 * Copyright (C) 2012, Intel Corporation
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
 * @file test_rank_range_tag.cpp
 *
 * @brief Test rank ranges for deterministic op-ad reducer.
 */
#include <assert.h>
#include <cstdio>
#include <cstdlib>

#include <cilk/reducer_opadd.h>
#include <cilkpub/rank_range_tag.h>

#include "cilktest_harness.h"

using namespace cilkpub;

inline void tfprint(FILE* f, rank_range_tag& tag)
{
    tag.fprint(f);
}

inline void pass_test() {
    CILKTEST_REMARK(1, "%s\n", "....PASSED");
}

// Test a few hard-coded values. 
int test_rank_range_tag_fixed() {
    CILKTEST_REMARK(1, "test_rank_range_tag_fixed()...");
    TEST_ASSERT(rank_range_tag(0, 0).is_left_sibling_of(rank_range_tag(1, 0)));

    rank_range_tag v = rank_range_tag(11, 0);
    TEST_ASSERT(v.is_valid());
    TEST_ASSERT(v.is_right_child());
    TEST_ASSERT(!v.is_left_child());
    TEST_ASSERT(v.is_leaf());
    
    rank_range_tag p = rank_range_tag(10, 1);
    rank_range_tag ls = rank_range_tag(10, 0);  // left sibling
    TEST_ASSERT(!p.is_leaf());
    TEST_ASSERT(ls.is_leaf());

    TEST_ASSERT(ls.is_left_sibling_of(v));
    TEST_ASSERT(!ls.is_left_sibling_of(p));
    TEST_ASSERT(!v.is_left_sibling_of(ls));
    TEST_ASSERT(!(rank_range_tag(9, 0).parent() ==
             rank_range_tag(10, 0).parent()));
    
    TEST_ASSERT(p.right_child() == v);
    TEST_ASSERT(p.left_child() == ls);
    
    rank_range_tag tmp = v.parent();
    TEST_ASSERT(p == v.parent());
    TEST_ASSERT(v.parent() == p);
    TEST_ASSERT(ls.parent() == p);
    TEST_ASSERT(ls.is_left_child());
    TEST_ASSERT(!ls.is_right_child());

    rank_range_tag p2 = rank_range_tag(8, 2);
    TEST_ASSERT(p.parent() == p2);
    TEST_ASSERT(p.is_right_child());
    TEST_ASSERT(!p.is_left_child());
    TEST_ASSERT(p2.right_child() == p);
    TEST_ASSERT(p2.left_child() == rank_range_tag(8, 1));
    
    TEST_ASSERT(ls.is_left_child_of(p));
    TEST_ASSERT(!ls.is_left_child_of(p2));
    TEST_ASSERT(!ls.is_right_child_of(p));
    TEST_ASSERT(!ls.is_right_child_of(p2));

    TEST_ASSERT(!(rank_range_tag(12, 0).parent() == p));
    
    rank_range_tag q = rank_range_tag(12, 2);
    TEST_ASSERT(!(q.parent() == p));
    TEST_ASSERT(q.parent() == rank_range_tag(8, 3));
    TEST_ASSERT(q.left_child() == rank_range_tag(12, 1));
    TEST_ASSERT(q.right_child() == rank_range_tag(14, 1));
    TEST_ASSERT(!q.is_leaf());

    TEST_ASSERT(rank_range_tag(8, 3).end() == 15);
    TEST_ASSERT(rank_range_tag(12, 2).end() == 15);
    TEST_ASSERT(rank_range_tag(11, 0).end() == 11);

    pass_test();
    return 0;
}


int test_rank_range_tag() {

    CILKTEST_REMARK(1, "test_rank_range_tag()...");
    const int MAX_VAL = 17;
    rank_range_tag test_vals[MAX_VAL+1];

    for (int i = 0; i <= MAX_VAL; ++i) {
        test_vals[i] = rank_range_tag((uint64_t)i, 0);
    }
    
    for (int i = 0; i <= MAX_VAL; ++i) {

        if (CILKTEST_VERBOSITY() >= 3) {
            CILKTEST_PRINT(3, "", test_vals[i], " ");
        }
        rank_range_tag tmp = rank_range_tag(i);
        TEST_ASSERT(test_vals[i].is_valid());
        TEST_ASSERT(test_vals[i] == tmp);
    }
    if (CILKTEST_VERBOSITY() >= 3) {
        CILKTEST_REMARK(3, "\n");
    }

    rank_range_tag tmp_level[MAX_VAL+1];
    int current_length = MAX_VAL+1;


    // We are going to do a BFS walk of this tree.
    rank_range_tag* input_array = test_vals;
    rank_range_tag* output_array = tmp_level;
    int output_length = 0;
    int level_num = 0;

    while (current_length > 1) {
        for (int i = 0; i < current_length; i +=2) {
            rank_range_tag p1, p2;
            TEST_ASSERT(input_array[i].is_left_child());
            TEST_ASSERT(!input_array[i].is_right_child());
            p1 = input_array[i].parent();
            TEST_ASSERT(p1.left_child() == input_array[i]);
            
            if ((i+1) < current_length) {
                TEST_ASSERT(!input_array[i+1].is_left_child());
                TEST_ASSERT(input_array[i+1].is_right_child());
                p2 = input_array[i+1].parent();
                TEST_ASSERT((p1 == p2) && (p2 == p1));
                TEST_ASSERT(p1.right_child() == input_array[i+1]);

                TEST_ASSERT(input_array[i].is_left_sibling_of(input_array[i+1]));
                if ((i-1) >= 0) {
                    TEST_ASSERT(!input_array[i-1].is_left_sibling_of(input_array[i]));
                }
            }

            if (CILKTEST_VERBOSITY() >= 3) {
                CILKTEST_REMARK(3, "P1 is: ");
                CILKTEST_PRINT(3, " ", p1, "...");
                CILKTEST_REMARK(3, "input_array[%d] is ", i);
                CILKTEST_PRINT(3, "", input_array[i], "...");
                if ((i+1) < current_length) {
                    CILKTEST_REMARK(3, "input_array[%d] is ", i+1);
                    CILKTEST_PRINT(3, " ", input_array[i+1], "...");
                }
                CILKTEST_REMARK(3, "DONE\n");
            }

            TEST_ASSERT(input_array[i].is_left_child_of(p1));
            TEST_ASSERT(!input_array[i].is_right_child_of(p1));

            if ((i+1) < current_length) {
                TEST_ASSERT(!input_array[i+1].is_left_child_of(p1));
                TEST_ASSERT(input_array[i+1].is_right_child_of(p1));
            }


            TEST_ASSERT(p1.lg_size() == (level_num+1));

            // Push parent onto next level.
            output_array[output_length] = p1;
            output_length++;
        }

        level_num++;
        // Swap input and output arrays.

        // Input array becomes new output. 
        rank_range_tag* tmp = output_array;
        output_array = input_array;
        input_array = tmp;

        // Set the lengths appropriately.
        current_length = output_length;
        output_length = 0;
    }

    if (CILKTEST_VERBOSITY() >= 3) {
        CILKTEST_REMARK(3,
                        "Finished with level_num = %d.  current_length =%d, Root range is ",
                        level_num,
                        current_length);
        output_array[0].print();
        CILKTEST_REMARK(3, "\n");
    }
    TEST_ASSERT(current_length == 1);
    pass_test();
    return 0;
}


int main(int argc, char* argv[]) {
    CILKTEST_PARSE_TEST_ARGS(argc, argv);
    CILKTEST_BEGIN("rank_range_tag");
    
    test_rank_range_tag_fixed();
    test_rank_range_tag();

    return CILKTEST_END("rank_range_tag");
}
