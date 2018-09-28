/* test_pedigrees.cpp                  -*-C++-*-
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
 * @file test_pedigrees.cpp
 *
 * @brief Test pedigree module.
 */

#include <cilkpub/pedigrees.h>

#include <assert.h>
#include <cstdio>
#include <cstdlib>
#include <cilk/cilk.h>
#include <cilk/reducer_list.h>
#include "cilktest_harness.h"

template <int STATIC_PED_LENGTH>
inline void tfprint(FILE* f,
                    cilkpub::opt_pedigree<STATIC_PED_LENGTH>& ped)
{
    ped.fprint(f, "");
}

template <int STATIC_PED_LENGTH>
inline void tfprint(FILE* f,
                    cilkpub::opt_pedigree_scope<STATIC_PED_LENGTH>& scope)
{
    scope.fprint(f, "");
}

/**
 * @brief Validate the comparisons between ped1 and ped2.
 * Pass in @c expected_result of  -1 for @c ped1 <  @c ped2
 *                                 0 for @c ped1 == @c ped2
 *                                 1 for @c ped1 >  @c ped2
 */
template <int STATIC_PED_LENGTH>
int comparison_test_helper(cilkpub::opt_pedigree<STATIC_PED_LENGTH>& ped1,
                           cilkpub::opt_pedigree<STATIC_PED_LENGTH>& ped2,
                           int expected_result)
{
    int initial_error_count = CILKTEST_NUM_ERRORS();
    int res = ped1.compare(ped2);

    TEST_ASSERT(res == expected_result);
    if (res != expected_result) {
        REPORT("ERROR: different results... ped1 = ");
        CILKTEST_PRINT(0, "Ped 1 is ", ped1, "\n");
        CILKTEST_PRINT(0, "Ped 2 is ", ped2, "\n");
    }
    
    switch (expected_result) {
        // Less than:
    case -1:
        TEST_ASSERT( (ped1 <  ped2) );
        TEST_ASSERT( (ped1 <= ped2) );
        TEST_ASSERT(!(ped1 == ped2) );
        TEST_ASSERT( (ped1 != ped2) );
        TEST_ASSERT(!(ped1 >= ped2) );
        TEST_ASSERT(!(ped1 >  ped2) );
        break;
    case 0:
        TEST_ASSERT(!(ped1 <  ped2) );
        TEST_ASSERT( (ped1 <= ped2) );
        TEST_ASSERT( (ped1 == ped2) );
        TEST_ASSERT(!(ped1 != ped2) );
        TEST_ASSERT( (ped1 >= ped2) );
        TEST_ASSERT(!(ped1 >  ped2) );
        break;
    case 1:
        TEST_ASSERT(!(ped1 <  ped2) );
        TEST_ASSERT(!(ped1 <= ped2) );
        TEST_ASSERT(!(ped1 == ped2) );
        TEST_ASSERT( (ped1 != ped2) );
        TEST_ASSERT( (ped1 >= ped2) );
        TEST_ASSERT( (ped1 >  ped2) );
        break;
    }

    return CILKTEST_NUM_ERRORS() - initial_error_count;
}


/**
 * This test looks for an initial pedigree of [0, 0].
 * Thus we can only call it once, on the first pthread in a program. 
 */                                
template <int STATIC_PED_LENGTH>
void test_initial_pedigree() {
    CILKTEST_REMARK(2, "Checking root pedigree\n");
    cilkpub::opt_pedigree<STATIC_PED_LENGTH> root_ped = cilkpub::opt_pedigree<STATIC_PED_LENGTH>::current();
    CILKTEST_PRINT(3, "Root pedigree: ", root_ped, "\n");
    TEST_ASSERT(root_ped.length() == 2);
    TEST_ASSERT(root_ped[0] == 0);
    TEST_ASSERT(root_ped[1] == 0);
}


#define MAX_PED_LENGTH 100
struct pedigree_testcase {
    uint64_t a[MAX_PED_LENGTH];
    int length;
};


