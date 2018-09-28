/* detred_opadd.cpp                  -*-C++-*-
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

#include "detred_debug.h"

namespace cilkpub {

    // The reduce function for the monoid, which merges views.
    
    template <typename T>
    void det_reducer_monoid<T>::reduce(DetRedIview<T>* left,
                                       DetRedIview<T>* right)
    {
    #if CILKPUB_DBG_LEVEL(2)
        T left_val = left->get_value();
        T right_val = right->get_value();
    #endif

        left->merge(right);
        
    #if CILKPUB_DBG_LEVEL(2)
        T post_val = left->get_value();
        if (!detred_within_tol(left_val+right_val, post_val)) {
            CILKPUB_DBG_PRINTF(2,
                               "MERGE ERROR: left_val = %f, right_val=%f, post_val=%f\n",
                               left_val, right_val, post_val);
        }
        else {
            CILKPUB_DBG_PRINTF(3,
                               "Update: valid. left_val = %f, right_val = %f, post_val=%f\n",
                               left_val, right_val, post_val);
        }
    #endif
    }

    template <typename T>
    det_reducer_opadd<T>::det_reducer_opadd()
        : m_current_scope()
        , m_imp()
    {
    }
    
    template <typename T>
    det_reducer_opadd<T>::det_reducer_opadd(T v)
        : m_current_scope()
        , m_imp(v)
    {
    }

    template <typename T>
    det_reducer_opadd<T>::det_reducer_opadd(T v, const pedigree_scope& scope)
        : m_current_scope(scope)
        , m_imp(v)
    {
    }

    template <typename T>
    void det_reducer_opadd<T>::set_scope(pedigree_scope& scope)
    {
        m_current_scope = scope;
    }

    template <typename T>
    const pedigree_scope& det_reducer_opadd<T>::get_scope() const
    {
        return this->m_current_scope;
    }
    
    template <typename T>
    T det_reducer_opadd<T>::get_value() const
    {
        return get_m_imp()->view().get_value();
    }

    
    template <typename T>
    void det_reducer_opadd<T>::operator+=(const T& v) 
    {
    #if CILKPUB_DBG_LEVEL(2)
        T test_val = get_value();
    #endif            

        pedigree current_ped = pedigree::current(m_current_scope);
        get_m_imp()->view().update_with_pedigree(current_ped, v);

    #if CILKPUB_DBG_LEVEL(2)        
        T post_val = get_value();
        if (!detred_within_tol(test_val + v, post_val)) {
            CILKPUB_DBG_PRINTF(2, "ERROR: after update with v=%f, test_val = %f, post_val=%f\n",
                               v, test_val, post_val);
            __test_det_reducer_opadd::tfprint(stderr, *this);
            current_ped.fprint(stderr, "\n");
            CILKPUB_DBG_ERROR("Failed det_reducer_opadd update\n");
        }
    #endif            
    }

    
    class __test_det_reducer_opadd {
    public:
        template <typename T>
        static void tfprint(FILE* f, const det_reducer_opadd<T>& detred);
    };
    

    template <typename T>
    void __test_det_reducer_opadd::tfprint(FILE *f,
                                           const det_reducer_opadd<T>& detred)
    {
        fprintf(f, "DetReducer %p: ", &detred);
        detred.m_current_scope.fprint(f, "Scope: ");
        fprintf(f, "\nView: ");
        __test_DetRedIview::tfprint_iview(f, detred.get_m_imp()->view());
        fprintf(f, "\n");
    }


    

};  // namespace cilkpub
