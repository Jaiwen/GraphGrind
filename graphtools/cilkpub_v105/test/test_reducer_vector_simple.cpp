/*  test_reducer_vector_simple.cpp                  -*- C++ -*-
 *
 *  Copyright (C) 2013 Intel Corporation.  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *    * Neither the name of Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *  
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 *  AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 *  WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/** @file test_reducer_vector_simple.cpp
 *  @brief Test program for vector append reducers.
 */

#include <cilk/cilk.h>
#include <cilkpub/reducer_vector.h>

#include "cilktest_harness.h"

#include <vector>
#include <cstdlib>


inline void tfprint(FILE* f, const std::vector<int>& v) 
{
    typedef std::vector<int> vector_type;
    if (v.empty()) {
        fprintf(f, "[ ]");
    }
    else {
        const char* prefix = "[";
        for (vector_type::const_iterator i = v.begin(); i != v.end(); ++i) {
            fprintf(f, "%s", prefix);
            tfprint(f, *i);
            prefix = ", ";
        }
        fprintf(f, "]"); 
    }
}


// Common test data

const int NDATA = 1000;

std::vector<int> a;

void initialize_data()
{
    std::srand(0);
    std::generate_n(std::back_inserter(a), NDATA, std::rand); 
}


typedef cilk::reducer< cilkpub::op_vector<int> > R;
typedef std::vector<int> V;

void test_constructors(std::vector<V> &vv)
{
    // Constructors, get_value.
    R r1;
    TEST_ASSERT(r1.get_value().empty());

    R r2(6, 101);
    V v2(6, 101);
    TEST_ASSERT_EQ(r2.get_value(), v2);

    for (int i = 0; i != NDATA; ++i) {
        R r3(a.begin(), a.begin() + i);
        TEST_ASSERT_EQ(r3.get_value(), vv[i]);

        R r4(vv[i]);
        TEST_ASSERT_EQ(r4.get_value(), vv[i]);

        V v5(vv[i]);
        R r5(cilk::move_in(v5));
        TEST_ASSERT_EQ(r5.get_value(), vv[i]);
    }
}

void test_set_get_move(std::vector<V> &vv)
{
    // Set_value, get_value, move_in, move_out.
    for (int i = 0; i != NDATA; ++i) {
        R r1;
        r1.set_value(vv[i]);
        TEST_ASSERT_EQ(r1.get_value(), vv[i]);
        
        R r2;
        V v2(vv[i]);
        r2.move_in(v2);
        TEST_ASSERT_EQ(r2.get_value(), vv[i]);
        
        R r3(vv[i]);
        V v3;
        r3.move_out(v3);
        TEST_ASSERT_EQ(v3, vv[i]);
    }
}

void test_append(std::vector<V> &vv)
{
    // Append
    {
    R r1;
    for (int i = 0; i != NDATA; ++i) {
        r1->push_back(a[i]);
        TEST_ASSERT_EQ(r1.get_value(), vv[i+1]);
    }
    
    R r2;
    for (int i = 0; i != NDATA; ++i) {
        r2->insert_back(1, a[i]);
        TEST_ASSERT_EQ(r2.get_value(), vv[i+1]);
    }
    
    R r3;
    for (int i = 0; i != NDATA; ++i) {
        r3->insert_back(1, a[i]);
        TEST_ASSERT_EQ(r3.get_value(), vv[i+1]);
    }
    
    R r4;
    V v4;
    V::const_iterator i4 = a.begin();
    for (int i = 0; i != NDATA; ++i) {
        r4->insert_back(i4, i4+3);
        v4.insert(v4.end(), i4, i4+3);
        i4 += 3;
    }
    TEST_ASSERT_EQ(r4.get_value(), v4);
    
    }
}

void run_tests()
{
    std::vector<V> vv;
    for (int i = 0; i != NDATA + 1; ++i) {
        vv.push_back(V(a.begin(), a.begin() + i));
    }

    CILKTEST_REMARK(2, "Testing constructors...");
    test_constructors(vv);
    CILKTEST_REMARK(2, "done\n");

    CILKTEST_REMARK(2, "Testing set get move...");
    test_set_get_move(vv);
    CILKTEST_REMARK(2, "done\n");
    
    CILKTEST_REMARK(2, "Testing append...");
    test_append(vv);
    CILKTEST_REMARK(2, "done\n");
}


int main()
{
    const char* test_name = "Vector Reducer Tests";
    CILKTEST_BEGIN(test_name);
    initialize_data();
    run_tests();
    return CILKTEST_END(test_name);
}
