/* scan_c.h                  -*-C-*-
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
 * @file scan_c.h
 *
 * @brief C macros imitating the C++ scan in scan.h
 * @author Jim Sukha
 *
 * This code is copied directly from scan.h.
 *
 * It is preferable to use the C++ version, but this version may be
 * require less effort to use if you are working with pure C code.
 *
 */

#ifndef _CILKPUB_SCAN_C_H_
#define _CILKPUB_SCAN_C_H_

#include <assert.h>
#include <stdio.h>
#include <cilk/cilk.h>



inline size_t cilkpub_c_scan_int_log2_split( size_t m ) {
    size_t k=1;
    while( 2*k<m ) k*=2;
    return k;
}

#define CILKPUB_C_UPSWEEP_DECL(FNAME, T)                       \
    void CILKPUB_C_UPSWEEP_##FNAME(size_t i,                   \
                                   size_t m,                   \
                                   size_t tilesize,            \
                                   T* r,                       \
                                   size_t lastsize,            \
                                   T (*reduce)(size_t, size_t),\
                                   T (*combine)(T, T))         \
    {                                                          \
       if( m==1 ) {                                            \
           r[0] = reduce(i*tilesize, lastsize);                \
       } else {                                                \
           size_t k = cilkpub_c_scan_int_log2_split(m);        \
           cilk_spawn CILKPUB_C_UPSWEEP_##FNAME(i, k,          \
                                        tilesize,              \
                                        r,                     \
                                        tilesize,              \
                                        reduce,                \
                                        combine);              \
           CILKPUB_C_UPSWEEP_##FNAME(i+k, m-k,                 \
                                    tilesize,                  \
                                    r+k,                       \
                                    lastsize,                  \
                                    reduce,                    \
                                    combine);                  \
           cilk_sync;                                          \
           if( m==2*k )                                        \
               r[m-1] = combine(r[k-1], r[m-1]);               \
       }                                                       \
    }                                                          



#define CILKPUB_C_DOWNSWEEP_DECL(FNAME, T)                              \
    void CILKPUB_C_DOWNSWEEP_##FNAME( size_t i,                         \
                                      size_t m,                         \
                                      size_t tilesize,                  \
                                      T* r,                             \
                                      size_t lastsize,                  \
                                      T initial,                        \
                                      T (*combine)(T, T),               \
                                      void (*scan)(size_t, size_t, T) ) \
    {                                                                   \
        if( m==1 ) {                                                    \
            scan(i*tilesize, lastsize, initial);                        \
        } else {                                                        \
            size_t k = cilkpub_c_scan_int_log2_split(m);                \
            cilk_spawn CILKPUB_C_DOWNSWEEP_##FNAME(i, k,                \
                                                   tilesize, r,         \
                                                   tilesize, initial,   \
                                                   combine, scan);      \
            initial = combine(initial, r[k-1]);                         \
            CILKPUB_C_DOWNSWEEP_##FNAME(i+k, m-k,                       \
                                        tilesize, r+k,                  \
                                        lastsize, initial,              \
                                        combine, scan);                 \
        }                                                               \
    }


#define CILKPUB_C_PARALLEL_SCAN_BODY_DECL(FNAME, T)                           \
    void CILKPUB_C_PARALLEL_SCAN_##FNAME( size_t n,                           \
                                          T initial,                          \
                                          size_t tilesize,                    \
                                          T (*reduce)(size_t, size_t),        \
                                          T (*combine)(T, T),                 \
                                          void (*scan)(size_t, size_t, T)  )  \
    {                                                                         \
        if( n>0 ) {                                                           \
            size_t m = (n-1)/tilesize;                                        \
            T* r = malloc(sizeof(T) * (m+1));                                 \
                                                                              \
            CILKPUB_C_UPSWEEP_##FNAME(0, m+1, tilesize,                       \
                                    r,                                        \
                                    n-m*tilesize,                             \
                                    reduce,                                   \
                                    combine);                                 \
                                                                              \
            CILKPUB_C_DOWNSWEEP_##FNAME(0, m+1, tilesize,                     \
                                      r,                                      \
                                      n-m*tilesize,                           \
                                      initial,                                \
                                      combine,                                \
                                      scan);                                  \
            free(r);                                                          \
        }                                                                     \
    }


/// Declare the scan function.
#define CILKPUB_C_PARALLEL_SCAN_DECL(FNAME, T)                                \
    CILKPUB_C_UPSWEEP_DECL(FNAME, T);                                         \
    CILKPUB_C_DOWNSWEEP_DECL(FNAME, T);                                       \
    CILKPUB_C_PARALLEL_SCAN_BODY_DECL(FNAME, T);                              


/// Actually invoke the parallel scan.
#define CILKPUB_C_PARALLEL_SCAN(FNAME, n, initial_val, tilesize, reduce, combine, scan) \
    CILKPUB_C_PARALLEL_SCAN_##FNAME(n, initial_val, tilesize, reduce, combine, scan)



#endif // _CILKPUB_SCAN_C_H_
