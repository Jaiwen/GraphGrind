/* pedigree_scope.h                  -*-C++-*-
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
 * @file pedigree_scope.h
 *
 * @brief Implementation of pedigree scope. 
 *
 *
 * @author Jim Sukha
 *
 * @see Pedigrees
 */

namespace cilkpub {

    template <int STATIC_PED_LENGTH>
    opt_pedigree_scope<STATIC_PED_LENGTH>::opt_pedigree_scope()
        : m_ped()
    {
        m_stop_node.rank = 0;
        m_stop_node.parent = NULL;
    }

    template <int STATIC_PED_LENGTH>
    opt_pedigree_scope<STATIC_PED_LENGTH>::~opt_pedigree_scope() 
    {
        // Nothing to do for destructor.
    }
    
    // Copy constructor.
    template <int STATIC_PED_LENGTH>
    opt_pedigree_scope<STATIC_PED_LENGTH>::opt_pedigree_scope(const opt_pedigree_scope& other)
        : m_ped(other.m_ped)
        , m_stop_node(other.m_stop_node)
    {
    }

    // Assignment operator. 
    template <int STATIC_PED_LENGTH>
    opt_pedigree_scope<STATIC_PED_LENGTH>&
    opt_pedigree_scope<STATIC_PED_LENGTH>::operator=(const opt_pedigree_scope& rhs) 
    {
        m_ped = rhs.m_ped;
        m_stop_node = rhs.m_stop_node;
        return *this;    
    }

    template <int STATIC_PED_LENGTH>
    opt_pedigree_scope<STATIC_PED_LENGTH>
    opt_pedigree_scope<STATIC_PED_LENGTH>::current()
    {
        opt_pedigree_scope<STATIC_PED_LENGTH> c;
        c.m_stop_node = __cilkrts_get_pedigree();
        c.m_ped = opt_pedigree<STATIC_PED_LENGTH>::current();
        return c;
    }

    template <int STATIC_PED_LENGTH>
    bool opt_pedigree_scope<STATIC_PED_LENGTH>::
    current_is_in_scope(const opt_pedigree_scope& other)
    {
        opt_pedigree_scope<STATIC_PED_LENGTH> c;
        c = opt_pedigree_scope<STATIC_PED_LENGTH>::current();
        return c.m_ped.is_prefix_of(other.m_ped);
    }

    template <int STATIC_PED_LENGTH>
    void opt_pedigree_scope<STATIC_PED_LENGTH>::fprint(FILE* f, const char* header) const
    {
        m_ped.fprint(f, header);
        std::fprintf(f, ": stop node: rank=%llu, ptr=%p\n",
                     m_stop_node.rank,
                     m_stop_node.parent);
    }

    template <int STATIC_PED_LENGTH>
    __cilkrts_pedigree opt_pedigree_scope<STATIC_PED_LENGTH>::get_stop_node() const
    {
        return m_stop_node;
    }
    
}; // namespace cilkpub
