#include <assert.h>
#include <cstdio>
#include <cstdlib>

#include <cilk/reducer_opadd.h>
#include <cilkpub/internal/detred_iview.h>

#include "test_detred.h"
#include "cilktest_harness.h"


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
inline void check_current_pedigree_view(int verbosity_level) 
{
    pedigree cped_in_loop = pedigree::current();
    CILKTEST_PRINT(verbosity_level, "Current pedigree: ", cped_in_loop, "\n");
    
    DetRedIview<T> iv_in_loop = DetRedIview<T>(132, pedigree::current());
    CILKTEST_PRINT(verbosity_level, "Iview: ", iv_in_loop, "\n");
    pedigree start_ped = iv_in_loop.get_start_pedigree();
    pedigree active_ped = iv_in_loop.get_active_pedigree();

    if (cped_in_loop != start_ped) {
        CILKTEST_PRINT(0, "ERROR: cped_in_loop is ", cped_in_loop, "\n");
        CILKTEST_PRINT(0, "ERROR: start_ped is ", start_ped, "\n");
    }
    TEST_ASSERT_EQ(cped_in_loop, start_ped);
    TEST_ASSERT_EQ(cped_in_loop, active_ped);
    TEST_ASSERT_EQ(132, iv_in_loop.get_active_elem_val());

    iv_in_loop.validate();
}

template <typename T>
inline int tfib_check_pedigrees(int n, int verbosity_level)
{
    if (n < 3) {
        check_current_pedigree_view<T>(verbosity_level);
        return n;
    }
    else {
        int x, y, z;
        check_current_pedigree_view<T>(verbosity_level);
        x = _Cilk_spawn tfib_check_pedigrees<T>(n-1, verbosity_level);
        check_current_pedigree_view<T>(verbosity_level);
        y = _Cilk_spawn tfib_check_pedigrees<T>(n-2, verbosity_level);
        check_current_pedigree_view<T>(verbosity_level);
        z = tfib_check_pedigrees<T>(n-3, verbosity_level);
        check_current_pedigree_view<T>(verbosity_level);
        _Cilk_sync;
        check_current_pedigree_view<T>(verbosity_level);
        return (x+y+z);
    }
}

template <typename T>
void test_detred_iview_constructors() 
{
    DetRedIview<T> empty = DetRedIview<T>();
    tfprint(stdout, empty);

    CILKTEST_REMARK(2, "Constructing initial range with current pedigree\n");

    pedigree cped = pedigree::current();
    CILKTEST_PRINT(2, "Current pedigree: ", cped, "\n");

    DetRedIview<T> current = DetRedIview<T>(1);
    tfprint(stdout, current);

    #pragma cilk grainsize=1
    _Cilk_for (int i = 0; i < 100; ++i) {
        pedigree cped_in_loop = pedigree::current();
        check_current_pedigree_view<T>(3);
    }

    tfib_check_pedigrees<T>(11, 4);
}


template <typename T>
void test_rank_group_helper(int start_i,
                            int stop_i,
                            int delta,
                            int num_reps)
{
    const int K = 1;
    uint64_t test_ped[K+1];
    test_ped[0] = 0;
    test_ped[K] = start_i;
    int base_verbosity = 4;
    
    pedigree start_ped = pedigree(test_ped, K+1, false);
    T start_val = 42;
    
    DetRedIview<T> current = DetRedIview<T>(start_val, start_ped);
    T current_value = start_val;

    CILKTEST_REMARK(base_verbosity, "test_rank_group_helper: start_i = %d\n", start_i);
    TEST_ASSERT(current.active_group_start_rank() == start_i);
    TEST_ASSERT(current.active_group_active_rank() == start_i);
    current.validate();
    
    {
        T test_val = current.get_value();
        T test_val2 = current.get_value();
        TEST_ASSERT_EQ(test_val, test_val2);
        if (!within_tol(test_val, current_value)) {
            CILKTEST_PRINT(1, "Test val: ", test_val, "\n");
            CILKTEST_PRINT(1, "Expected val: ", current_value, "\n");
        }
        TEST_ASSERT(within_tol(test_val, current_value));
    }

    CILKTEST_REMARK(base_verbosity,
                    "test_rank_group_helper(%d, %d, %d, %d):\n",
                    start_i, stop_i, delta, num_reps);

    for (int i = start_i; i < stop_i; i += delta) {
        CILKTEST_REMARK(base_verbosity+1, "Rank range update: initial i = %d\n", i);
        CILKTEST_PRINT(base_verbosity+1, "Current range: ", current, "\n");
        
        for (int j = 0; j < num_reps; ++j) {
            CILKTEST_REMARK(base_verbosity+2, "Rank update: (%d, %d):\n", i, j);

            current.update_active_range_group((uint64_t)i, j+1);
            current_value += j+1;

            CILKTEST_PRINT(base_verbosity+1, "After update: ", current, "\n");
            CILKTEST_REMARK(base_verbosity+2, "**** End (%d, %d)\n", i, j);

            current.validate();
            TEST_ASSERT(current.active_group_start_rank() == start_i);

            const pedigree& cref = current.get_start_pedigree();
            const pedigree& tref = start_ped;
            if (cref != tref) {
                CILKTEST_PRINT(0, "View object: ", current, "\n");
                CILKTEST_PRINT(0, "ERROR: cref = ", cref, "\n");
                CILKTEST_PRINT(0, "ERROR: tref = ", tref, "\n");
                while (1);
            }
            TEST_ASSERT_EQ(cref, tref);

            T test_val = current.get_value();
            T test_val2 = current.get_value();
            TEST_ASSERT_EQ(test_val, test_val2);

            if (!within_tol(test_val, current_value)) {
                CILKTEST_PRINT(1, "Test val: ", test_val, "\n");
                CILKTEST_PRINT(1, "Expected val: ", current_value, "\n");
            }
            TEST_ASSERT(within_tol(test_val, current_value));
            TEST_ASSERT(within_tol(current.get_value(), current_value));
        }

        TEST_ASSERT(within_tol(current.get_value(), current_value));
    }

    CILKTEST_PRINT(base_verbosity+1, "Final range: ", current, "\n");
    CILKTEST_REMARK(base_verbosity+1,
                    "End test_rank_group(%d, %d, %d, %d)\n\n",
                    start_i, stop_i, delta, num_reps);
}

