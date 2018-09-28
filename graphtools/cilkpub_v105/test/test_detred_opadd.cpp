/* test_detred_opadd.cpp               -*-C++-*-
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

// TBD: Hack for now, until we fix this for real.
#define CILK_IGNORE_REDUCER_ALIGNMENT

#include <cilk/reducer_opadd.h>
#include <cilkpub/detred_opadd.h>

#include "cilktest_harness.h"


using namespace cilkpub;

template <typename T>
inline void tfprint(FILE* f, DetRedIview<T>& iview)
{
    iview.fprint(f);
}

inline void tfprint(FILE* f, const pedigree& ped)
{
    ped.fprint(f, "");
}
// void tfprint(FILE* f, det_reducer_opadd<double>* detred)
// {
// //    detred->fprint(f);
// }

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

#define COMPLEX_ADD

void test_det_reducer_recursive_helper(cilk::reducer_opadd<double>* normal_red,
                                       det_reducer_opadd<double>* det_red,
                                       int start, int end) {
    if ((end - start) <= 100) {
        for (int j = start; j < end; ++j) {

#ifdef COMPLEX_ADD            
            double tmp = 2.54312  / (1.0 * (j+2));
#else
            double tmp = 1.0;
#endif
            (*normal_red) += tmp;
            (*det_red) += tmp;
        }
    }
    else {
        int mid = (start + end)/2;
        _Cilk_spawn test_det_reducer_recursive_helper(normal_red, det_red, start, mid);

        if (mid % 10 == 3) {
#ifdef COMPLEX_ADD
            double tmp = 1.32;
#else
            double tmp = 1.0;
#endif            
            (*normal_red) += tmp;
            (*det_red) += tmp;
        }
        test_det_reducer_recursive_helper(normal_red, det_red, mid, end);
        _Cilk_sync;
    }
}


void test_det_reducer_helper(double* normal_ans,
                             double* det_ans,
                             int NUM_TRIALS,
                             bool scope_test) 
{
    double starting_val = 1.321;
    int LOOP_WIDTH = 10000;
    for (int i = 0; i < NUM_TRIALS; ++i) {
        cilk::reducer_opadd<double> normal_red(starting_val);
        pedigree_scope current_scope;
        det_reducer_opadd<double>* det_red;

        if (scope_test) {
            current_scope = pedigree_scope::current();
            det_red = new det_reducer_opadd<double>(starting_val, current_scope);
        }
        else {
            det_red = new det_reducer_opadd<double>(starting_val);
        }
        CILKTEST_PRINT(3, "Detred Initial View: ", det_red, "\n");
        
        _Cilk_for(int j = 0; j < LOOP_WIDTH; ++j) {
            double tmp = 2.45312 * j;
            normal_red += tmp;
            *det_red += tmp;
        }

        CILKTEST_PRINT(4, "Detred Initial View: ", det_red, "\n");
        
        // NOTE: In order to get the exact same results between two
        // different calls, I need to spawn this function.  Otherwise,
        // the initial pedigree of the first update to the reducer is
        // shifted by the initial rank of function.
        //
        // (Really, this is a hack.  What we need here is the same
        //  notion of a scoped pedigree that we used for DPRNG.)
        test_det_reducer_recursive_helper(&normal_red, det_red, 0, LOOP_WIDTH);
        
        normal_ans[i] = normal_red.get_value();
        det_ans[i] = det_red->get_value();

        CILKTEST_REMARK(3, "Iteration %d: \n", i);
        CILKTEST_PRINT(3, "Detred View: ", det_red, "\n");
        delete det_red;
    }
}


void test_det_reducer() {

    CILKTEST_REMARK(2, "test_det_reducer()...");
    
    const int NUM_TRIALS = 15;
    double normal_ans[NUM_TRIALS];
    double det_ans[NUM_TRIALS];

    _Cilk_spawn test_det_reducer_helper(normal_ans, det_ans, NUM_TRIALS, false);
    _Cilk_sync;
    
    test_det_reducer_helper(normal_ans, det_ans, NUM_TRIALS, false);
    test_det_reducer_helper(normal_ans, det_ans, NUM_TRIALS, true);

    CILKTEST_REMARK(2, "\nReducer: normal ans = %g, det ans = %g\n",
                    normal_ans[0], det_ans[0]);
    for (int i = 1; i < NUM_TRIALS; ++i) {
        CILKTEST_REMARK(2, "....Reducer diff: normal = %11g, det = %g\n",
                        (normal_ans[i] - normal_ans[0]),
                        (det_ans[i] - det_ans[0]));
        TEST_ASSERT(within_tol(det_ans[i], normal_ans[i]));
        TEST_ASSERT(det_ans[i] == det_ans[0]);
    }

    pass_test();
}



int main(int argc, char* argv[]) {
    CILKTEST_PARSE_TEST_ARGS(argc, argv);
    CILKTEST_BEGIN("Determinsitic reducer test");

    test_det_reducer();

    return CILKTEST_END("Determinsitic reducer test");
}