// An global array of pedigree test cases.
const int NUM_TEST_PEDS = 17;
pedigree_testcase test_peds[NUM_TEST_PEDS] = {
    { {},                 0},
    { {0},                1},
    { {0, 0},             2},
    { {0, 0, 0},          3},
    { {0, 3},             2},
    { {1, 0},             2},
    { {1, 0, 4, 0},       4},
    { {1, 0, 4, 0, 1},    5},
    { {1, 0, 4, 2},       4},
    { {1, 0, 4, 2, 1},    5},
    { {1, 0, 4, 3},       4},
    { {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13},  13},
    { {2, 1, 3},          3},
    { {(uint64_t)(-1)-1}, 2},
    { {(uint64_t)(-1)},   1},
    { {(uint64_t)(-1), (uint64_t)(-1)}, 2},
    { {(uint64_t)(-1), (uint64_t)(-1), (uint64_t)(-1)}, 3},
};



template <int STATIC_PED_LENGTH>
void test_pedigree_prefix_and_scope()
{
    CILKTEST_REMARK(2, "Checking pedigree prefix and scope\n");
    cilkpub::opt_pedigree<STATIC_PED_LENGTH> empty_ped;
    uint64_t p1_array[3] = {1, 0, 4};
    uint64_t p2_array[4] = {1, 0, 4, 2};
    uint64_t p3_array[5] = {1, 0, 4, 2, 7};
    uint64_t p4_array[5] = {1, 0, 4, 1, 7};
    uint64_t p5_array[6] = {1, 0, 4, 3, 7, 6};

    cilkpub::opt_pedigree<STATIC_PED_LENGTH> p1 = cilkpub::opt_pedigree<STATIC_PED_LENGTH>(p1_array, 3, false);
    cilkpub::opt_pedigree<STATIC_PED_LENGTH> p2 = cilkpub::opt_pedigree<STATIC_PED_LENGTH>(p2_array, 4, false);
    cilkpub::opt_pedigree<STATIC_PED_LENGTH> p3 = cilkpub::opt_pedigree<STATIC_PED_LENGTH>(p3_array, 5, false);
    cilkpub::opt_pedigree<STATIC_PED_LENGTH> p4 = cilkpub::opt_pedigree<STATIC_PED_LENGTH>(p4_array, 4, false);
    cilkpub::opt_pedigree<STATIC_PED_LENGTH> p5 = cilkpub::opt_pedigree<STATIC_PED_LENGTH>(p5_array, 4, false);

    TEST_ASSERT(empty_ped.is_prefix_of(p1));
    TEST_ASSERT(!p1.is_prefix_of(empty_ped));
    TEST_ASSERT(p1.in_scope_of(empty_ped));
    TEST_ASSERT(!empty_ped.in_scope_of(p1));

    TEST_ASSERT(p1.is_prefix_of(p2));
    TEST_ASSERT(!p2.is_prefix_of(p1));
    TEST_ASSERT(!p1.in_scope_of(p2));
    TEST_ASSERT(p2.in_scope_of(p1));

    TEST_ASSERT(p1.is_prefix_of(p3));
    TEST_ASSERT(!p3.is_prefix_of(p1));
    TEST_ASSERT(!p1.in_scope_of(p3));
    TEST_ASSERT(p3.in_scope_of(p1));
    TEST_ASSERT(p2.is_prefix_of(p3));
    TEST_ASSERT(!p3.is_prefix_of(p2));
    TEST_ASSERT(p3.in_scope_of(p2));
    TEST_ASSERT(!p2.in_scope_of(p3));

    // You are always a prefix / in scope of yourself.
    TEST_ASSERT(p1.is_prefix_of(p1));
    TEST_ASSERT(p1.in_scope_of(p1));
    
    TEST_ASSERT(p1.is_prefix_of(p5));
    TEST_ASSERT(p5.in_scope_of(p1));
    TEST_ASSERT(!p1.in_scope_of(p5));
    
    TEST_ASSERT(p1.is_prefix_of(p4));
    TEST_ASSERT(p4.in_scope_of(p1));
    TEST_ASSERT(!p1.in_scope_of(p4));
    

    TEST_ASSERT(!p2.is_prefix_of(p5));
    TEST_ASSERT(p5.in_scope_of(p2));
    TEST_ASSERT(!p5.is_prefix_of(p2));
    

    TEST_ASSERT(!p4.is_prefix_of(p5));
    TEST_ASSERT(!p5.is_prefix_of(p4));
    TEST_ASSERT(p5.in_scope_of(p4));
    TEST_ASSERT(!p4.in_scope_of(p5));

    TEST_ASSERT(!p3.is_prefix_of(p4));
    TEST_ASSERT(!p4.is_prefix_of(p3));
    TEST_ASSERT(p3.in_scope_of(p4));
    TEST_ASSERT(!p4.in_scope_of(p3));
}

