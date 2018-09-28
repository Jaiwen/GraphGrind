/* cilktest_timing.h                  -*-C++-*-
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
 * @file cilktest_timing.h
 *
 * @brief Apparatus for performance-test timing.
 */

#ifndef INCLUDED_CILKTEST_TIMING_DOT_H
#define INCLUDED_CILKTEST_TIMING_DOT_H

#include <cilk/common.h>

#ifdef __cplusplus
#   include <cstdio>
#else
#   include <stdio.h>
#endif

#ifdef _WIN32
#   ifndef _WINBASE_
__CILKRTS_BEGIN_EXTERN_C
unsigned long __stdcall GetTickCount();
__CILKRTS_END_EXTERN_C
#   endif
#endif  // _WIN32

#if defined __unix__ || defined __APPLE__
#   include <sys/time.h>
#endif  // defined __unix__ || defined __APPLE__

/// @brief Return the system clock with millisecond resolution
///
/// This function returns a long integer representing the number of
/// milliseconds since an arbitrary starting point, e.g., since the system was
/// started or since the Unix Epoch.  The result is meaningless by itself, but
/// the difference between two sequential calls to CILKTEST_GETTICKS()
/// represents the time interval that elapsed between them (in ms).
static inline unsigned long long CILKTEST_GETTICKS()
{
    unsigned long long ans;
    // When inlined, prevent code motion around this call
#if __INTEL_COMPILER > 1200
    __notify_zc_intrinsic((void*) "CILKTEST_GETTICKS_START", 0);
#endif

#ifdef _WIN32
    // Return milliseconds elapsed since the system started
    ans = GetTickCount();
#elif defined(__unix__) || defined(__APPLE__)
    // Return milliseconds elapsed since the Unix Epoch
    // (1-Jan-1970 00:00:00.000 UTC)
    struct timeval t;
    gettimeofday(&t, 0);
    ans = t.tv_sec * 1000ULL + t.tv_usec / 1000;
#else
#   error CILKTEST_GETTICKS() not implemented for this OS
#endif
    /* UNREACHABLE */

    // When inlined, prevent code motion around this call
#if __INTEL_COMPILER > 1200
    // Isn't that the point of having the annotations?
    __notify_zc_intrinsic((void*) "CILKTEST_GETTICKS_END", 0);
#endif
    return ans;
}

/**
 * @brief Time the specified expression and store answer into t.
 *
 * @param t     Variable to store time into.
 * @param expr  Expression to time
 */
#define CILKTEST_GET_TIMING(t, expr) {                    \
        t = CILKTEST_GETTICKS();                          \
        expr;                                             \
        t = CILKTEST_GETTICKS() - t;                      \
    }

/**
 * @brief Get and print the time to execute specified expression.
 *
 * @param expr  Expression to time
 */
#define CILKTEST_PRINT_TIMING(expr) {                                   \
        unsigned long long timing;                                      \
        CILKTEST_GET_TIMING(timing, expr);                              \
        __STDNS printf("time for %s = %llu ms\n", #expr, timing);       \
    }

#endif // ! defined(INCLUDED_CILKTEST_TIMING_DOT_H)
