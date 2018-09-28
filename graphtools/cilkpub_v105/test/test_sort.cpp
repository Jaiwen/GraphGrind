/* test_sort.cpp              -*-C++-*-
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

#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS

#include <cilkpub/sort.h> /* Put header to be tested first to improve odds of detecting missing header. */

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cmath>
#include <string> 

#include <cilk/cilk.h>
#include <cilk/reducer_opadd.h>
#include <cilk/cilk_api.h>

#include "cilktest_harness.h"
#include "cilktest_timing.h"


unsigned Random() {
    return rand()*(RAND_MAX+1u)+rand();
}

enum TEST_MODE {
    CHECK_STABILITY = 1,
    EXPONENTIAL_DISTRIBUTION = 2,
    UNIFORM_DISTRIBUTION = 3,
    STRING_KEY = 4
};

template <int MODE>
struct ModeTypes {
    static const char* SequenceName;        // Full, lengthy name.
    static const char* SequenceNameShort;   // Shorter name, read in by performance testing scripts.
};

template <>
const char* ModeTypes<CHECK_STABILITY>::SequenceName = "stability_check_uniform distribution of (char,int)";
template <>
const char* ModeTypes<CHECK_STABILITY>::SequenceNameShort = "stability_check";

template <>
const char* ModeTypes<EXPONENTIAL_DISTRIBUTION>::SequenceName = "exponential distribution of double";
template <>
const char* ModeTypes<EXPONENTIAL_DISTRIBUTION>::SequenceNameShort = "exponential_double";

template <>
const char* ModeTypes<UNIFORM_DISTRIBUTION>::SequenceName = "uniform distribution of int";
template <>
const char* ModeTypes<UNIFORM_DISTRIBUTION>::SequenceNameShort = "uniform_int";

template <>
const char* ModeTypes<STRING_KEY>::SequenceName = "strings";
template <>
const char* ModeTypes<STRING_KEY>::SequenceNameShort = "strings";


// Test that checks stability of sort and checks for construct/assign/destruct errors
cilk::reducer_opadd<int> KeyCount;

/// Data type used to check stability of the sort.
class StableCheckT {
    char value;
    size_t index;
    StableCheckT* self;
    StableCheckT( int value_, size_t index_ )
       : value(value_&0xF), index(index_), self(this) {++KeyCount;}
public:
    friend bool operator<( const StableCheckT& x,
                           const StableCheckT& y ) {
        return x.value<y.value;
    }

    // Function to call to fill in index with a "random" value.
    static StableCheckT get_random_t( size_t idx ) {
        return StableCheckT(rand(), idx);
    }
    
    friend int IndexOf( const StableCheckT& x ) {
        assert(x.self==&x);
        return x.index;
    }
    StableCheckT() : self(this) {
        ++KeyCount;
    }
    StableCheckT( const StableCheckT& x ) : value(x.value), index(x.index), self(this) {
        assert(x.self==&x);
        ++KeyCount;
    }
    void operator=( const StableCheckT& x ) {
        assert( self==this );
        assert( x.self==&x );
        value = x.value;
        index = x.index;
    }
    ~StableCheckT() {
        assert(self==this);
        --KeyCount;
        self = NULL;
    }
};

// Used to flip between two signatures for sort
static unsigned Chooser;

// Class that defines the various sorts we want to test on a
// particular data-type T.
template <typename T>
class Sorter {
public:
    // Class that defines random-access iterator
    class iter: public std::iterator<std::random_access_iterator_tag,T> {
        T* m_ptr;
    public:
        explicit iter(T* ptr) : m_ptr(ptr) {}
        friend ptrdiff_t operator-( iter i, iter j ) {return i.m_ptr-j.m_ptr;}
        friend iter operator-( iter i, ptrdiff_t j ) {return iter(i.m_ptr-j);}
        friend iter operator+( iter i, ptrdiff_t j ) {return iter(i.m_ptr+j);}
        friend bool operator<( iter i, iter j ) {return i.m_ptr<j.m_ptr;}
        friend bool operator==( iter i, iter j ) {return i.m_ptr==j.m_ptr;}
        friend bool operator!=( iter i, iter j ) {return i.m_ptr!=j.m_ptr;}
        T& operator*() const {return *m_ptr;}
        T& operator[]( ptrdiff_t j ) const {return m_ptr[j];}
        iter operator--() {--m_ptr; return *this;}
        iter operator++() {++m_ptr; return *this;}
        iter operator--(int) {return iter(m_ptr--);}
        iter operator++(int) {return iter(m_ptr++);}
        iter operator+=( ptrdiff_t j ) {m_ptr+=j; return *this;}
    };