template <int STATIC_PED_LENGTH>
void test_pedigree_comparison()
{
    CILKTEST_REMARK(2, "Checking pedigree comparison \n");

    const int L=4;
    uint64_t ped1_array[L] = {1, 0, 4, 3};
    uint64_t ped1_rev_array[L] = {3, 4, 0, 1};
    uint64_t ped2_array[L] = {1, 0, 4, 0};
    cilkpub::opt_pedigree<STATIC_PED_LENGTH> ped1 = cilkpub::opt_pedigree<STATIC_PED_LENGTH>(ped1_array, L, false);
    cilkpub::opt_pedigree<STATIC_PED_LENGTH> ped1_rev = cilkpub::opt_pedigree<STATIC_PED_LENGTH>(ped1_rev_array, L, true);
    comparison_test_helper(ped1, ped1_rev, 0);

    for(int i = 0; i < NUM_TEST_PEDS; ++i) {
        cilkpub::opt_pedigree<STATIC_PED_LENGTH> p1 = cilkpub::opt_pedigree<STATIC_PED_LENGTH>(test_peds[i].a, test_peds[i].length, false);

        CILKTEST_REMARK(3, "Comparison for pedigree # %d: ", i);
        CILKTEST_PRINT(3, "", p1, ") ...");

        int num_errors = 0;
        for (int j = 0; j < NUM_TEST_PEDS; ++j) {
            cilkpub::opt_pedigree<STATIC_PED_LENGTH> p2 = cilkpub::opt_pedigree<STATIC_PED_LENGTH>(test_peds[j].a, test_peds[j].length, false);
            if (i < j) {
                num_errors += comparison_test_helper(p1, p2, -1);
            }
            else if (i == j) {
                num_errors += comparison_test_helper(p1, p2, 0);
            }
            else {
                num_errors += comparison_test_helper(p1, p2, 1);
            }
        }

        TEST_ASSERT_MSG(num_errors == 0, " Comparison check...");
        if (num_errors == 0) {
            CILKTEST_REMARK(3, "PASSED\n");
        }
    }
}

template <int STATIC_PED_LENGTH>
void test_copy() 
{
    CILKTEST_REMARK(2, "Checking assignment and copy constructor\n");
    const int L = 4;
    uint64_t ped_array[L] = {1, 0, 4, 3};
    cilkpub::opt_pedigree<STATIC_PED_LENGTH> ped1 = cilkpub::opt_pedigree<STATIC_PED_LENGTH>(ped_array, L, false);
    cilkpub::opt_pedigree<STATIC_PED_LENGTH> ped2 = cilkpub::opt_pedigree<STATIC_PED_LENGTH>(ped1);
    cilkpub::opt_pedigree<STATIC_PED_LENGTH> ped3 = cilkpub::opt_pedigree<STATIC_PED_LENGTH>(ped1);
    TEST_ASSERT_EQ(ped2, ped1);
    TEST_ASSERT_EQ(ped3, ped1);

    uint64_t long_ped[100];
    for (int i = 0; i < 100; ++i) {
        long_ped[i] = i;
    }

    cilkpub::opt_pedigree<STATIC_PED_LENGTH> *lp1 = new cilkpub::opt_pedigree<STATIC_PED_LENGTH>(long_ped, 100, false);
    cilkpub::opt_pedigree<STATIC_PED_LENGTH> *lp2 = new cilkpub::opt_pedigree<STATIC_PED_LENGTH>(*lp1);
    TEST_ASSERT_EQ(*lp1, *lp2);

    CILKTEST_PRINT(4, "LP1 before free: ", *lp1, "\n");
    CILKTEST_PRINT(4, "LP2 before free: ", *lp2, "\n");
    delete lp1;

    CILKTEST_PRINT(4, "LP2 after free: ", *lp2, "\n");

    cilkpub::opt_pedigree<STATIC_PED_LENGTH> *lp3 = new cilkpub::opt_pedigree<STATIC_PED_LENGTH>(*lp2);

    CILKTEST_PRINT(4, "LP3 create: ", *lp3, "\n");
    CILKTEST_PRINT(4, "LP2 after copy: ", *lp2, "\n");

    TEST_ASSERT_EQ(*lp3, *lp2);
    delete lp2;
    delete lp3;
}