template <typename T>
void test_rank_group_fixed() 
{
    int start_i = 1;
    int last_i = 100;
    uint64_t test_ped[2] = {0, start_i};
    int base_verbosity = 4;
    int start_val = 41;

    pedigree start_ped = pedigree(test_ped, 2, false);
    DetRedIview<T> r1 = DetRedIview<T>(start_val, start_ped);

    r1.update_active_range_group(start_i, 1);
    CILKTEST_PRINT(base_verbosity, "r1 after start update: ", r1, "\n");
    r1.validate();
    
    r1.update_active_range_group(last_i, 1);
    CILKTEST_PRINT(base_verbosity, "r1 after last update: ", r1, "\n");
    r1.validate();

    TEST_ASSERT(r1.get_value() == (start_val + 2));
    TEST_ASSERT(r1.active_group_start_rank() == start_i);
    TEST_ASSERT(r1.active_group_active_rank() == last_i);

    assert(CILKTEST_NUM_ERRORS() == 0);
}


template <typename T>
void test_rank_group() {

    CILKTEST_REMARK(2, "test_rank_group()...");
    CILKTEST_REMARK(4, "***********************************\n");

    test_rank_group_fixed<T>();
    
    test_rank_group_helper<T>(0, 10, 3, 3);
    test_rank_group_helper<T>(1, 10, 1, 3);

    for (int start_i = 0; start_i < 17; ++start_i) {
        for (int step = 1; step < 15; ++step) {
            test_rank_group_helper<T>(start_i, 400, step, 3);
        }

        CILKTEST_REMARK(4, "Finished test_rank_group with start_i=%d\n", start_i);
        CILKTEST_REMARK(3, ".");
    }
    pass_test();
}



template <typename T>
void test_rank_terminate_helper(int start_i, int max_val, int base_verbosity)
{
    const int K = 1;
    uint64_t test_ped[K+1];
    test_ped[0] = 0;
    test_ped[K] = start_i;
    pedigree start_ped = pedigree(test_ped, K+1, false);
    T start_val = 0;
            
    DetRedIview<T> r1 = DetRedIview<T>(start_val, start_ped);
    DetRedIview<T> r2 = DetRedIview<T>(start_val, start_ped);
    T v1_before, v1_after;
    T v2_before, v2_after;

    TEST_ASSERT(start_i < max_val);

    CILKTEST_REMARK(base_verbosity+1, "test_rank_terminate_helper(start_i=%d, max_val=%d)\n",
                    start_i, max_val);

    CILKTEST_PRINT(base_verbosity+1,
                   "R1",r1, "\n");
    CILKTEST_PRINT(base_verbosity+1,
                   "R2",r2, "\n");

    // update r1 by incrementing each value.
    for (int q = start_i; q <= max_val; ++q) {
        r1.update_active_range_group(q, 1);
    }

    // update r2 by updating once at start, and once at end.
    r2.update_active_range_group(start_i, 1);

    CILKTEST_PRINT(base_verbosity+1,
                   "R2",r2, "\n");

    r2.update_active_range_group(max_val, 1);
    CILKTEST_PRINT(base_verbosity+1,
                   "R2",r2, "\n");


    CILKTEST_REMARK(base_verbosity+1, "terminate(start_i=%d, max_val=%d)\n",
                    start_i, max_val);
    CILKTEST_PRINT(base_verbosity+1,
                   "R1",r1, "\n");
    CILKTEST_PRINT(base_verbosity+1,
                   "R2",r2, "\n");
            
    // These two values should have the same tag structure.
    TEST_ASSERT(r1.tags_equal(r2));
    TEST_ASSERT(r2.tags_equal(r1));
    v1_before = r1.get_value();
    v2_before = r2.get_value();

    r1.terminate_active_range_group();
    r2.terminate_active_range_group();

    CILKTEST_REMARK(base_verbosity+1, "after terminate(start_i=%d, max_val=%d)\n",
                    start_i, max_val);
    CILKTEST_PRINT(base_verbosity+1,
                   "R1",r1, "\n");
    CILKTEST_PRINT(base_verbosity+1,
                   "R2",r2, "\n");
            
    TEST_ASSERT(r1.tags_equal(r2));
    TEST_ASSERT(r2.tags_equal(r1));
    v1_after = r1.get_value();
    v2_after = r2.get_value();

    TEST_ASSERT(v1_before == v1_after);
    TEST_ASSERT(v2_before == v2_after);
}

