/* detred_debug.h                  -*-C++-*-
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

#ifndef __DETRED_DEBUG_H_
#define __DETRED_DEBUG_H_

#include <cstdio>
#include <cstdlib>
#include <cstdarg>

// Compile-time flag for setting the debugging level of
// deterministic reducer code. 
#ifndef CILKPUB_DETRED_DBG_LEVEL
#define CILKPUB_DETRED_DBG_LEVEL 1
#endif

namespace cilkpub {

    inline void cilkpub_dbg_error(const char* file,
                                 int line,
                                 const char* msg)
    {
        std::fprintf(stderr, "DETRED ERROR at %s:%d --- %s\n",
                     file, line, (msg ? msg : "Unknown"));
        assert(0);
    }

    inline void cilkpub_dbg_test_assert(int dbg_level,
                                        bool test_condition,
                                        const char* file,
                                        int line,
                                        const char* msg)
    {
        if (dbg_level >= CILKPUB_DETRED_DBG_LEVEL) {
            if (!test_condition) {
                cilkpub_dbg_error(file, line, msg);
            }
        }
    }

    #define CILKPUB_DETRED_DBG_ASSERT(dbg_level, c) do {                    \
            cilkpub_dbg_test_assert((dbg_level), (c), __FILE__, __LINE__, #c); \
        } while (0)


    #define CILKPUB_DBG_ERROR(msg)

    // True if dbg_level >= our set debug level.
    #define CILKPUB_DBG_LEVEL(print_level) ((print_level <= CILKPUB_DETRED_DBG_LEVEL))

    inline int CILKPUB_DBG_PRINTF(int level, const char* fmt, ...)
    {
        if (CILKPUB_DETRED_DBG_LEVEL < level)
            return 0;
        va_list ap;
        va_start(ap, fmt);
        int ret = std::vfprintf(stderr, fmt, ap);
        va_end(ap);
        return ret;
    }


    inline bool detred_within_tol(double x, double y)
    {
        return ((x - y) * (x - y) < 1.0e-12);
    }

};


#endif // __DETRED_DEBUG_H