// Check simple assignments 
template <int STATIC_PED_LENGTH>
void test_assignment_simple()
{

    CILKTEST_REMARK(2, "Checking simple assignments and copies\n");
    const int L = STATIC_PED_LENGTH * 2;
    uint64_t ped_array[L];
    for (int i = 0; i < L; ++i) {
        ped_array[i] = 3*i + 1;
    }

    // Pedigree declared outside the loop --- gets assigned to
    // multiple times.
    cilkpub::opt_pedigree<STATIC_PED_LENGTH> global_tmp;

    // Test assignment with pedigrees of an increasing length.
    for (int i = 1; i < L; ++i) {
        cilkpub::opt_pedigree<STATIC_PED_LENGTH> local(ped_array, i, false);
        cilkpub::opt_pedigree<STATIC_PED_LENGTH> local_tmp;
        cilkpub::opt_pedigree<STATIC_PED_LENGTH> local_copy;

        // Copy an existing object.
        local_copy = local;

        // Assignments.
        local_tmp = cilkpub::opt_pedigree<STATIC_PED_LENGTH>(ped_array, i, false);
        global_tmp = cilkpub::opt_pedigree<STATIC_PED_LENGTH>(ped_array, i, false);

        TEST_ASSERT_EQ(local, local_copy);        
        TEST_ASSERT_EQ(local, local_tmp);
        TEST_ASSERT_EQ(local, global_tmp);
    }

    // Now test assignment / copy with pedigrees of decreasing length.
    for (int i = L-1; i >= 0; --i) {
        cilkpub::opt_pedigree<STATIC_PED_LENGTH> local(ped_array, i, false);
        cilkpub::opt_pedigree<STATIC_PED_LENGTH> local_tmp;
        cilkpub::opt_pedigree<STATIC_PED_LENGTH> local_copy;

        // Copy an existing object.
        local_copy = local;

        // Assignments.
        local_tmp = cilkpub::opt_pedigree<STATIC_PED_LENGTH>(ped_array, i, false);
        global_tmp = cilkpub::opt_pedigree<STATIC_PED_LENGTH>(ped_array, i, false);

        TEST_ASSERT_EQ(local, local_copy);        
        TEST_ASSERT_EQ(local, local_tmp);
        TEST_ASSERT_EQ(local, global_tmp);
    }
}

