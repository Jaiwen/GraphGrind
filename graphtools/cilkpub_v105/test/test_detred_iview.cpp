/* test_detred_iview.cpp              -*-C++-*-
 *
 *************************************************************************
 *
 * Copyright (C) 2013, Intel Corporation
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

#include <assert.h>
#include <cstdio>
#include <cstdlib>

#include <cilk/cilk.h>
#include <cilk/reducer_opadd.h>
#include <cilkpub/internal/detred_iview.h>

#include "cilktest_harness.h"
#include "test_detred.h"
#include "test_detred_pedlist.h"

using namespace cilkpub;

inline void tfprint(FILE* f, const pedigree& ped)
{
    ped.fprint(f, "");
}

inline void pass_test() {
    if (CILKTEST_NUM_ERRORS() == 0) {
        CILKTEST_REMARK(1, "%s\n", "....PASSED");
    }
    else {
        CILKTEST_REMARK(1, "%s\n", "....FAILED");
    }
}


static bool within_tol(double sum, double expected) {
    return ((sum - expected) * (sum - expected)) < 1.0e-12;
}

static bool within_tol(int sum, int expected)
{
    return (sum == expected);
}


template <typename T>
void test_det_reduce_view_basic() {
    CILKTEST_REMARK(2, "test_det_reduce_view_basic... ");

    uint64_t test1[] = {4, 2, 1};
    DetRedIview<T> v(32, pedigree(test1, 3, true));
    CILKTEST_PRINT(2, "v: ", v, "\n");

    uint64_t test2[] = {0, 1};
    DetRedIview<T> v2(12, pedigree(test2, 2, true));
    CILKTEST_PRINT(2, "v2: ", v, "\n");
    pass_test();
}


// #define PED_DEBUG 1


template <typename T>
void test_det_reduce_view_update_helper(uint64_t stem_root,
                                        uint64_t start_i,
                                        uint64_t ped_delta) {
    int base_verbosity = 3;
    // The common pedigree stem we are using for this test (stored as
    // a reverse pedigree).
    const int STEM_LENGTH = 3;
    uint64_t common_stem[STEM_LENGTH] = {100, 32, 5};
    common_stem[0] = stem_root;

    T expected_sum = 0;
    const int NUM_PED_TERMS = 3;
    const int MAX_RANK = 11;
    // We are going to encode small pedigrees as 64-bit ints, encoded
    // so that the ordering of the encoded ints matches the natural
    // ordering of pedigrees.
    uint64_t MAX_PED = SimpleTestPed::compute_max_ped(NUM_PED_TERMS, MAX_RANK);

    CILKTEST_REMARK(base_verbosity, "test_det_reduce_view_update[stem_root=%llu, start_i=%llu, ped_delta=%llu]\n",
                    stem_root, start_i, ped_delta);
    TEST_ASSERT(ped_delta >= 1);

    pedigree common_stem_ped = pedigree(common_stem, STEM_LENGTH, true);
    DetRedIview<T> v(expected_sum, common_stem_ped);
    T tmp_sum = __test_DetRedIview::inject_random_values(v);
    expected_sum += tmp_sum;
    
    CILKTEST_REMARK(base_verbosity+1, "Starting with initial stem with length = %d: ", STEM_LENGTH);
    CILKTEST_PRINT(base_verbosity+1, "Common stem ped: ", common_stem_ped, "\n");
    CILKTEST_REMARK(base_verbosity+1, "start_i = %llu, MAX_PED = %llu, delta = %llu\n",
           start_i, MAX_PED, ped_delta);
    CILKTEST_PRINT(base_verbosity+1, "DetRedIview is: ", v, "\n");

    
    uint64_t ped1[NUM_PED_TERMS + STEM_LENGTH];
    int ped1_length;

        
    for (uint64_t ped = start_i; ped < MAX_PED; ped += ped_delta) {
        SimpleTestPed::fill_rev_ped_from_int(ped, MAX_RANK, NUM_PED_TERMS,
                                             ped1, &ped1_length,
                                             common_stem, STEM_LENGTH);
        pedigree test_ped = pedigree(ped1, ped1_length, true);
        CILKTEST_REMARK(base_verbosity+1, "Before update with ped %llu  \n", ped);
        CILKTEST_PRINT(base_verbosity+1, "test_ped: ", test_ped, "\n");
        CILKTEST_PRINT(base_verbosity+1, "iview: ", v, "\n");
        CILKTEST_PRINT(base_verbosity+1, "iview start pedigree: ", v.get_start_pedigree(), "\n");
        CILKTEST_PRINT(base_verbosity+1, "iview active pedigree: ", v.get_active_pedigree(), "\n");
        v.update_with_pedigree(test_ped, 1);
        expected_sum += 1;

        // Technically, I should be checking against a tolerance...
        CILKTEST_PRINT(base_verbosity+1, "Expected sum is ", expected_sum, "\n");
        CILKTEST_PRINT(base_verbosity+1, "View sum is ", v.get_value(), "\n");
        TEST_ASSERT(within_tol(v.get_value(), expected_sum));

        CILKTEST_REMARK(base_verbosity+1, "After update with ped %llu  \n", ped);
        CILKTEST_PRINT(base_verbosity+1, "test_ped: ", test_ped, "\n");
        CILKTEST_PRINT(base_verbosity+1, "iview after: ", v, "\n");
        CILKTEST_PRINT(base_verbosity+1, "iview after start pedigree: ", v.get_start_pedigree(), "\n");
        CILKTEST_PRINT(base_verbosity+1, "iview after active pedigree: ", v.get_active_pedigree(), "\n");

        TEST_ASSERT_EQ(v.get_active_pedigree(), test_ped);
    }

    CILKTEST_REMARK(base_verbosity,  "Final range with sum %f \n", v.get_value());
    CILKTEST_PRINT(base_verbosity, "Final view: ", v, "\n");
}

template <typename T>
void test_det_reducer_view_update() {
    CILKTEST_REMARK(2, "test_det_reducer_view_update...");
    for (uint64_t stem_root = 0; stem_root < 3; ++stem_root) {
        for (uint64_t start_i = 1; start_i < 5; ++start_i) {
            for (uint64_t ped_delta = 1; ped_delta < 17; ++ped_delta) {
                test_det_reduce_view_update_helper<T>(stem_root,
                                                      start_i,
                                                      ped_delta);
            }
            CILKTEST_REMARK(2, ".");
        }
    }
    pass_test();
}


template <typename T>
void test_det_reducer_view_update_from_list() {
    CILKTEST_REMARK(2, "test_det_reducer_view_update_from_list...");

    for (int start_i = 0; start_i < TESTPED_LENGTH; ++start_i) {
        pedigree start_ped = pedigree(PED_LIST[start_i].ped,
                                      PED_LIST[start_i].length,
                                      false);
        
        for (int update_i = start_i; update_i < TESTPED_LENGTH; ++update_i) {
            pedigree update_ped = pedigree(PED_LIST[update_i].ped,
                                           PED_LIST[update_i].length,
                                           false);

            CILKTEST_PRINT(3, "Reducer update start_ped: ", start_ped, "\n");
            CILKTEST_PRINT(3, "Reducer update update_ped: ", update_ped, "\n");

            DetRedIview<T> v(T(32), start_ped);
            v.update_with_pedigree(update_ped, T(12));

            T expected_sum = T(32) + T(12);

            TEST_ASSERT(within_tol(v.get_value(), expected_sum));

            TEST_ASSERT(v.get_start_pedigree() == start_ped);
            TEST_ASSERT(v.get_active_pedigree() == update_ped);
        }
        CILKTEST_REMARK(2, ".");
    }
    pass_test();
}


template <typename T>
void test_det_reducer_iview_update_simple()
{
    CILKTEST_REMARK(2, "test_det_reducer_iview_update_simple\n");
    int base_verbosity = 3;

    const int BUFFER_LENGTH = 5;
    uint64_t temp_ped[BUFFER_LENGTH] = {1, 2, 4, 8, 0};
    T test_val = T();

    // <1, 2, 4, 8>
    pedigree start_ped = pedigree(temp_ped, 4, false);
    temp_ped[3] = 11;
    DetRedIview<T> v(1, start_ped);
    test_val += 1;
    CILKTEST_PRINT(base_verbosity, "Initial view ", v, "\n");
    TEST_ASSERT_EQ(v.start_pedigree_length(), 4);
    TEST_ASSERT_EQ(v.active_pedigree_length(), 4);

    // Yes, this 3 and not 4.  The common stem is almost the same as
    // the common prefix, except that the stem corresponds to pedigree
    // terms which can not be completed (because they are waiting for
    // values from the left).  When we create a view at 
    // <1, 2, 4, 8>, the common stem length is 3, because we those 
    // first three terms can not be completed.  For example,
    // we can't merge into <1, 2, 4> because we are waiting on <1, 2, 4, 0-7>.
    // 
    // But 8 is special, because we know we have all the terms beginning with 
    // <1, 2, 4, 8> in this range.
    TEST_ASSERT_EQ(v.common_stem_length(), 3);
    TEST_ASSERT_EQ(v.active_pedigree_distinct_terms(), 0);
    
    // Update leaf range: Active becomes <1, 2, 4, 11>
    pedigree u1 = pedigree(temp_ped, 4, false);
    v.update_active_range_group(11, 3);
    test_val += 3;
    CILKTEST_PRINT(base_verbosity, "u1 (update_active_range_group): ", v, "\n");
    TEST_ASSERT_EQ(v.get_start_pedigree(), start_ped);
    TEST_ASSERT_EQ(v.start_pedigree_length(), 4);
    TEST_ASSERT_EQ(v.get_active_pedigree(), u1);
    TEST_ASSERT_EQ(v.active_pedigree_distinct_terms(), 0);
    TEST_ASSERT(within_tol(test_val, v.get_value()));

    temp_ped[4] = 3;
    // Push new right range: Active becomes <1, 2, 4, 11, 3>
    pedigree u2 = pedigree(temp_ped, 5, false);
    v.push_new_right_range(3, 8);
    test_val += 8;
    CILKTEST_PRINT(base_verbosity, "u2 (push_new_right_range): ", v, "\n");
    TEST_ASSERT_EQ(v.get_start_pedigree(), start_ped);
    TEST_ASSERT_EQ(v.start_pedigree_length(), 4);
    TEST_ASSERT_EQ(v.get_active_pedigree(), u2);
    TEST_ASSERT_EQ(v.active_pedigree_distinct_terms(), 1);
    TEST_ASSERT(within_tol(test_val, v.get_value()));

    temp_ped[3]++;
    // Pop and merge right range: Active becomes <1, 2, 4, 12>
    pedigree u3 = pedigree(temp_ped, 4, false);
    v.pop_and_merge_right_range(true);
    CILKTEST_PRINT(base_verbosity, "u3 (pop_and_merge_right_range): ", v, "\n");
    TEST_ASSERT_EQ(v.get_start_pedigree(), start_ped);
    TEST_ASSERT_EQ(v.get_active_pedigree(), u3);
    TEST_ASSERT_EQ(v.start_pedigree_length(), 4);
    TEST_ASSERT_EQ(v.active_pedigree_length(), 4);
    TEST_ASSERT_EQ(v.common_stem_length(), 3);
    TEST_ASSERT_EQ(v.active_pedigree_distinct_terms(), 0);
    TEST_ASSERT(within_tol(test_val, v.get_value()));

    temp_ped[2]++;
    // Pop and move common range: Active becomes <1, 2, 5>
    pedigree u4 = pedigree(temp_ped, 3, false);
    v.pop_and_move_common_range();
    CILKTEST_PRINT(base_verbosity, "u4 (pop_and_move_common_range): ", v, "\n");
    TEST_ASSERT_EQ(v.start_pedigree_length(), 4);
    TEST_ASSERT_EQ(v.active_pedigree_length(), 3);
    TEST_ASSERT_EQ(v.common_stem_length(), 2);
    TEST_ASSERT_EQ(v.get_start_pedigree(), start_ped);
    TEST_ASSERT_EQ(v.get_active_pedigree(), u4);
    TEST_ASSERT_EQ(v.active_pedigree_distinct_terms(), 0);
    TEST_ASSERT(within_tol(test_val, v.get_value()));


    temp_ped[1]++;
    // Pop and move common range: Active becomes <1, 3>
    pedigree u5 = pedigree(temp_ped, 2, false);
    v.pop_and_move_common_range();
    CILKTEST_PRINT(base_verbosity, "u5 (pop_and_move_common_range): ", v, "\n");
    TEST_ASSERT_EQ(v.start_pedigree_length(), 4);
    TEST_ASSERT_EQ(v.active_pedigree_length(), 2);
    TEST_ASSERT_EQ(v.common_stem_length(), 1);
    TEST_ASSERT_EQ(v.get_start_pedigree(), start_ped);
    TEST_ASSERT_EQ(v.get_active_pedigree(), u5);
    TEST_ASSERT_EQ(v.active_pedigree_distinct_terms(), 0);
    TEST_ASSERT(within_tol(test_val, v.get_value()));


    temp_ped[2] = 17;
    // Push_new_right_range: Active becomes <1, 3, 17>
    pedigree u6 = pedigree(temp_ped, 3, false);
    v.push_new_right_range(17, 32);
    test_val += 32;
    CILKTEST_PRINT(base_verbosity, "u6 (push_new_right_range): ", v, "\n");
    TEST_ASSERT_EQ(v.start_pedigree_length(), 4);
    TEST_ASSERT_EQ(v.active_pedigree_length(), 3);
    TEST_ASSERT_EQ(v.common_stem_length(), 1);
    TEST_ASSERT_EQ(v.get_start_pedigree(), start_ped);
    TEST_ASSERT_EQ(v.get_active_pedigree(), u6);
    TEST_ASSERT_EQ(v.active_pedigree_distinct_terms(), 1);
    TEST_ASSERT(within_tol(test_val, v.get_value()));


    temp_ped[1]++;
    // Pop and merge right_range: Active becomes <1, 4>
    pedigree u7 = pedigree(temp_ped, 2, false);
    v.pop_and_merge_right_range(true);
    CILKTEST_PRINT(base_verbosity, "u7 (pop_and_merge_right_range): ", v, "\n");
    TEST_ASSERT_EQ(v.start_pedigree_length(), 4);
    TEST_ASSERT_EQ(v.active_pedigree_length(), 2);
    TEST_ASSERT_EQ(v.common_stem_length(), 1);
    TEST_ASSERT_EQ(v.get_start_pedigree(), start_ped);
    TEST_ASSERT_EQ(v.get_active_pedigree(), u7);
    TEST_ASSERT_EQ(v.active_pedigree_distinct_terms(), 0);
    TEST_ASSERT(within_tol(test_val, v.get_value()));

    temp_ped[0]++;
    // Pop and move common range: Active becomes <2>
    pedigree u8 = pedigree(temp_ped, 1, false);
    v.pop_and_move_common_range();
    CILKTEST_PRINT(base_verbosity, "u8 (pop_and_move_common_range): ", v, "\n");
    TEST_ASSERT_EQ(v.start_pedigree_length(), 4);
    TEST_ASSERT_EQ(v.active_pedigree_length(), 1);
    TEST_ASSERT_EQ(v.common_stem_length(), 0);
    TEST_ASSERT_EQ(v.get_start_pedigree(), start_ped);
    TEST_ASSERT_EQ(v.get_active_pedigree(), u8);
    TEST_ASSERT_EQ(v.active_pedigree_distinct_terms(), 0);
    TEST_ASSERT(within_tol(test_val, v.get_value()));


    pass_test();
}


template <typename T>
void test_det_reducer_iview_update_simple2()
{
    CILKTEST_REMARK(2, "test_det_reducer_iview_update_simple2\n");
    int base_verbosity = 3;

    const int BUFFER_LENGTH = 4;
    uint64_t temp_ped1[BUFFER_LENGTH] = {1, 2, 4, 7};
    uint64_t temp_ped2[BUFFER_LENGTH] = {1, 2, 4, 12};

    uint64_t temp_ped3[BUFFER_LENGTH] = {1, 2, 5, -1};    
    uint64_t temp_ped4[BUFFER_LENGTH] = {3, 2, 1, -1};
    // <1, 2, 4, 7>
    pedigree p1 = pedigree(temp_ped1, 4, false);
    // <1, 2, 4, 12>
    pedigree p2 = pedigree(temp_ped2, 4, false);
    // <1, 2, 5>
    pedigree p3 = pedigree(temp_ped3, 3, false);
    // <3, 2, 1>
    pedigree p4 = pedigree(temp_ped4, 3, false);
    
    T test_val = T(1);
    DetRedIview<T> v(test_val, p1);    

    v.update_with_pedigree(p2, T(2));
    test_val += T(2);
    CILKTEST_PRINT(base_verbosity, "After p2 update: ", v, "\n");
    TEST_ASSERT_EQ(v.get_value(), test_val);
    TEST_ASSERT_EQ(v.get_start_pedigree(), p1);
    TEST_ASSERT_EQ(v.get_active_pedigree(), p2);

    // NOTE: if we call pop_and_move_common_range with "false" as a
    // parameter, we leave the view in a funny "intermediate" state,
    // where there is no range on the active stack to manipulate.
    v.pop_and_move_common_range();
    CILKTEST_PRINT(base_verbosity, "After pop_and_move_common: ", v, "\n");
    CILKTEST_PRINT(base_verbosity, "After pop and move common, what is start pedigree? ", v.get_start_pedigree(), "\n");
    CILKTEST_PRINT(base_verbosity, "After pop and move common, what is active pedigree? ", v.get_active_pedigree(), "\n");
    TEST_ASSERT_EQ(v.active_pedigree_length(), 3);
    TEST_ASSERT_EQ(v.get_value(), test_val);
    TEST_ASSERT_EQ(v.get_start_pedigree(), p1);
    TEST_ASSERT_EQ(v.get_active_pedigree(), p3);
    

    v.update_with_pedigree(p3, T(3));
    test_val += T(3);
    CILKTEST_PRINT(base_verbosity, "After p3 update: ", v, "\n");
    TEST_ASSERT_EQ(v.get_value(), test_val);
    TEST_ASSERT_EQ(v.get_start_pedigree(), p1);
    TEST_ASSERT_EQ(v.get_active_pedigree(), p3);

    v.update_with_pedigree(p4, T(4));
    test_val += T(4);
    CILKTEST_PRINT(base_verbosity, "After p4 update: ", v, "\n");
    TEST_ASSERT_EQ(v.get_value(), test_val);
    TEST_ASSERT_EQ(v.get_start_pedigree(), p1);
    TEST_ASSERT_EQ(v.get_active_pedigree(), p4);
    
    pass_test();
}

template <typename T>
void test_det_reducer_iview_update_simple3()
{
    CILKTEST_REMARK(2, "test_det_reducer_iview_update_simple3\n");
    int base_verbosity = 3;


    const int BUFFER_LENGTH = 4;
    uint64_t temp_ped1[BUFFER_LENGTH] = {0, 0, 0, 0};
    uint64_t temp_ped2[BUFFER_LENGTH] = {1, 0, 0, 0};

    // <0, 0, 0>
    pedigree p1 = pedigree(temp_ped1, 3, false);
    // <1>
    pedigree p2 = pedigree(temp_ped2, 1, false);

    T test_val(4);
    
    DetRedIview<T> v(test_val, p1);

    CILKTEST_PRINT(3, "Initial view: ", v, "\n");
    v.update_with_pedigree(p2, T(3));
    test_val += T(3);

    CILKTEST_PRINT(3, "After update: ", v, "\n");

    TEST_ASSERT(within_tol(v.get_value(), test_val));
    TEST_ASSERT(v.get_start_pedigree() == p1);
    TEST_ASSERT(v.get_active_pedigree() == p2);

    pass_test();
}


template <typename T>
void test_iview_merge_base(int base_verbosity,
                           const pedigree& common_stem_ped,
                           const pedigree& left_start_ped,
                           const pedigree& left_end_ped,
                           const pedigree& split_ped,
                           const pedigree& final_ped)
{
    T initial_value(53);
    T diff_sum(0);
    T tmp_sum(0);
    TEST_ASSERT(common_stem_ped <= left_start_ped);
    TEST_ASSERT(left_start_ped <= left_end_ped);
    TEST_ASSERT(left_end_ped < split_ped);
    TEST_ASSERT(split_ped <= final_ped);
           
    // Create the left view.
    DetRedIview<T> left(initial_value, common_stem_ped);
    left.update_with_pedigree(left_start_ped, T(1));
    left.update_with_pedigree(left_end_ped, T(2));
    T left_val = initial_value + T(1) + T(2);
    tmp_sum = __test_DetRedIview::inject_random_values(left);
    left_val += tmp_sum;
    diff_sum += tmp_sum;
    TEST_ASSERT(within_tol(left.get_value(), left_val));
    

    // Create the right value.
    DetRedIview<T> right(initial_value, split_ped);
    right.update_with_pedigree(split_ped, T(3));
    right.update_with_pedigree(final_ped, T(4));
    T right_val = initial_value + T(3) + T(4);
    tmp_sum = __test_DetRedIview::inject_random_values(right);
    right_val += tmp_sum;
    diff_sum += tmp_sum;
    TEST_ASSERT(within_tol(right.get_value(), right_val));


    DetRedIview<T> cmp_iview(initial_value, common_stem_ped);
    cmp_iview.update_with_pedigree(left_start_ped, T(1));
    cmp_iview.update_with_pedigree(left_end_ped, T(2));
    cmp_iview.update_with_pedigree(split_ped, T(3));
    cmp_iview.update_with_pedigree(final_ped, T(4));

    T cmp_sum = initial_value + T(1) + T(2) + T(3) + T(4);
    T merged_ans = initial_value + T(1) + T(2) + initial_value + T(3) + T(4);
    merged_ans += diff_sum;

    CILKTEST_PRINT(base_verbosity + 2, "Left view: ", left, "\n");
    CILKTEST_PRINT(base_verbosity + 2, "Left start: ", left.get_start_pedigree(), "\n");
    CILKTEST_PRINT(base_verbosity + 2, "Left active: ", left.get_active_pedigree(), "\n");
    CILKTEST_PRINT(base_verbosity + 2, "Left value: ", left.get_value(), "\n");

    CILKTEST_PRINT(base_verbosity + 2, "Right view: ", right, "\n");
    CILKTEST_PRINT(base_verbosity + 2, "Right start: ", right.get_start_pedigree(), "\n");
    CILKTEST_PRINT(base_verbosity + 2, "Right active: ", right.get_active_pedigree(), "\n");
    CILKTEST_PRINT(base_verbosity + 2, "Right value: ", left.get_value(), "\n");

    CILKTEST_PRINT(base_verbosity + 2, "Cmp merged view: ", cmp_iview, "\n");
    CILKTEST_PRINT(base_verbosity + 2, "Cmp start: ", cmp_iview.get_start_pedigree(), "\n");
    CILKTEST_PRINT(base_verbosity + 2, "Cmp active: ", cmp_iview.get_active_pedigree(), "\n");
    CILKTEST_PRINT(base_verbosity + 2, "Cmp value: ", cmp_iview.get_value(), "\n");
    
    left.validate();
    right.validate();
    cmp_iview.validate();

    left.merge(&right);

    CILKTEST_PRINT(base_verbosity + 2, "Left after merge: ", left, "\n");
    CILKTEST_PRINT(base_verbosity + 2, "Final merged value: ", left.get_value(), "\n");
    CILKTEST_PRINT(base_verbosity + 2, "Expected merged value: ", merged_ans, "\n");
    TEST_ASSERT(cmp_iview.tags_equal(left));
    TEST_ASSERT(left.tags_equal(cmp_iview));
    TEST_ASSERT(within_tol(cmp_iview.get_value(), cmp_sum));
    TEST_ASSERT(within_tol(left.get_value(), merged_ans));
}



template <typename T>
void test_det_reducer_iview_merge_helper(uint64_t start_i,
                                         uint64_t left_delta,
                                         uint64_t split_delta,
                                         uint64_t final_delta) {
    int base_verbosity = 2;
    const int PED_DELTA_TERMS = 3;
    const int MAX_RANK = 11;
    const int STEM_LENGTH = 3;
    uint64_t common_stem[STEM_LENGTH] = {0, 4, 3};
    
    uint64_t MAX_PED = SimpleTestPed::compute_max_ped(PED_DELTA_TERMS, MAX_RANK);

    uint64_t left_start_array[STEM_LENGTH + PED_DELTA_TERMS];
    int left_start_array_length;
    uint64_t left_end_array[STEM_LENGTH + PED_DELTA_TERMS];
    int left_end_array_length;
    uint64_t split_array[STEM_LENGTH + PED_DELTA_TERMS];
    int split_array_length;
    uint64_t final_array[STEM_LENGTH + 2*PED_DELTA_TERMS];
    int final_array_length;

    // Create the 4 pedigrees that mark the boundaries of the merge
    // test.
    // Start of left
    SimpleTestPed::fill_rev_ped_from_int(start_i, MAX_RANK, PED_DELTA_TERMS,
                                         left_start_array, &left_start_array_length,
                                         common_stem, STEM_LENGTH);
    pedigree left_start_ped =
        pedigree(left_start_array, left_start_array_length, true);
        
    // End of left. 
    TEST_ASSERT(start_i + left_delta < MAX_PED);
    SimpleTestPed::fill_rev_ped_from_int(start_i + left_delta, MAX_RANK, PED_DELTA_TERMS,
                                         left_end_array, &left_end_array_length,
                                         common_stem, STEM_LENGTH);
    pedigree left_end_ped =
        pedigree(left_end_array, left_end_array_length, true);
    
    // Start of right. 
    TEST_ASSERT(start_i + left_delta + split_delta < MAX_PED);
    SimpleTestPed::fill_rev_ped_from_int(start_i + left_delta + split_delta, MAX_RANK, PED_DELTA_TERMS,
                                         split_array, &split_array_length,
                                         common_stem, STEM_LENGTH);
    pedigree split_ped =
        pedigree(split_array, split_array_length, true);
    
    // End of right.
    TEST_ASSERT(final_delta < MAX_PED);
    SimpleTestPed::fill_rev_ped_from_int(final_delta, MAX_RANK, PED_DELTA_TERMS,
                                         final_array, &final_array_length,
                                         split_array, split_array_length);
    pedigree final_ped =
        pedigree(final_array, final_array_length, true);


    CILKTEST_REMARK(base_verbosity+1,
                    "merge_helper(start_i=%llu, left_delta=%llu, split_delta=%llu, final_delta=%llu)\n",
                    start_i, left_delta, split_delta, final_delta);

    CILKTEST_PRINT(base_verbosity+2, "Left start: ", left_start_ped, "\n");
    CILKTEST_PRINT(base_verbosity+2, "Left end: ", left_end_ped, "\n");
    CILKTEST_PRINT(base_verbosity+2, "Right start: ", split_ped, "\n");
    CILKTEST_PRINT(base_verbosity+2, "Right end: ", final_ped, "\n");

    pedigree common_stem_ped = pedigree(common_stem, STEM_LENGTH, true);
    T initial_value(0);

    test_iview_merge_base<T>(base_verbosity,
                             common_stem_ped,
                             left_start_ped,
                             left_end_ped,
                             split_ped,
                             final_ped);                               
}


template <typename T>
void test_det_reducer_iview_merge_simple()
{
    CILKTEST_REMARK(2, "test_det_reducer_iview_merge_simple()...");

    {
        uint64_t cs_array[] = {1};
        uint64_t ls_array[] = {1, 2, 3, 4};
        uint64_t le_array[] = {1, 3, 2, 4, 1};
        uint64_t s_array[] = {1, 3, 2, 4, 1, 0};
        uint64_t f_array[] = {1, 5, 7};
        
        pedigree common_stem_ped(cs_array, 1, false);
        pedigree left_start_ped(ls_array, 4, false);
        pedigree left_end_ped(le_array, 5, false);
        pedigree split_ped(s_array, 6, false);
        pedigree final_ped(f_array, 3, false);
        test_iview_merge_base<T>(3, common_stem_ped,
                                 left_start_ped,
                                 left_end_ped,
                                 split_ped,
                                 final_ped);
    }

    {

        uint64_t cs_array[1] = {(uint64_t)-1};     // We use this as an empty pedigree.
        uint64_t ls_array[] = {0, 0};
        uint64_t le_array[] = {0, 0};
        uint64_t s_array[] = {0, 0, 0};
        uint64_t f_array[] = {1};
        
        pedigree common_stem_ped(cs_array, 0, false);
        pedigree left_start_ped(ls_array, 2, false);
        pedigree left_end_ped(le_array, 2, false);
        pedigree split_ped(s_array, 3, false);
        pedigree final_ped(f_array, 1, false);
        test_iview_merge_base<T>(3, common_stem_ped,
                                 left_start_ped,
                                 left_end_ped,
                                 split_ped,
                                 final_ped);
    }

    {
        uint64_t cs_array[1] = {(uint64_t)-1};     // We use this as an empty pedigree.
        uint64_t ls_array[] = {0, 0, 0};
        uint64_t le_array[] = {0, 0, 0};
        uint64_t s_array[] = {0, 0, 0, 0};
        uint64_t f_array[] = {1};
        
        pedigree common_stem_ped(cs_array, 0, false);
        pedigree left_start_ped(ls_array, 3, false);
        pedigree left_end_ped(le_array, 3, false);
        pedigree split_ped(s_array, 4, false);
        pedigree final_ped(f_array, 1, false);
        test_iview_merge_base<T>(3, common_stem_ped,
                                 left_start_ped,
                                 left_end_ped,
                                 split_ped,
                                 final_ped);
    }

    {
        uint64_t cs_array[1] = {(uint64_t)-1};     // We use this as an empty pedigree.
        uint64_t ls_array[] = {0, 0, 0, 0};
        uint64_t le_array[] = {0, 0, 0, 0};
        uint64_t s_array[] = {0, 0, 0, 0, 0};
        uint64_t f_array[] = {1};
        pedigree common_stem_ped(cs_array, 0, false);
        pedigree left_start_ped(ls_array, 4, false);
        pedigree left_end_ped(le_array, 4, false);
        pedigree split_ped(s_array, 5, false);
        pedigree final_ped(f_array, 1, false);
        test_iview_merge_base<T>(3, common_stem_ped,
                                 left_start_ped,
                                 left_end_ped,
                                 split_ped,
                                 final_ped);
    }

    {
        uint64_t cs_array[1] = {(uint64_t)-1};     // We use this as an empty pedigree.
        uint64_t ls_array[] = {0, 0, 0, 0, 0, 0};
        uint64_t le_array[] = {0, 0, 0, 0, 1, 2};
        uint64_t s_array[] = {0, 0, 0, 0, 1, 3};
        uint64_t f_array[] = {1};
        pedigree common_stem_ped(cs_array, 0, false);
        pedigree left_start_ped(ls_array, 6, false);
        pedigree left_end_ped(le_array, 6, false);
        pedigree split_ped(s_array, 6, false);
        pedigree final_ped(f_array, 1, false);
        test_iview_merge_base<T>(3, common_stem_ped,
                                 left_start_ped,
                                 left_end_ped,
                                 split_ped,
                                 final_ped);
    }


    {
        uint64_t cs_array[1] = {(uint64_t)-1};     // We use this as an empty pedigree. 
        uint64_t ls_array[] = {0, 0, 0, 0};
        uint64_t le_array[] = {0, 0, 0, 3};
        uint64_t s_array[] = {0, 0, 1};
        uint64_t f_array[] = {1};
        pedigree common_stem_ped(cs_array, 0, false);
        pedigree left_start_ped(ls_array, 4, false);
        pedigree left_end_ped(le_array, 4, false);
        pedigree split_ped(s_array, 3, false);
        pedigree final_ped(f_array, 1, false);
        test_iview_merge_base<T>(3, common_stem_ped,
                                 left_start_ped,
                                 left_end_ped,
                                 split_ped,
                                 final_ped);
    }

    pass_test();
}

template <typename T>
void test_det_reducer_iview_merge()
{
    CILKTEST_REMARK(2, "test_det_reducer_iview_merge()...");

    // Cases that tripped errors in the past.
    test_det_reducer_iview_merge_helper<T>(0, 0, 1, 1);
    test_det_reducer_iview_merge_helper<T>(0, 4, 8, 0);
    
    for (uint64_t start_i = 0; start_i < 4; ++start_i) {
        for (uint64_t left_delta = 0; left_delta < 4; ++left_delta) {
            // Split delta better be at least 1.
            for (uint64_t split_delta = 1; split_delta < 9; ++split_delta) {
                for (uint64_t final_delta = 0; final_delta < 3; ++final_delta) {
                    test_det_reducer_iview_merge_helper<T>(start_i,
                                                           left_delta,
                                                           split_delta,
                                                           final_delta);
                }
            }
            CILKTEST_REMARK(3, ".");
        }
    }
    pass_test();
}

template <typename T>
void test_det_reducer_iview_merge_from_list()
{
    CILKTEST_REMARK(2, "test_det_reducer_iview_merge_from_list()...");
    pedigree common_ped;
    for (int start_i = 0; start_i < TESTPED_LENGTH; ++start_i) {
        pedigree start_ped = pedigree(PED_LIST[start_i].ped, 
                                                        PED_LIST[start_i].length,
                                                        false);
        CILKTEST_REMARK(2, ".");
        cilk_for (int left_end_i = start_i; left_end_i < TESTPED_LENGTH; ++left_end_i) {
            pedigree left_end_ped = pedigree(PED_LIST[left_end_i].ped,
                                                               PED_LIST[left_end_i].length,
                                                               false);
            if ((left_end_i+1) < TESTPED_LENGTH) {
                pedigree split_ped = pedigree(PED_LIST[left_end_i+1].ped,
                                                                PED_LIST[left_end_i+1].length,
                                                                false);


                for (int final_ped_i = left_end_i+1;
                     final_ped_i < TESTPED_LENGTH;
                     ++final_ped_i) {
                    pedigree final_ped = pedigree(PED_LIST[final_ped_i].ped,
                                                                    PED_LIST[final_ped_i].length,
                                                                    false);
                    CILKTEST_PRINT(4, "merge_from_list: start_ped = ", start_ped, "\n");
                    CILKTEST_PRINT(4, "merge_from_list: left_end_ped = ", left_end_ped, "\n");
                    CILKTEST_PRINT(4, "merge_from_list: split_ped = ", split_ped, "\n");
                    CILKTEST_PRINT(4, "merge_from_list: final_ped = ", final_ped, "\n");
                    test_iview_merge_base<T>(4, common_ped,
                                             start_ped,
                                             left_end_ped,
                                             split_ped,
                                             final_ped);
                }
            }
        }
    }
    pass_test();
}


template <typename T>
void test_det_reduce_iview_merge_generic(uint64_t stem_root)
{
    int base_verbosity = 2;
    // The common pedigree stem we are using for this test (stored as
    // a reverse pedigree).
    const int STEM_LENGTH = 1;
    uint64_t common_stem[STEM_LENGTH] = {100};
    common_stem[0] = stem_root;

    T expected_sum = 0;
    const int NUM_PED_TERMS = 3;
    const int MAX_RANK = 5;
    // We are going to encode small pedigrees as 64-bit ints, encoded
    // so that the ordering of the encoded ints matches the natural
    // ordering of pedigrees.
    uint64_t MAX_PED = SimpleTestPed::compute_max_ped(NUM_PED_TERMS, MAX_RANK);

    CILKTEST_REMARK(base_verbosity, "test_det_reduce_view_update[stem_root=%llu]\n", stem_root);

    pedigree common_stem_ped = pedigree(common_stem, STEM_LENGTH, true);
    DetRedIview<T> v(expected_sum, common_stem_ped);
    
    CILKTEST_REMARK(base_verbosity+1, "Starting with initial stem with length = %d: ", STEM_LENGTH);
    CILKTEST_PRINT(base_verbosity+1, "Common stem ped: ", common_stem_ped, "\n");
    CILKTEST_PRINT(base_verbosity+1, "DetRedIview is: ", v, "\n");

    CILKTEST_REMARK(2, "MAX_PED is %d\n", MAX_PED);

    // Here are a list of test cases that tripped errors before.
    //  start_ped_num=1, split_ped_num=2, final_ped_num=6
    //  start_ped_num=1, split_ped_num=3, final_ped_num=6
    //  start_ped_num=1, split_ped_num=6, final_ped_num=9
    //  start_ped_num=1, split_ped_num=2, final_ped_num=36
    //  start_ped_num=1, split_ped_num=6, final_ped_num=36

    int INIT_START = 0;    // 1
    int FINAL_START = MAX_PED/2;   // Really, I'd like to run until MAX_PED.
                           // But this test seems to take long enough as it is...
    int LEFT_DELTA = 0;
    int SPLIT_DELTA = 1;   // 6  - INIT_START;
    int FINAL_DELTA = 0;   // 36 - INIT_START - SPLIT_DELTA;
    
    int num_test_cases = 0;
    for (uint64_t start_ped_num = INIT_START; start_ped_num < FINAL_START; start_ped_num+=1) {
        uint64_t ped1[NUM_PED_TERMS + STEM_LENGTH];
        int ped1_length;
        SimpleTestPed::fill_rev_ped_from_int(start_ped_num, MAX_RANK, NUM_PED_TERMS,
                                             ped1, &ped1_length,
                                             common_stem, STEM_LENGTH);
        pedigree start_ped = pedigree(ped1, ped1_length, true);

        CILKTEST_REMARK(base_verbosity, "\nOuter loop: start_ped_num = %ld\n", start_ped_num);
        _Cilk_for (uint64_t left_end_ped_num = start_ped_num + LEFT_DELTA;
                   left_end_ped_num < MAX_PED;
                   left_end_ped_num++) {
            uint64_t left_end_p[NUM_PED_TERMS + STEM_LENGTH];
            int left_end_p_length;
            SimpleTestPed::fill_rev_ped_from_int(left_end_ped_num, MAX_RANK, NUM_PED_TERMS,
                                                 left_end_p, &left_end_p_length,
                                                 common_stem, STEM_LENGTH);
            pedigree left_end_ped = pedigree(left_end_p, left_end_p_length, true);
            CILKTEST_REMARK(base_verbosity, ".");
            _Cilk_for (uint64_t split_ped_num = left_end_ped_num+SPLIT_DELTA;
                       split_ped_num < MAX_PED;
                       split_ped_num++) {
                uint64_t split_p[NUM_PED_TERMS + STEM_LENGTH];
                int split_p_length;
                SimpleTestPed::fill_rev_ped_from_int(split_ped_num, MAX_RANK, NUM_PED_TERMS,
                                                     split_p, &split_p_length,
                                                     common_stem, STEM_LENGTH);
                pedigree split_ped = pedigree(split_p, split_p_length, true);

                for (uint64_t final_ped_num = split_ped_num+FINAL_DELTA; final_ped_num < MAX_PED; final_ped_num++) {
                    uint64_t final_p[NUM_PED_TERMS + STEM_LENGTH];
                    int final_p_length;
                    SimpleTestPed::fill_rev_ped_from_int(final_ped_num, MAX_RANK, NUM_PED_TERMS,
                                                         final_p, &final_p_length,
                                                         common_stem, STEM_LENGTH);
                    pedigree final_ped = pedigree(final_p, final_p_length, true);

                    CILKTEST_REMARK(base_verbosity+1,
                                    "start_ped_num=%d, left_end_ped_num=%d, split_ped_num=%d, final_ped_num=%d\n",
                                    start_ped_num,
                                    left_end_ped_num,
                                    split_ped_num,
                                    final_ped_num);
                    CILKTEST_PRINT(base_verbosity+1, "start_ped: ", start_ped, "\n");
                    CILKTEST_PRINT(base_verbosity+1, "left_ped: ", left_end_ped, "\n");
                    CILKTEST_PRINT(base_verbosity+1, "split_ped: ", split_ped, "\n");
                    CILKTEST_PRINT(base_verbosity+1, "final_ped: ", final_ped, "\n");                
                    TEST_ASSERT(start_ped <= left_end_ped);
                    TEST_ASSERT(left_end_ped < split_ped);
                    TEST_ASSERT(split_ped <= final_ped);

                    num_test_cases++;
                    test_iview_merge_base<T>(base_verbosity,
                                             start_ped,
                                             start_ped, left_end_ped,
                                             split_ped, final_ped);
                }
            }
        }
    }
    fprintf(stderr, "Total number of test cases: %d\n", num_test_cases);

    CILKTEST_REMARK(base_verbosity,  "Final range with sum %f \n", v.get_value());
    CILKTEST_PRINT(base_verbosity, "Final view: ", v, "\n");
}




template <typename T>
void test_detred_iview_all()
{
    test_det_reduce_view_basic<T>();
    test_det_reducer_iview_update_simple<T>();
    test_det_reducer_iview_update_simple2<T>();
    test_det_reducer_iview_update_simple3<T>();

    test_det_reducer_view_update<T>();
    test_det_reducer_view_update_from_list<T>();

    test_det_reducer_iview_merge<T>();
    test_det_reducer_iview_merge_simple<T>();

    // A useful test, but takes a long time.
    test_det_reducer_iview_merge_from_list<T>();

    // A bit redundant, given the previous test.
    //    test_det_reduce_iview_merge_generic<T>(0);
}

int main(int argc, char* argv[]) {
    CILKTEST_PARSE_TEST_ARGS(argc, argv);
    CILKTEST_BEGIN("Determinsitic reducer iviews");
    
    test_detred_iview_all<double>();

    // Generate test pedigree list.
    //     SimpleTestPed::generate_test_ped_list(5, 2);

    return CILKTEST_END("Determinsitic reducer iviews");
}
