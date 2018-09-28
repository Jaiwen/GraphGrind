/* dotmix_test_file_buffer.h                  -*-C++-*-
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
 * @file dotmix_test_file_buffer.h
 *
 * @brief Defines a simple class for creating a file filled with
 * random numbers.
 */

#ifndef __DOTMIX_TEST_FILE_BUFFER_H__
#define __DOTMIX_TEST_FILE_BUFFER_H__

#include <assert.h>
#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>


#include <cilk/cilk_api.h>

/**
 *@brief A simple class for creating a file filled with random
 *numbers.
 */
class DPRNGFile {

public:

    /**
     * @brief Create an empty file for storing random numbers.
     */ 
    DPRNGFile(const char* file_prefix, uint64_t seed)
        : m_current_idx(0)
        , m_total_count(0)
        , m_seed(seed)
        , m_f(NULL)
    {
        char fname[400];
        if (file_prefix) {
#ifdef _WIN32        
            _snprintf_s(fname,
                        400,
                        400,
                        "%s_Seed%llu.output",
                        file_prefix,
                        m_seed);

            // Make sure to write the file in binary mode.  Otherwise,
            // we get difference results between OS's when hashing the
            // file because write files in text mode gets handled in
            // different ways.
            fopen_s(&m_f, fname, "wb");
#else
            std::snprintf(fname, 400,
                          "%s_Seed%llu.output",
                          file_prefix,
                          m_seed);
            m_f = fopen(fname, "wb");
#endif
            assert(m_f);
//            std::printf("Opened output file %s\n", fname);
            memset(m_buf, 0, BLOCK_SIZE * sizeof(uint64_t));
        }
    }

    /**
     * @brief Add a number into the buffer.
     *
     * @return 1 if the current block was flushed
     * @return 0 if no write to file occurred 
     */
    int add_num(uint64_t num) {
        m_buf[m_current_idx] = num;
        m_current_idx++;
        m_total_count++;
    
        assert(m_current_idx <= BLOCK_SIZE);
        if (m_current_idx == BLOCK_SIZE) {
            flush_buffer_to_file();
            assert(m_current_idx == 0);
            return 1;
        }
        return 0;
    }

    /**
     * @brief Close the file.
     */
    ~DPRNGFile() {
        if (m_current_idx > 0) {
            flush_buffer_to_file();
        }
        if (m_f) {
            int error_code = fclose(m_f);
            assert(0 == error_code);
        }
    }

private:
    static const int BLOCK_SIZE = 4096*4;

    /// Buffer to store random numbers.
    uint64_t m_buf[BLOCK_SIZE];

    /// Current index into the buffer.
    int64_t m_current_idx;

    /// Total number of blocks written out to the file.
    int64_t m_total_count;

    /// Initial seed we used to generate the file.
    uint64_t m_seed;

    /// File to write data out to.
    FILE* m_f;

    void flush_buffer_to_file() {
        size_t num_written;
        // Flush the buffer and increment the number of blocks.
        assert(m_current_idx <= BLOCK_SIZE);
        num_written = fwrite(m_buf,
                             sizeof(uint64_t),
                             m_current_idx,
                             m_f);
        assert(num_written == m_current_idx);
        m_current_idx = 0;
    }
  
};


#endif // !defined(__DOTMIX_TEST_FILE_BUFFER_H__)