template <int STATIC_PED_LENGTH>
void test_iteration()
{
    CILKTEST_REMARK(2, "Checking pedigree iteration\n");
    const int L = 5;
    uint64_t ped_array[L] = {0, 1, 2, 3, 4};

    {
        // Walk over const pedigree backwards and forwards.
        int i = 0;
        const cilkpub::opt_pedigree<STATIC_PED_LENGTH> cped
            = cilkpub::opt_pedigree<STATIC_PED_LENGTH>(ped_array, L, false);

        CILKTEST_REMARK(3, "Iterate ped in reverse: \n");
        for (typename cilkpub::opt_pedigree<STATIC_PED_LENGTH>::const_reverse_iterator it = cped.rbegin();
             it < cped.rend();
             ++it, ++i) {
            CILKTEST_REMARK(3, "(%d,  %ld) ", i, *it);
            assert(*it == (L-1 - i));
        }
        CILKTEST_REMARK(3, "\n");

        i = 0;
        CILKTEST_REMARK(3, "Iterate ped in forward order: \n");
        for (typename cilkpub::opt_pedigree<STATIC_PED_LENGTH>::const_iterator it = cped.begin();
             it < cped.end();
             ++it, ++i) {
            CILKTEST_REMARK(3, "(%d,  %ld) ", i, *it);
            assert(*it == i);
        }
        CILKTEST_REMARK(3, "\n");
    }


    // TBD(jsukha): For now, I'm going to assume the pedigree object is immutable.
    // Maybe some day, we will find a reason to allow the pedigree object to mutate.
    // But for now, I'll keep it immutable.
    // 
    // {
    //     // Walk over pedigree backwards and mutate. 
    //     int i = 0;
    //     cilkpub::opt_pedigree<STATIC_PED_LENGTH> pedr = cilkpub::opt_pedigree<STATIC_PED_LENGTH>(ped_array, L, false);
    //     CILKTEST_REMARK(3, "Iterate ped in reverse and mutate: \n");
    //     for (cilkpub::opt_pedigree<STATIC_PED_LENGTH>::reverse_iterator it = pedr.rbegin();
    //          it < pedr.rend();
    //          ++it, ++i) {

    //         int tmp = 10*i;
    //         (*it) += tmp;
    //         CILKTEST_REMARK(3, "(%d,  %ld) ", i, *it);
    //         assert(*it == (L-1 - i + tmp));
    //     }
    //     CILKTEST_REMARK(3, "\n");
    // }
    
    // {
    //     // Walk over pedigree forwards and mutate.
    //     int i = 0;
    //     cilkpub::opt_pedigree<STATIC_PED_LENGTH> pedf = cilkpub::opt_pedigree<STATIC_PED_LENGTH>(ped_array, L, false);
    //     CILKTEST_REMARK(3, "Iterate ped in forward order and mutate: \n");
    //     for (cilkpub::opt_pedigree<STATIC_PED_LENGTH>::iterator it = pedf.begin();
    //          it < pedf.end();
    //          ++it, ++i) {

    //         int tmp = 10*i;
    //         (*it) += tmp;

    //         CILKTEST_REMARK(3, "(%d,  %ld) ", i, *it);
    //         assert(*it == (i + tmp));
    //     }
    //     CILKTEST_REMARK(3, "\n");
    // }
}

// Create a list of the pedigrees as we execute (add to a reducer).
template <int STATIC_PED_LENGTH>
int test_fib_v1(int n,
                cilk::reducer_list_append<cilkpub::opt_pedigree<STATIC_PED_LENGTH> >* ped_list)
{
    if (n < 2) {
        // Fetch the current pedigree.
        cilkpub::opt_pedigree<STATIC_PED_LENGTH> current_ped = cilkpub::opt_pedigree<STATIC_PED_LENGTH>::current();
        CILKTEST_PRINT(6, "Fib base: pedigree is ", current_ped, "\n");
        ped_list->push_back(current_ped);
        return n;
    }

    int x, y;
    cilkpub::opt_pedigree<STATIC_PED_LENGTH> p1 = cilkpub::opt_pedigree<STATIC_PED_LENGTH>::current();
    CILKTEST_PRINT(6, "p1: pedigree is ", p1, "\n");
    ped_list->push_back(p1);

    x = _Cilk_spawn test_fib_v1(n-1, ped_list);

    cilkpub::opt_pedigree<STATIC_PED_LENGTH> p2 = cilkpub::opt_pedigree<STATIC_PED_LENGTH>::current();
    CILKTEST_PRINT(6, "p2: pedigree is ", p2, "\n");
    ped_list->push_back(p2);
    
    y = test_fib_v1(n-2, ped_list);

    cilkpub::opt_pedigree<STATIC_PED_LENGTH> p3 = cilkpub::opt_pedigree<STATIC_PED_LENGTH>::current();
    CILKTEST_PRINT(6, "p3: pedigree is ", p3, "\n");
    ped_list->push_back(p3);
    _Cilk_sync;

    p1 = cilkpub::opt_pedigree<STATIC_PED_LENGTH>::current();
    CILKTEST_PRINT(6, "p1 again: pedigree is ", p1, "\n");
    ped_list->push_back(p1);
    return (x+y);
}


