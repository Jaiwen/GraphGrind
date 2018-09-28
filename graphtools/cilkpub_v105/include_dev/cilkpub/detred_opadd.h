/* detred_opadd.h                  -*-C++-*-
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

#ifndef __DETREDUCE_OPADD_H_
#define __DETREDUCE_OPADD_H_

#include <cilk/reducer.h>

#include <cilkpub/pedigrees.h>
#include "internal/detred_iview.h"
#include "internal/detred_debug.h"

namespace cilkpub {

    template <typename T>
    class det_reducer_monoid : public cilk::monoid_base<DetRedIview<T> >
    {
        // Define this enum for a custom reducer, so that the reducer
        // library does not expect this reducer to be aligned by
        // default.  
        // enum { align_reducer = false };
    public:
        static void reduce(DetRedIview<T>* left,
                           DetRedIview<T>* right);
    };
    

    template <typename T>
    class det_reducer_opadd
    {

    public:

        /**
         * @brief Construct a reducer with identity initial value,
         * empty pedigree, at global scope.
         */
        det_reducer_opadd();

        /**
         * @brief Construct a reducer with initial value v, empty
         * pedigree, at global scope.
         */
        det_reducer_opadd(T v);

        /**
         * @brief Construct a reducer with initial value v, empty
         * pedigree, at specified scope.
         */
        det_reducer_opadd(T v, const pedigree_scope& scope);

        void set_scope(pedigree_scope& scope);
        const pedigree_scope& get_scope() const;

        /// Returns the value of the current view of the reducer.
        T get_value() const;

        /// Add v to the current view.
        void operator+= (const T& v);

    private:
        pedigree_scope m_current_scope;
        
        // The actual reducer object.
        cilk::reducer<det_reducer_monoid<T> > m_imp;
        /// Get the pointer to the reducer object.
        inline const cilk::reducer<det_reducer_monoid<T> >* get_m_imp() const
        {
            return &m_imp;
        }

        inline cilk::reducer<det_reducer_monoid<T> >* get_m_imp() 
        {
            return &m_imp;
        }

        friend class __test_det_reducer_opadd;

    };
};  // namespace cilkpub


#include "internal/detred_opadd.cpp"


#endif
