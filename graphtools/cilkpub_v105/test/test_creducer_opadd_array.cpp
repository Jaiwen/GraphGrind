/* test_creducer_opadd_array.cpp               -*-C++-*-
 *
 *************************************************************************
 *
 * Copyright (C) 2013-14, Intel Corporation
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

#include <cstdio>
#include <cstdlib>

#define CILK_IGNORE_REDUCER_ALIGNMENT

#include <cilk/cilk.h>
#include <cilk/reducer_opadd.h>
#include <cilkpub/creducer_opadd_array.h>

#include "cilktest_harness.h"
#include "cilktest_timing.h"


struct TimingPair {
    unsigned long long update;
    unsigned long long total;
    TimingPair() : update(0), total(0) {}
};


const int ARRAY_OF_REDUCERS = 1;
const int CRED_OPADD_ARRAY = 2;

/// An arbitrary hash of i and rep.
inline int random_hash(int i, int rep) {
    int q = (i+1) * (rep+1);
    for (int j = 0; j < 10; ++j) {
        q = q * (2*q + 1);
    }
    return q;
}


/// Test patterns for updating the reducer.  Do "num_updates" updates
/// in parallel to a reducer array of size L, repeating this process
/// num_reps times.

// Randomly access the entire array.
unsigned long long random_dense(int num_reps,
                                int num_updates,
                                int L,
                                bool do_sleep,
                                void* reducer,
                                void (*update_f)(void* red, size_t i, int val))
{
    unsigned long long update_time = CILKTEST_GETTICKS();
    for (int rep = 0; rep < num_reps; ++rep) {
        cilk_for(int i = 0; i < num_updates; ++i) {
            int q = random_hash(i, rep);
            q = q % L;    // The difference from quarter dense, is we mod over the entire array.
            if (q < 0) {
                q += L;
                assert((0 <= q) && (q < L));
            }
            update_f(reducer, q, i);
            if (do_sleep) {
                cilk_ms_sleep(1);
            }
        }
    }
    update_time = CILKTEST_GETTICKS() - update_time;
    return update_time;
}

// Quarter dense: randomly access the first 1/4 of the array.
unsigned long long quarter_dense(int num_reps,
                                 int num_updates,
                                 int L,
                                 bool do_sleep,
                                 void* reducer,
                                 void (*update_f)(void* red, size_t i, int val))
{
    unsigned long long update_time = CILKTEST_GETTICKS();
    for (int rep = 0; rep < num_reps; ++rep) {
        cilk_for(int i = 0; i < num_updates; ++i) {
            int q = random_hash(i, rep);
            q = q % L/4;
            if (q < 0) {
                q += L;
                assert((0 <= q) && (q < L));
            }
            update_f(reducer, q, i);
            if (do_sleep) {
                cilk_ms_sleep(1);
            }
        }
    }
    update_time = CILKTEST_GETTICKS() - update_time;
    return update_time;
}


// Each loop iteration accesses its own part of the array.
unsigned long long in_order(int num_reps,
                            int num_updates,
                            int L,
                            bool do_sleep,
                            void* reducer,
                            void (*update_f)(void* red, size_t i, int val))
{
    unsigned long long update_time = CILKTEST_GETTICKS();
    for (int rep = 0; rep < num_reps; ++rep) {
        cilk_for(int i = 0; i < num_updates; ++i) {
            update_f(reducer, i % L, i);
            if (do_sleep) {
                cilk_ms_sleep(1);
            }
        }
    }
    update_time = CILKTEST_GETTICKS() - update_time;
    return update_time;
}

// Each loop iteration accesses a small part of the array.
unsigned long long in_order_contention(int num_reps,
                                       int num_updates,
                                       int L,
                                       bool do_sleep,
                                       void* reducer,
                                       void (*update_f)(void* red, size_t i, int val))
{
    unsigned long long update_time = CILKTEST_GETTICKS();
    for (int rep = 0; rep < num_reps; ++rep) {
        cilk_for(int i = 0; i < num_updates; ++i) {
            update_f(reducer, i % (L/1000), i);
            if (do_sleep) {
                cilk_ms_sleep(1);
            }
        }
    }
    update_time = CILKTEST_GETTICKS() - update_time;
    return update_time;
}



inline void update_array_of_reducers(void* reducer, size_t i, int val)
{
    cilk::reducer_opadd<int>* red =
        (cilk::reducer_opadd<int>*)reducer;
    red[i] += val;
}

inline void update_creducer_array(void* reducer, size_t i, int val) {
    cilkpub::creducer_opadd_array<int>* cred =
        (cilkpub::creducer_opadd_array<int>*)reducer;
    (*cred)[i] += val;
}


template <int BSIZE, typename F>
TimingPair
test_creducers(int num_updates,
               int* x, int L,
               int reducer_type,
               bool do_sleep,
               const F& test_fun)
{
    int num_reps = 100;
    TimingPair times;

    times.total = CILKTEST_GETTICKS();

    switch (reducer_type) {
    case ARRAY_OF_REDUCERS:
        {
            // Ugly hack to get rid of alignment warning on
            // Windows.  Use aligned_malloc.
            // TBD: It would be nice to find a more portable way to
            // deal with this issue...
#ifdef _WIN32
            cilk::reducer_opadd<int>* expected_ans;
            expected_ans = (cilk::reducer_opadd<int>*)
                _aligned_malloc(sizeof(cilk::reducer_opadd<int>)*L,
                                64);
#else
            void* buffer;
            int ret = posix_memalign(&buffer,
                                     64,
                                     sizeof(cilk::reducer_opadd<int>)*L);
            assert(0 == ret);
            cilk::reducer_opadd<int>* expected_ans = (cilk::reducer_opadd<int>*)buffer;
#endif
            // Placement-new allocate the array.
            cilk_for(int i = 0; i < L; ++i) {
                new (&expected_ans[i]) cilk::reducer_opadd<int>();
            }
            
            times.update = test_fun(num_reps, num_updates, L,
                                    do_sleep, expected_ans,
                                    update_array_of_reducers);

            // We need to move out the value from each of the
            // individual reducers into the original array x now.
            // We'd like to do this in parallel, but we can't because
            // identities may get created when we access a reducer in
            // parallel!
            for (int i = 0; i < L; ++i) {
                x[i] += expected_ans[i].get_value();
            }

            // Placement-destructor for the reducer too.  We can't do
            // this in parallel, because we can't destroy reducers in
            // continuations...
            for(int i = 0; i < L; ++i)
            {
                expected_ans[i].~reducer_opadd<int>();
            }
            
#ifdef _WIN32
            _aligned_free(expected_ans);
#else
            free(expected_ans);
#endif
        }
        break;

    case CRED_OPADD_ARRAY:
        {
            cilkpub::creducer_opadd_array<int> ctest(x, L);
            times.update = test_fun(num_reps, num_updates, L,
                                    do_sleep, &ctest,
                                    update_creducer_array);
            ctest.move_out(x);
        }
        break;

    default:
        fprintf(stderr, "Undefined reducer type %d\n", reducer_type);
        assert(0);
    }

    times.total = CILKTEST_GETTICKS() - times.total;
    return times;
}



template <int L, int B, typename F>
void test_reducer_driver(bool do_sleep,
                         const F& test_pattern,
                         const char* pattern_desc,
                         double load_factor)
{
    int* creducer_x = new int[L];
    int* aor_x = new int[L];
    int P = __cilkrts_get_nworkers();

    int num_updates = (int)(load_factor * L);
    
    cilk_for(int i = 0; i < L; ++i) {
        aor_x[i] = i;
        creducer_x[i] = i;
    }

    TimingPair normal_time;
    TimingPair creducer_time;
    
    creducer_time = test_creducers<B>(num_updates, creducer_x, L, CRED_OPADD_ARRAY, do_sleep, test_pattern);
    normal_time = test_creducers<B>(num_updates, aor_x, L, ARRAY_OF_REDUCERS, do_sleep, test_pattern);
    
    CILKTEST_REMARK(2,
                    "***[CRED_OPADD_ARRAY,  L=%6d, B=%4d, P=%3d, Pattern=%s, NumUpdates=%6d, Sleep=%d, UpdateTime_in_ms=%5llu, TotalTime=%6llu]\n",
                    L, B, P,
                    pattern_desc,
                    num_updates,
                    do_sleep,
                    creducer_time.update, creducer_time.total);

    CILKTEST_REMARK(2,
                    "***[ARRAY_OF_REDUCERS, L=%6d, B=%4d, P=%3d, Pattern=%s, NumUpdates=%6d, Sleep=%d, UpdateTime_in_ms=%5llu, TotalTime=%6llu]\n",
                    L, B, P,
                    pattern_desc,
                    num_updates,
                    do_sleep,
                    normal_time.update, normal_time.total);


    int num_errors = 0;
    for (int i = 0; i < L; ++i) {
        TEST_ASSERT_EQ(aor_x[i], creducer_x[i]);
        if (aor_x[i] != creducer_x[i]) {
            fprintf(stderr, "ARRAY_OF_REDUCERS[%d] = %d, CRED_OPADD_ARRAY[%d] = %d\n",
                    i, aor_x[i],
                    i, creducer_x[i]);
            num_errors++;
        }
    }

    if (num_errors > 0) {
        fprintf(stderr, "FAILED\n");
    }
    assert(num_errors == 0);

    delete[] aor_x;
    delete[] creducer_x;
}


template <typename F>
void test_reducer_access_pattern(const F& test_pattern,
                                 const char* test_desc,
                                 bool time_test_only) 
{


    double test_load_factors[] = {0.001, 0.10, 0.25, 0.5, 1.0, 1.5, 2.0, 4.0, -1.0};
    int current = 0;

    while (test_load_factors[current] > 0) {
        double load = test_load_factors[current];
        current++;
        if (!time_test_only) {
            test_reducer_driver<40, 6>(true, test_pattern, test_desc, load);
            test_reducer_driver<400, 6>(true, test_pattern, test_desc, load);
            test_reducer_driver<4000, 100>(true, test_pattern, test_desc, load);
        }

        // No sleep for these larger tests.
        test_reducer_driver<4000, 100>(false, test_pattern, test_desc, load);
        test_reducer_driver<4000, 128>(false, test_pattern, test_desc, load);
        test_reducer_driver<4000, 1024>(false, test_pattern, test_desc, load);
        test_reducer_driver<4000, 4096>(false, test_pattern, test_desc, load);
        test_reducer_driver<10000, 4096>(false, test_pattern, test_desc, load);

        // These tests are interesting, but may take too long with normal reducers.
        // I probably need to fix the test driver...
        //        test_reducer_driver<100000, 4096>(false, test_pattern, test_desc, load);
        //        test_reducer_driver<200000, 4096>(false, test_pattern, test_desc, load);
        //        test_reducer_driver<1000000, 4096>(false, test_pattern, test_desc, load);
    }
}

int fib(int n) {
    if (n < 2)
        return n;
    int x, y;
    x = cilk_spawn fib(n-1);
    y = fib(n-2);
    cilk_sync;
    return (x+y);
}


inline int update_calc(int j) {
    return fib(20 + j % 5);
}

void update_reducer_test(cilkpub::creducer_opadd_array<int>& cr,
                         int L,
                         int num_reps) {
    cilk_for(int j = 0; j < num_reps; ++j) {
        cilk_for (int i = 0; i < L; ++i) {
            int ans = update_calc(j);
            cr[i] += ans;
        }
    }
}                         

void verify_expected(int* x, int* expected, int L) {
    cilk_for(int i = 0; i < L; ++i) {
        TEST_ASSERT_EQ(x[i], expected[i]);
    }
}


// Test moving in and out of the reducer array.
void test_move_creducer() {

    const int L = 100;
    int num_reps = 10;
    int* x = new int[L];
    int* y = new int[L];
    int* expected = new int[L];
    const int fib_start = 4;

    for (int i = 0; i < L; ++i) {
        x[i] = 0;
        y[i] = 0;
        expected[i] = 0;
    }

    for (int i = 0; i < L; ++i) {
        for (int j = 0; j < num_reps; ++j) {
            expected[i] += update_calc(j);
        }
    }


    // TEST 1: Update x and y, constructed from 
    //  move_in in different ways.

    // Construct with move-in automatically. 
    cilkpub::creducer_opadd_array<int> cr1(x, L);
    // Construct empty, move out implicitly. 
    cilkpub::creducer_opadd_array<int> cr2(L);

    // Update both reducer arrays.
    update_reducer_test(cr1, L, num_reps);
    update_reducer_test(cr2, L, num_reps);
    cr1.move_out(x);
    cr2.move_out(y);

    verify_expected(x, expected, L);
    verify_expected(y, expected, L);
    CILKTEST_REMARK(2, "Move creducer, test 1 PASSED\n");

    // TEST 2: Try again on only cr2, moving in x.
    // Reset x.
    for (int i = 0; i < L; ++i) {
        x[i] = 0;
    }
    cr2.move_in(x);
    update_reducer_test(cr2, L, num_reps);
    cr2.move_out(x);

    // Verify result.  x should be the same, and y should be
    // unchanged.
    verify_expected(x, expected, L);
    verify_expected(y, expected, L);
    CILKTEST_REMARK(2, "Move creducer, test 2 PASSED\n");
    
    // TEST 3: Try again, moving in y. 
    // Try on cr1, moving in y.
    // Reset y. 
    for (int i = 0; i < L; ++i) {
        y[i] = 0;
    }
    cr1.move_in(y);
    // cr1 should no longer be pointing to x.  So x should remain
    // valid.
    verify_expected(x, expected, L);

    update_reducer_test(cr1, L, num_reps);
    cr1.move_out(y);

    // Verify result.  y should be updated now, and x should remain
    // unchanged.
    verify_expected(x, expected, L);
    verify_expected(y, expected, L);

    // TEST 3: Now move-in from x, and store the output into y.
    const int RAND_VAL = 12825;
    for (int i = 0; i < L; ++i) {
        x[i] = 0;
        y[i] = RAND_VAL + i; // GARBAGE into y.
    }

    // Test to make sure that no extra values are left in cr2.
    cr2.move_in(y);
    cr2.move_out(y);
    for (int i = 0; i < L; ++i) {
        TEST_ASSERT_EQ(y[i], RAND_VAL + i);
    }

    // Move into x, move out into y.
    cr1.move_in(x);
    update_reducer_test(cr1, L, num_reps);
    cr1.move_out(y);

    verify_expected(y, expected, L);
    for (int i = 0; i < L; ++i) {
        TEST_ASSERT_EQ(x[i], 0);
    }

    /// Now, x should still be there. Update, move out, and check that
    /// we have the expected result.
    update_reducer_test(cr1, L, num_reps);
    cr1.move_out(x);
    verify_expected(x, expected, L);
    CILKTEST_REMARK(2, "Move creducer, test 3 PASSED\n");
    
    delete[] x;
    delete[] y;
    delete[] expected;
}


int main(int argc, char* argv[])
{

    CILKTEST_PARSE_TEST_ARGS(argc, argv);
    CILKTEST_BEGIN("cred_opadd_array");
    bool time_test_only = true;

    test_move_creducer();

    test_reducer_access_pattern(random_dense, "random_dense", time_test_only);
    test_reducer_access_pattern(quarter_dense, "quarter_dense", time_test_only);
    test_reducer_access_pattern(in_order, "in_order", time_test_only);
    test_reducer_access_pattern(in_order_contention, "in_order_contention", time_test_only);


    return CILKTEST_END("cred_opadd_array");
}