template <int STATIC_PED_LENGTH>
int fib_copy_array_test(int n, int max_ped_length, bool do_print) {
    if (n < 2) {
        cilkpub::opt_pedigree<STATIC_PED_LENGTH> current_ped = cilkpub::opt_pedigree<STATIC_PED_LENGTH>::current();
        uint64_t* test_in_order = new uint64_t[max_ped_length];
        uint64_t* test_rev_order = new uint64_t[max_ped_length];

        int x1 = current_ped.copy_to_array(test_in_order, max_ped_length);
        int x2 = current_ped.copy_reverse_to_array(test_rev_order, max_ped_length);

        if (do_print) {
            CILKTEST_PRINT(5, "Flatten test: ", current_ped, "\n");
        }
        TEST_ASSERT(x1 == x2);
        TEST_ASSERT(x1 == current_ped.length());

        for (int i = 0; i < x1; ++i) {
            if (do_print) {
                CILKTEST_REMARK(4, "Term %d: %lu\n", i, test_in_order[i]);
            }
            TEST_ASSERT(test_in_order[i] == test_rev_order[x1-1-i]);
        }
        delete[] test_in_order;
        delete[] test_rev_order;
        return n;
    }

    int x, y;
    x = _Cilk_spawn fib_copy_array_test<STATIC_PED_LENGTH>(n-1, max_ped_length, do_print);
    y = fib_copy_array_test<STATIC_PED_LENGTH>(n-2, max_ped_length, do_print);
    _Cilk_sync;
    return (x+y);
}

template <int STATIC_PED_LENGTH>
void test_pedigrees_copy_array() {
    CILKTEST_REMARK(2, "Testing copy_array methods for pedigrees \n");
    for (int n = 0; n < 20; ++n) {
        CILKTEST_REMARK(4, "Flatten test, n = %d\n", n);
        int ans1, ans2;
        int max_ped_length = 100;
        ans1 = fib_copy_array_test<STATIC_PED_LENGTH>(n, max_ped_length, n < 5);
        ans2 = fib_copy_array_test<STATIC_PED_LENGTH>(n, max_ped_length, n < 5);
        TEST_ASSERT(ans2 == ans1);
    }
}

template <int STATIC_PED_LENGTH>
void validate_list_of_pedigrees_in_order(const std::list<cilkpub::opt_pedigree<STATIC_PED_LENGTH> >& r)
{
    bool do_print = (CILKTEST_VERBOSITY() >= 5) && (r.size() <= 500);
    typename std::list<cilkpub::opt_pedigree<STATIC_PED_LENGTH> >::const_iterator i, j;
    i = r.begin();
    j = r.begin();

    if (j != r.end()) {
        if (do_print) {
            CILKTEST_PRINT(5, "List of pedigrees: \n",  *i, "\n");
        }
        j++;
    }

    while (j != r.end()) {
        if (do_print) {
            CILKTEST_PRINT(5, "", *i, "\n");
        }
        TEST_ASSERT( (*i) <= (*j) );
        i++;
        j++;
    }
    if (do_print) {
        CILKTEST_REMARK(5, "\n");
    }
}

int fib(int n) {
    if (n < 2) {
        return n;
    }
    int x, y;
    x = _Cilk_spawn fib(n-1);
    y = fib(n-2);
    _Cilk_sync;
    return (x+y);
}


template <int STATIC_PED_LENGTH>
cilkpub::opt_pedigree<STATIC_PED_LENGTH>
fib_with_scoped_pedigree(cilkpub::opt_pedigree_scope<STATIC_PED_LENGTH>& root_scope,
                         int n)
{
    if (n < 2) {
        return cilkpub::opt_pedigree<STATIC_PED_LENGTH>::current(root_scope);
    }
    else {
        int x, y;
        cilkpub::opt_pedigree<STATIC_PED_LENGTH> ans;
        x = _Cilk_spawn fib(n-1);
        y = fib(n-2);
        _Cilk_sync;
        ans = cilkpub::opt_pedigree<STATIC_PED_LENGTH>::current(root_scope);
        return ans;
    }
}