    static void stl_sort( T* first, T* last ) {
        std::sort(first,last);
    }

    static void cilk_quicksort ( T* first, T* last ) {
        if( Chooser++&1 )
            cilkpub::cilk_sort_in_place(iter(first),iter(last));
        else
            cilkpub::cilk_sort_in_place(first,last,std::less<T>());
    }

    static void cilk_samplesort ( T* first, T* last ) {
        if( Chooser++&1 )
            cilkpub::cilk_sort(iter(first),iter(last));
        else
            cilkpub::cilk_sort(first,last,std::less<T>());
    }
};

template <typename T, int MODE>
struct TestData {
    size_t M;
    size_t N;
    T *Unsorted;
    T *Expected;
    T *Actual;

    // Construct the test data.
    TestData(size_t M_, size_t N_);

    // Destroy the data. 
    ~TestData();

    // Generate a "random" element for a specified index.
    static T MakeRandomT(size_t idx);
};


template <typename T, int MODE>
TestData<T, MODE>::TestData(size_t M_, size_t N_)
    : M(M_)
    , N(N_)
{
    Unsorted = new T[M*N];
    Expected = new T[M*N];
    Actual = new T[M*N];
    for( size_t i=0; i<M; ++i ) {
        for( size_t j=0; j<N; ++j ) {
            Unsorted[i*N+j] = MakeRandomT(j);
        }
        std::copy( Unsorted+i*N, Unsorted+(i+1)*N, Expected+i*N );
        if( cilkpub::cilk_is_sorted(Expected+i*N, Expected+(i+1)*N) ) {
            REPORT("Error for %s: input sequence already sorted\n",ModeTypes<MODE>::SequenceName);
        }
        std::stable_sort( Expected+i*N, Expected+(i+1)*N );
        if( !cilkpub::cilk_is_sorted(Expected+i*N, Expected+(i+1)*N) ) {
            REPORT("Error for %s: cilk_is_sorted returned false when it should be true\n",ModeTypes<MODE>::SequenceName);
        }
    }
}

template <typename T, int MODE>
TestData<T, MODE>::~TestData() {
    delete[] Unsorted;
    delete[] Expected;
    delete[] Actual;
}

template <>
StableCheckT
TestData<StableCheckT, CHECK_STABILITY>::MakeRandomT( size_t idx )
{
    return StableCheckT::get_random_t(idx);
}

template <>
double
TestData<double, EXPONENTIAL_DISTRIBUTION>::MakeRandomT( size_t ) {
    return std::log(double(Random()+1));
}

template <>
int
TestData<int, UNIFORM_DISTRIBUTION>::MakeRandomT( size_t ) {
    return Random();
}

typedef std::string StringT;
template <>
StringT
TestData<StringT, STRING_KEY>::MakeRandomT( size_t ) {
    char buffer[20];
    sprintf(buffer,"%d",int(Random()));
    return buffer;
}