template <typename T>
void test_rank_terminate()
{
    CILKTEST_REMARK(2, "test_terminate()...");

    test_rank_terminate_helper<T>(1, 100, 3);
    
    int base_verbosity = 4;
    for (int max_val = 1000; max_val < 1027; ++max_val) {
        for (int start_i = 0; start_i < 10; ++start_i) {
            test_rank_terminate_helper<T>(start_i, max_val, base_verbosity);
        }
        CILKTEST_REMARK(base_verbosity, "Finished test_terminate(max_val=%d)\n",
               max_val);
        CILKTEST_REMARK(2, ".");
    }
    pass_test();
}





template <typename T>
void test_merge_helper(int start_val, int final_val) {
    int base_verbosity = 4;
    for (int split = start_val+1; split <= final_val; ++split) {
        CILKTEST_REMARK(base_verbosity, "TestMerge(%d, %d, %d)\n", start_val, split, final_val);
        double v1, v2, v3;
        uint64_t start_ped_array[2] = {0, start_val};
        uint64_t split_ped_array[2] = {0, split};
        uint64_t final_ped_array[2] = {0, final_val};
        pedigree start_ped = pedigree(start_ped_array, 2, false);
        pedigree split_ped = pedigree(split_ped_array, 2, false);
        pedigree final_ped = pedigree(final_ped_array, 2, false);

        T test_val = 0;
        DetRedIview<T> left = DetRedIview<T>(test_val, start_ped);
        DetRedIview<T> right = DetRedIview<T>(test_val, split_ped);
        left.update_active_range_group(split-1, 1);
        right.update_active_range_group(final_val, 2);
        v1 = left.get_value();
        v1 += right.get_value();

        DetRedIview<T> expected = DetRedIview<T>(test_val, start_ped);
        expected.update_active_range_group(split-1, 1);
        expected.update_active_range_group(split, test_val);
        expected.update_active_range_group(final_val, 2);
        v2 = expected.get_value();


        CILKTEST_PRINT(base_verbosity+2, "Left: \n", left, "\n");
        CILKTEST_PRINT(base_verbosity+2, "Right: \n", right, "\n");
        CILKTEST_PRINT(base_verbosity+2, "Expected ans: \n", expected, "\n");
        
        TEST_ASSERT_EQ(v1, v2);

        left.merge_active_range_groups(&right);

        CILKTEST_PRINT(base_verbosity+2, "Left after merge: \n", left, "\n");
        CILKTEST_PRINT(base_verbosity+2, "Right after mereg: \n", right, "\n");

        TEST_ASSERT(left.tags_equal(expected));
        TEST_ASSERT(expected.tags_equal(left));
        v3 = left.get_value();
        TEST_ASSERT(v1 == v3);

        // We can't make any assertions about the value left in
        // "right".  Depends on what the implementation wants to do...
        // TEST_ASSERT(right.get_value() == 0);
        CILKTEST_REMARK(base_verbosity, ".");
    }
}

template <typename T>
void test_merge()
{
    CILKTEST_REMARK(2, "test_merge()...");
    for (int start_val = 0; start_val < 13; ++start_val) {
        CILKTEST_REMARK(3, "TestMerge(%d)\n", start_val);
        for (int final_val = start_val+1; final_val < 200; ++final_val) {
            test_merge_helper<T>(start_val, final_val);
        }
        CILKTEST_REMARK(2, ".");
    }
    pass_test();
}



template <typename T>
void test_detred_iview_all()
{
    test_detred_iview_constructors<T>();
    test_rank_group<T>();
    test_rank_terminate<T>();
    test_merge<T>();
}

int main(int argc, char* argv[]) {
    CILKTEST_PARSE_TEST_ARGS(argc, argv);
    CILKTEST_BEGIN("Determinsitic reducer iviews (rank ranges)");
    
    test_detred_iview_all<double>();
    test_detred_iview_all<int>();
    test_detred_iview_all<float>();

    return CILKTEST_END("Determinsitic reducer iviews (rank ranges)");
}