template <int STATIC_PED_LENGTH>
void test_scoped_pedigree()
{
    const int N = 10;
    cilkpub::opt_pedigree<STATIC_PED_LENGTH> p1[N];
    cilkpub::opt_pedigree<STATIC_PED_LENGTH> p2[N];
    cilkpub::opt_pedigree_scope<STATIC_PED_LENGTH> scope1
        = cilkpub::opt_pedigree_scope<STATIC_PED_LENGTH>::current();
    cilkpub::opt_pedigree<STATIC_PED_LENGTH> c1
        = cilkpub::opt_pedigree<STATIC_PED_LENGTH>::current();
    cilkpub::opt_pedigree<STATIC_PED_LENGTH> cs1
        = cilkpub::opt_pedigree<STATIC_PED_LENGTH>::current(scope1);

    CILKTEST_PRINT(3, "Scope1: ", scope1, "\n");
    CILKTEST_PRINT(3, "First loop full pedigree: ", c1, "\n");
    CILKTEST_PRINT(3, "First loop scoped pedigree: ", cs1, "\n");
    _Cilk_for(int i = 0; i < N; ++i) {
        p1[i] = fib_with_scoped_pedigree(scope1, 7);
    }

    cilkpub::opt_pedigree_scope<STATIC_PED_LENGTH> scope2
        = cilkpub::opt_pedigree_scope<STATIC_PED_LENGTH>::current();
    cilkpub::opt_pedigree<STATIC_PED_LENGTH> c2
        = cilkpub::opt_pedigree<STATIC_PED_LENGTH>::current();
    cilkpub::opt_pedigree<STATIC_PED_LENGTH> cs2
        = cilkpub::opt_pedigree<STATIC_PED_LENGTH>::current(scope2);
    CILKTEST_PRINT(3, "First loop full pedigree: ", c2, "\n");
    CILKTEST_PRINT(3, "First loop scoped pedigree: ", cs2, "\n");
    _Cilk_for(int i = 0; i < N; ++i) {
        p2[i] = fib_with_scoped_pedigree(scope2, 7);
    }    


    CILKTEST_PRINT(4, "c1 ", c1, "\n");
    CILKTEST_PRINT(4, "c2 ", c2, "\n");
    CILKTEST_PRINT(4, "cs1 ", cs1, "\n");
    CILKTEST_PRINT(4, "cs2 ", cs2, "\n");
    TEST_ASSERT(cs1 == cs2);

    for (int i = 0; i < N; ++i) {
        CILKTEST_PRINT(4, "Pedigree ", p1[i], "\n");
        CILKTEST_PRINT(4, "Pedigree ", p2[i], "\n");
        TEST_ASSERT(p1[i] == p2[i]);
    }
}

template <int STATIC_PED_LENGTH>
void test_fib_run() {
    CILKTEST_REMARK(2, "Checking pedigree order for run of fib\n");
    // Test pedigrees for small values of n.
    for (int n = 0; n < 20; ++n) {
        int ans, ans2;

        cilk::reducer_list_append<cilkpub::opt_pedigree<STATIC_PED_LENGTH> > ped_list;
        CILKTEST_REMARK(3, "Testing fib(%d)...\n", n);
        cilkpub::opt_pedigree<STATIC_PED_LENGTH> before_r1 = cilkpub::opt_pedigree<STATIC_PED_LENGTH>::current();
        ans = test_fib_v1(n, &ped_list);
        cilkpub::opt_pedigree<STATIC_PED_LENGTH> after_r1 = cilkpub::opt_pedigree<STATIC_PED_LENGTH>::current();

        CILKTEST_PRINT(3, "Before r1: ", before_r1, "\n");
        CILKTEST_PRINT(3, "After r1: ", after_r1, "\n");
        
        const std::list<cilkpub::opt_pedigree<STATIC_PED_LENGTH> >& r1 = ped_list.get_value();
        CILKTEST_REMARK(3, "Number of pedigrees for fib(%d): %d\n",
               n, r1.size());
        validate_list_of_pedigrees_in_order(r1);


        cilk::reducer_list_append<cilkpub::opt_pedigree<STATIC_PED_LENGTH> > ped_list2;
        CILKTEST_REMARK(3, "Repeating fib(%d) again...\n", n);
        cilkpub::opt_pedigree<STATIC_PED_LENGTH> before_r2 = cilkpub::opt_pedigree<STATIC_PED_LENGTH>::current();
        ans2 = test_fib_v1(n, &ped_list2);
        cilkpub::opt_pedigree<STATIC_PED_LENGTH> after_r2 = cilkpub::opt_pedigree<STATIC_PED_LENGTH>::current();
        const std::list<cilkpub::opt_pedigree<STATIC_PED_LENGTH> >& r2 = ped_list2.get_value();
        CILKTEST_REMARK(3, "Number of pedigrees for fib(%d): %d\n",
               n, r2.size());
        validate_list_of_pedigrees_in_order(r2);

        CILKTEST_PRINT(3, "Before r2: ", before_r2, "\n");
        CILKTEST_PRINT(3, "After r2: ", after_r2, "\n");

        CILKTEST_REMARK(3, "r1.size() == %d, r2.size() == %d\n", r1.size(), r2.size());
//        TEST_ASSERT_MSG(r1 == r2, "This test should fail!\n");

        typename std::list<cilkpub::opt_pedigree<STATIC_PED_LENGTH> >::const_iterator i, j;
        i = r1.begin();
        j = r2.begin();

        if (r1.size() < 100) {
            int k = 0;
            while (i != r1.end()) {
                CILKTEST_REMARK(4, "Pedigree %d: ", k);
                CILKTEST_PRINT(4, "From i: ", *i, ",   ");
                CILKTEST_PRINT(4, "From j: ", *j, "\n  ");
                i++;
                j++;
                k++;
            }

            TEST_ASSERT(k == r1.size());
            assert(i == r1.end());
            assert(j == r2.end());
        }


        // TBD: This assert is obviously wrong.
        // What should be true, however, is the "difference" between all the pedigrees
        // should be the same.
        // TEST_ASSERT_MSG(r1 == r2, "This test should fail!\n");
        
        TEST_ASSERT(ans2 == ans);
        TEST_ASSERT(r2.size() == r1.size());                
    }
}