template <typename S>
void TestSort( TestData<StableCheckT, CHECK_STABILITY>& data,
               S sortToBeTested,
               const char* what,
               bool shouldBeStable=false )
{
    unsigned long long t0, t1;
    std::copy( data.Unsorted,
               data.Unsorted+ data.M * data.N,
               data.Actual );
    // Warm up run-time
    sortToBeTested(data.Actual,
                   data.Actual + data.N);
    t0 = CILKTEST_GETTICKS();
    for( size_t i=1; i < data.M; ++i ) {
        KeyCount.set_value(0);
        sortToBeTested(data.Actual+i*data.N,
                       data.Actual+(i+1)*data.N);
        TEST_ASSERT_EQ(KeyCount.get_value(), 0);
    }

    t1 = CILKTEST_GETTICKS();

    if (!CILKTEST_PERF_RUN()) {
        // Check for correctness if we aren't measuring performance.
        for( size_t k=0; k< data.M * data.N; ++k ) {
            if((data.Actual[k] < data.Expected[k]) ||
               (data.Expected[k] < data.Actual[k])) {
                REPORT("Error for %s\n", what);
                TEST_ASSERT(0);
                return;
            }

            if( shouldBeStable ) {
                if( IndexOf(data.Actual[k])!=IndexOf(data.Expected[k]) ) {
                    REPORT("Stability error for %s\n", what);
                    TEST_ASSERT(0);
                    return;
                }
            }
        }
    }

    CILKTEST_REMARK(2,
                    "%30s\t%5.2f\n",
                    what,
                    (t1-t0) * 1.0e-3);

    // If we are executing a performance run, report the time.
    if (CILKTEST_PERF_RUN()) {
        CILKPUB_PERF_REPORT_TIME(stdout,
                                 what, 
                                 __cilkrts_get_nworkers(),
                                 t1 - t0,
                                 ModeTypes<CHECK_STABILITY>::SequenceNameShort,
                                 NULL);
    }
}


template<typename S, typename T, int MODE> 
void TestSort( TestData<T, MODE>& data,
               S sortToBeTested,
               const char* what)
{
    unsigned long long t0, t1;
    std::copy( data.Unsorted,
               data.Unsorted+ data.M * data.N,
               data.Actual );
    // Warm up run-time
    sortToBeTested(data.Actual,
                   data.Actual + data.N);


    t0 = CILKTEST_GETTICKS();
    for( size_t i=1; i < data.M; ++i ) {
        sortToBeTested(data.Actual+i*data.N,
                       data.Actual+(i+1)*data.N);
    }
    t1 = CILKTEST_GETTICKS();


    // Check correctness if we aren't running a performance test.
    if (!CILKTEST_PERF_RUN()) {
        for( size_t k=0; k< data.M * data.N; ++k ) {
            if((data.Actual[k] < data.Expected[k]) ||
               (data.Expected[k] < data.Actual[k])) {
                REPORT("Error for %s\n",what);
                TEST_ASSERT(0);
                return;
            }
        }
    }

    CILKTEST_REMARK(2,
                    "%30s\t%5.2f\n",
                    what,
                    (t1 - t0) * 1.0e-3);

    // If we are executing a performance run, report the time.
    if (CILKTEST_PERF_RUN()) {
        CILKPUB_PERF_REPORT_TIME(stdout,
                                 what,
                                 __cilkrts_get_nworkers(),
                                 t1 - t0,
                                 ModeTypes<MODE>::SequenceNameShort,
                                 NULL);
    }
}



template <typename T, int MODE>
void run_test_sorts(size_t M, size_t N)
{
    TestData<T, MODE> data(M, N);

    CILKTEST_REMARK(2,
                    "Testing for %d sorts of length %d for %s\n",
                    int(data.M-1),
                    int(data.N),
                    ModeTypes<MODE>::SequenceName);


    // Test the serial sort if we aren't measuring performance, or if
    // P = 1.  Otherwise, the serial sort takes too long.
    if (!CILKTEST_PERF_RUN() ||
        (__cilkrts_get_nworkers() == 1)) {
        // Test serial sort
        TestSort(data, Sorter<T>::stl_sort,"STL sort");
    }
    
    // Test Cilk Plus sorts.
    TestSort(data, Sorter<T>::cilk_quicksort, "Cilk Plus quicksort");
    TestSort(data, Sorter<T>::cilk_samplesort, "Cilk Plus samplesort");
}



int main( int argc, char* argv[] ) {
    CILKTEST_PARSE_TEST_ARGS(argc, argv);
    CILKTEST_BEGIN("sort");

#if 0
    // Small defaults for debugging
    size_t M = 5;
    size_t N = 50;
#else
    // Big defaults for timing
    size_t M = 10;
    size_t N = 1000000;
#endif

    ++M;    // Add one for the warmup sort
    srand(2);

    run_test_sorts<int, UNIFORM_DISTRIBUTION>(M, N);
    run_test_sorts<double, EXPONENTIAL_DISTRIBUTION>(M, N);
    run_test_sorts<StringT, STRING_KEY>(M, N);
    run_test_sorts<StableCheckT, CHECK_STABILITY>(M, N);
    

    return CILKTEST_END("sort");
}
