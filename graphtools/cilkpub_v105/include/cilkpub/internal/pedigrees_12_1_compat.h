/* pedigrees_12_1_compat.h                  -*-C-*-
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
 * @file pedigrees_12_1_compat.h
 *
 * @brief Extra definitions to try to make pedigree module compatible
 *  with the Cilk Plus API that ships with Intel v12.1 compilers.
 *
 * This file is an undocumented hack that I got to work, but there are
 * no guaranteees...
 *
 * If you really want to use pedigrees reliably, upgrade to a 13.0
 * version of the Cilk Plus runtime. :)
 *
 * @author Jim Sukha
 * @version 1.03
 */

#ifndef __CILKPUB_PEDIGREES_12_1_COMPAT_H_
#define __CILKPUB_PEDIGREES_12_1_COMPAT_H_

#include <cilk/cilk_api.h>

// For icc 12.1 compilers, we don't have this struct defined.
typedef struct __cilkrts_pedigree
{
    uint64_t rank;
    const struct __cilkrts_pedigree *parent;
} __cilkrts_pedigree;


/**
 * @brief A hack to use the 13.0 pedigree interface with the 12.1 compiler.
 *
 * The two interfaces are mostly the same, except where they are
 * different. :)
 *
 * Pay no attention to the magic being done inside this method.
 *
 * @warning Undocumented hack.  Don't use this method unless you
 * really have to.
 */
inline __cilkrts_pedigree __cilkrts_get_pedigree(void) {
    // Build the initial node in the linked list.
    __cilkrts_pedigree ans;
    int error_code;

    // The rank of the initial node is stored in the worker.
    error_code = __cilkrts_get_worker_rank(&ans.rank);
    if (error_code != 0) {
        fprintf(stderr, "CILKPUB ERROR: Trying to read in pedigree outside of Cilk region.\n");
        fprintf(stderr, "CILKPUB_ERROR: One way to fix this error is to put a _Cilk_spawn at the top of main.\n");
        assert(0);
    }

    // The pointer information is buried somewhere in the context.
    __cilkrts_pedigree_context_t context;
    uint64_t tmp;
    memset(&context, 0, sizeof(context));
    context.size = sizeof(context);

    error_code = __cilkrts_get_pedigree_info(&context,
                                             &tmp);
    assert(error_code >= 0);
    ans.parent = (__cilkrts_pedigree*)context.data[0];
    if (error_code >= 1) {
        ans.parent = NULL;
    }
    return ans;
}

#endif // defined(__CILKPUB_PEDIGREES_12_1_COMPAT_H_)
