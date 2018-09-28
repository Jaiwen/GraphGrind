/* sample_fib_pedigree.cpp                  -*-C++-*-
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

/**
 * @file sample_fib_pedigree.cpp
 *
 * @brief Sample program printing out pedigrees for fib(4).
 */

#include <cstdio>
#include <cstdlib>
#include <cilk/cilk_api.h>

// This include should only be necessary if you are using ICC v12.1,
// because it does not provide the improved pedigree interface.
#include <cilkpub/pedigrees.h>

// NOTE: This method does not do anything special to try to print a
// pedigree atomically.  Output from different workers may be
// interleaved if the program is run in parallel.
inline void get_and_print_current_pedigree(const char* desc,
                                           int advance_pedigree)
{
    // Get the current pedigree.  This function returns a copy of the
    // least-significant node in list representing the current
    // pedigree.
    __cilkrts_pedigree ls_node = __cilkrts_get_pedigree();

    // Traverse the linked list for the current pedigree to figure out
    // its length.  The length should never be 0.
    int len = 0;
    const __cilkrts_pedigree *ped = &ls_node;
    while (NULL != ped) {
        ped = ped->parent;
        len++;
    }
    assert(len > 0);

    // Now allocate an array, 
    uint64_t* pedigree = new uint64_t[len];
    
    // Reset, and walk the list again.  Note that our original pointer
    // is still valid.
    
    // Walk the pedigree list again and store the pedigree into an
    // array in order of most-significant node to least-significant
    // node.  Note that our original pointer is still valid because
    // the current "Cilk block" has not ended yet.
    ped = &ls_node;
    int i = 0;
    while (i < len) {
        assert(ped != NULL);
        // Store the rank into the pedigree.
        pedigree[len - 1 - i] = ped->rank;
        ped = ped->parent;
        i++;
    }

    // Now print out the pedigree we just read in.
    std::printf("%15s: [ ", desc);
    for (int i = 0; i < len; ++i) {
	std::printf("%llu ", pedigree[i]);
    }
    std::printf("]\n");

    if (advance_pedigree) {
        // Increment the current rank.
        __cilkrts_bump_worker_rank();
    }

    delete[] pedigree;
}


// Global variable to indicate whether we want to end the current
// strand every time we read in a pedigree for fib.
int end_strand;

int fib(int n) {
    if (n < 2) {
        get_and_print_current_pedigree("Base case", end_strand);
        return n;
    }
    else {
        int x, y;
        get_and_print_current_pedigree("Before spawn", end_strand);
        x = _Cilk_spawn fib(n-1);
        get_and_print_current_pedigree("In continuation", end_strand);
        y = fib(n-2);
        _Cilk_sync;
        get_and_print_current_pedigree("After sync", end_strand);
        return x+y;
    }
}



int main(int argc, char* argv[]) {
    // Force execution on 1 worker.
    __cilkrts_set_param("nworkers", "1");

    int n = 4;
    if (argc > 1) {
        // If we have a second argument, end the current strand every
        // time we check and read the pedigree.
        end_strand = 1;
    }

    int ans = fib(n);
    std::printf("Fib(%d) = %d. End strands = %d\n", n, ans, end_strand);
    return 0;
}
