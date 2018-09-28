/* csample_scan.c                  -*-C-*-
 *
 *************************************************************************
 *
 * Copyright (C) 2014 Intel Corporation
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
 * @file csample_scan.c
 *
 * @brief Sample program illustrating use of scan (with C interface)
 */

#include <cilkpub/scan_c.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>


// Idempotent function that does dummy work to has a value x.
int dummy_work(int x) {
    int j;
    for (j = 0; j < 1000; ++j) {
        x = x*(2*x-1);
    }
    return x;
}


// Global variables for the test.
#define N 400000
int a[N];
int b[N];
int b_expected[N];

int reduce_int(size_t s, size_t m)
{
    int sum = 0;
    size_t i;
    for (i = s; i < s+m; ++i) {
        // Compute work value, store into slot into b.
        b[i]= dummy_work(a[i]);
        sum += b[i];
    }
    return sum;
}

int combine_int(int left, int right)
{
    return left + right;
}

void sum_scan_int(size_t s, size_t m, int initial_val) {
    size_t i;
    for (i = s; i < s+m; ++i) {
        initial_val += b[i];
        b[i] = initial_val;
    }
}

// Declare the parallel scan function on int's.
CILKPUB_C_PARALLEL_SCAN_DECL(MY_INT_SCAN, int);


// The same code as above, except using char's.
char a2[N];
char b2[N];
char b2_expected[N];

char reduce_char(size_t s, size_t m)
{
    char sum = 0;
    size_t i;
    for (i = s; i < s+m; ++i) {
        // Compute work value, store into slot into b.
        b2[i]= (char)dummy_work(a2[i]);
        sum += b2[i];
    }
    return sum;
}

char combine_char(char left, char right)
{
    return left + right;
}

void sum_scan_char(size_t s, size_t m, char initial_val) {
    size_t i;
    for (i = s; i < s+m; ++i) {
        initial_val += b2[i];
        b2[i] = initial_val;
    }
}
// Declare the parallel scan on size_t's.
CILKPUB_C_PARALLEL_SCAN_DECL(MY_CHAR_SCAN, char);


int main(int argc, char* argv[]) {

    int initial_val = 43;
    srand(2);
    int sum = initial_val;
    int i;

    char initial_val2 = 43;
    char sum2 = initial_val2;

    // For all i, b_expected[i] should store
    // cumulative sum of
    //  \sum_{j=0}^{i} dummy_work(a[j]).
    for (i = 0; i < N; ++i) {
        a[i] = rand();
        sum += dummy_work(a[i]);
        b_expected[i] = sum;

        a2[i] = a[i];
        sum2 += dummy_work(a2[i]);
        b2_expected[i] = sum2;
    }

    printf("Sample scan with n = %d... ", N);

    int tilesize = 50;

    // Scan for int. 
    CILKPUB_C_PARALLEL_SCAN(MY_INT_SCAN,
                            N,
                            initial_val,
                            tilesize,
                            reduce_int,
                            combine_int,
                            sum_scan_int);
    // Scan over char
    CILKPUB_C_PARALLEL_SCAN(MY_CHAR_SCAN,
                            N,
                            initial_val2,
                            tilesize,
                            reduce_char,
                            combine_char,
                            sum_scan_char);
    
    // Check output. 
    for (i = 0; i < N; ++i) {
        assert(b[i] == b_expected[i]);
        assert(b2[i] == b2_expected[i]);
    }

    printf("\n");
    printf("Int final value: b[%d] = %d\n",
           N-1, b[N-1]);
    printf("Char final value: b[%d] = %c\n",
           N-1, b2[N-1]);
           
    printf("PASSED\n");

    return 0;
}