template <int STATIC_PED_LENGTH>
void run_pedigree_tests()
{
    CILKTEST_REMARK(2, "Pedigree tests: STATIC_PED_LENGTH=%d\n", STATIC_PED_LENGTH);
    test_pedigree_comparison<STATIC_PED_LENGTH>();
    test_copy<STATIC_PED_LENGTH>();
    test_assignment_simple<STATIC_PED_LENGTH>();
    test_pedigree_prefix_and_scope<STATIC_PED_LENGTH>();
    test_iteration<STATIC_PED_LENGTH>();
    test_pedigrees_copy_array<STATIC_PED_LENGTH>();
    test_fib_run<STATIC_PED_LENGTH>();
    test_scoped_pedigree<STATIC_PED_LENGTH>();
}

/**
 *@brief Tests the declaration and use of the non-templated objects
 * i.e., the @c pedigree and @c pedigree_scope objects
 */            
void declare_and_test_default_pedigrees()
{
    cilkpub::pedigree final_ped = cilkpub::pedigree::current();
    CILKTEST_PRINT(2, "Final pedigree is ", final_ped, "\n");
    cilkpub::pedigree_scope test_scope = cilkpub::pedigree_scope::current();
    CILKTEST_PRINT(2, "Final scope is ", test_scope, "\n");
}

void force_cilk_runtime_start() {
    CILKTEST_REMARK(2, "Forcing start of Cilk runtime\n");
}

int main(int argc, char* argv[]) {
    CILKTEST_PARSE_TEST_ARGS(argc, argv);
    CILKTEST_BEGIN("pedigrees");

#if __INTEL_COMPILER < 1300
    // In the v12.1 compiler, pedigrees only work inside a Cilk
    // region.
    cilk_spawn force_cilk_runtime_start();
    cilk_sync;
#else    
    // This test looks for a pedigree of [0, 0].
    // We can only run it once.
    test_initial_pedigree<cilkpub::DEFAULT_STATIC_PED_LENGTH>();
#endif

    // Test different values of STATIC_PED_LENGTH,
    // starting with the default value. 
    run_pedigree_tests<cilkpub::DEFAULT_STATIC_PED_LENGTH>();
    run_pedigree_tests<1>();
    run_pedigree_tests<2>();
    run_pedigree_tests<3>();
    run_pedigree_tests<4>();
    run_pedigree_tests<5>();
    run_pedigree_tests<6>();
    run_pedigree_tests<7>();
    run_pedigree_tests<8>();
    run_pedigree_tests<9>();
    run_pedigree_tests<10>();

    declare_and_test_default_pedigrees();
    
    return CILKTEST_END("pedigrees");
}
