/* creducer_opadd_array.h                  -*-C++-*-
 *
 *************************************************************************
 *
 * Copyright (C) 2013-14 Intel Corporation
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
 * @file creducer_opadd_array.h
 *
 * @brief Commutative reducer opadd array.
 *
 * \addtogroup CommutativeReducers Commutative Reducers
 *
 * @author Jim Sukha 
 * @version 1.04
 *
 *
 *
 * A commutative reducer array, that supports (commutative) +=
 * operations on array of elements of type T.
 *
 * The idea is similar to declaring an array of reducer objects,
 * except that then += operation needs to be commutative as well as
 * associative.
 *
 * @todo Add more documentation. :)
 */

#ifndef __CREDUCER_OPADD_ARRAY_H_
#define __CREDUCER_OPADD_ARRAY_H_

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>


namespace cilkpub {
    
/***********************************************************************/
// creducer_opadd_array_view

    /**
     * @brief A "view" for a commutative reducer array.
     *
     * Each view is effectively just an array of a specified size, either
     * allocated from the heap during construction, or using memory
     * specified by the original user.
     */
    template <typename T>
    class creducer_opadd_array_view {
    public:
        /// Convert an existing array of the specified size into a view.
        creducer_opadd_array_view(T* orig_array, int size);

        /// Construct an array of 'size' elements,
        /// each with initial value T().
        creducer_opadd_array_view(int size);

        // Destructor.
        ~creducer_opadd_array_view();

        /**
         * @brief Access an element at specified index.
         */
        inline T& operator[](int idx) {
            return m_a[idx];
        }

        /**
         * @brief Returns number of elements in the array.
         */
        int size() const {
            return m_size;
        }

        /// move_in and move_out from a view.  There several cases: 

        /**
         * @brief Move input array into this reducer array.
         *
         * For @c move_in:
         *
         *   1. If this view was constructed with an array parameter
         *      "orig_array", and input_array == orig_array, then this
         *      method does nothing.
         *   2. If this view was constructed with an array parameter
         *      "orig_array" which is != input_array, then the data is
         *      moved from input_array into orig_array.
         *   3. If this view was default-constructed, without an array
         *      parameter, then a move happens from input_array into the
         *      underlying memory.
         * 
         * @c move_out is the reverse.  If output_array matches orig_array,
         *  then nothing happens.  Otherwise, the data is moved into
         *  output_array.
         *
         * @warning If there is any aliasing or overlap in array
         * arguments that are being passed in, then bad things happen.
         * Don't do that. :)
         */
        void move_in(T* input_array);

        /**
         * @brief Move data from reducer array into output array.
         *
         * See @c move_in for details.
         */ 
        void move_out(T* output_array);


        /// TBD: Do we actually need this method?  I'm not quite sure.
        /// For primitive types (e.g., an int), this method might be
        /// simpler.
    
        /// When we move something out of an array slot (e.g., through a
        /// merge or move_out), clear what is left in that slot.
        inline static void reset_array_slot(T* a, int i) {
            a[i].~T();
            new (&(a[i])) T();
        }

        /// Get pointer to the underlying array for this view.
        /// This method should not be exposed to users...
        inline T* array_ptr() const {
            return m_a;
        }

    private:
        // Length of the array.
        int m_size;
    
        /// The pointer to the array for the view.
        T* m_a;

        // TBD: Fix this code to use an aligned malloc instead of
        // doing our own alignment calculation.

        // Number of bytes we use for padding and alignment, to avoid
        // false sharing, and make vector instructions happier.
        static const int CACHE_LINE_PADDING = 64;
    
        // The beginning of the memory that we allocate for the array.
        // We allocate enough extra space for alignment and padding.
        // If this view comes from an existing array, then
        // m_raw_memory is NULL.
        void* m_raw_memory;

    };


    template <typename T>
    creducer_opadd_array_view<T>::creducer_opadd_array_view(T* orig_array,
                                                            int size)
        : m_size(size)
        , m_a(orig_array)
        , m_raw_memory(NULL)
    {
    }

    template <typename T>
    creducer_opadd_array_view<T>::creducer_opadd_array_view(int size)
        : m_size(size)
        , m_a(NULL)
    {
        assert(size > 0);
        size_t total_size = sizeof(T) * m_size + 2 * CACHE_LINE_PADDING;
        m_raw_memory = malloc(total_size);
        int offset = CACHE_LINE_PADDING - ((size_t)m_raw_memory % CACHE_LINE_PADDING);
        m_a = (T*)((char*)m_raw_memory + offset);

        assert((char*)(m_a + m_size) < ((char*)m_raw_memory + total_size));
        assert(((size_t)m_a % CACHE_LINE_PADDING) == 0);

#ifdef DEBUG_CREDUCER    
        fprintf(stderr, "Allocating array view %p: mraw_memory = %p, size = %zd, m_a = %p\n",
                this, m_raw_memory, total_size, m_a);
#endif

        // Initialize every slot in the array with an identity.
        cilk_for (int i = 0; i < m_size; ++i) {
            new (&m_a[i]) T();
        }
    }

    template <typename T>
    creducer_opadd_array_view<T>::~creducer_opadd_array_view()
    {
        if (m_raw_memory) {
#ifdef DEBUG_CREDUCER
            fprintf(stderr, "Deleting view %p\n", this);
#endif
            cilk_for (int i = 0; i < m_size; ++i) {
                m_a[i].~T();
            }
            free(m_raw_memory);
        }
        else {
            // If we are destroying a view that is created from a user's
            // array, then we may have missed a corresponding move_out
            // call on a reducer.
            if (m_a) {
                fprintf(stderr,
                        "WARNING: view created with user-allocated array %p is being destroyed\n",
                        m_a);
            }
        }
    }

    template <typename T>
    void creducer_opadd_array_view<T>::move_in(T* input_array) {
        if (m_a != input_array) {
            if (m_a) {
                // This view already has its own memory, either allocated
                // directly, or it points to a different external array.
                // Just move the data in.
                cilk_for (int i = 0; i < m_size; ++i) {
                    m_a[i] = input_array[i];
                }
            }
            else {
                // Otherwise, there is no external array yet.  Move
                // something in.
                m_a = input_array;
            }
        }
        else {
            // Nothing needs to happen if the array is already the input.
        }
    }

    template <typename T>
    void creducer_opadd_array_view<T>::move_out(T* output_array) {
        if (m_a == output_array) {
            // Default case. Moving out into the array that is already the
            // one we specified.  Then, do nothing except clear the field.
            m_a = NULL;
        }
        else {
            if (m_a) {
                // Otherwise, move contents out into the output array.
                cilk_for (int i = 0; i < m_size; ++i) {
                    output_array[i] = m_a[i];
                    reset_array_slot(m_a, i);
                }
            }
        }
    }

    // End of creducer_opadd_array_view defintion.
    /***********************************************************************/



    /// Forward declaration of reducer opadd array.
    template <typename T> class creducer_opadd_array;

    /**
     * @brief Wrapper class for an indexing operation, which defers
     * the worker lookup.
     * 
     * This class wraps an indexing operation.
     * It exists mainly to guarantee type safety.
     *
     * The idea is that when you index an element of a reducer array x,
     *  x[i] returns this wrapped object.
     *
     * The only thing you are allowed to do with the object is a +=
     * operation.
     *
     *
     * This wrapping might also avoid potential issues with a
     * statement like "x[i] += fib(30);"
     *
     * If the indexing operation x[i] gets evaluated before fib(30) is
     * evaluated, then, since fib(30) might return on a different
     * worker than, we might lookup the wrong thread id?
     */
    template <typename T>
    class creducer_opadd_array_op {
    private:
        /// Pointer to the original array reducer.
        creducer_opadd_array<T>* m_reducer_ptr;

        /// Index for the lookup.
        int m_idx; 

    public:
        /// Construct a token the slot in the reducer for lookup.
        creducer_opadd_array_op(creducer_opadd_array<T>* cred,
                                int idx)
            : m_reducer_ptr(cred)
            , m_idx(idx) {
        }

        /// Perform the update operation by adding the value to the
        /// token.
        template <typename UpdateType>
        void operator +=(const UpdateType& x) {
            int wkr_id = __cilkrts_get_worker_number();
            (m_reducer_ptr->m_data[wkr_id])[m_idx] += x;
        }
    };


    template <typename T>
    class creducer_opadd_array {
    public:
        /**
         * @brief Construct an empty reducer array of "size" elements.
         *
         * Each slot in the array is initialized with an identity T().
         */
        creducer_opadd_array(int size);

        /**
         * @brief Construct an array of "size" elements from the
         * user-specified array.
         *
         * This call is semantically equivalent to
         * a default-constructed reducer array, followed by
         * a call to move_in(orig_array, size);
         *
         * The main difference is that this operation is more efficient
         * because it does not fill the empty array with identities.
         */
        creducer_opadd_array(T* orig_array, int size);

        /**
         * @brief Destroy the current reducer array.  This method
         * triggers a warning if there was no matching "move_out"
         * call.
         */
        ~creducer_opadd_array();
    
        // Methods for accessing and manipulating the current view.

        
        /**
         * @brief Access the element at specified index, in the
         * worker's current view.
         *
         * We'd really want to allow only a[i] += x.  Thus, we wrap
         * the reference that is returned in an object that only
         * supports one operation.
         *
         * TBD: Does this indirection confuse the compiler and inhibit
         *  some optimizations...?
         */
        creducer_opadd_array_op<T>
        operator[](int idx) {
            return creducer_opadd_array_op<T>(this, idx);
        }

        /**
         * @brief Move an array of the original size into this reducer
         * array.
         *
         * This operation fails with an error if we already have
         * an array there.
         *
         * The size of a should match the size the reducer array was
         * created with.
         */
        void move_in(T* a);

        /**
         * @brief Move data from reducer out into the specified array.
         */
        void move_out(T* a);

        /**
         * @brief Merge all updates into the "master" view.
         *
         * A move_out operation implicitly performs a merge.
         */
        void merge();

    private:
        /// Give the array_op access to the internals of this array.
        friend creducer_opadd_array_op<T>;
    
        creducer_opadd_array_view<T>* m_data;
        int m_P;
        int m_size;
        T* m_orig_array;


    
        // Helper method for constructor.
        inline void init(T* orig_array) {
            m_P = __cilkrts_get_nworkers();
            assert(m_P >= 1);
            m_data = (creducer_opadd_array_view<T>*)
                malloc(sizeof(creducer_opadd_array_view<T>) * m_P);

#ifdef DEBUG_CREDUCER 
            fprintf(stderr, "Init: allocating view memory %p, size = %zd\n",
                    m_data,
                    sizeof(creducer_opadd_array_view<T>) * m_P);
#endif            
        
            // Construct P views in parallel.
            cilk_for(int i = 0; i < m_P; ++i) {
                if (orig_array && (i ==0)) {
                    // If the user has specified an initial array, then the
                    // view at index 0 is special --- it is constructed
                    // from the user's array.
                    new (&m_data[0]) creducer_opadd_array_view<T>(orig_array,
                                                                  m_size);
                }
                else {
                    // Otherwise, just allocate a new array.
                    new (&m_data[i]) creducer_opadd_array_view<T>(m_size);
                }
            }
        }

        // Collapse all updates to the commutative reducer into the
        // view 0.
        void merge_internal(T* output, int p_start, int p_end);

        // Return the array of view 0.
        T* get_master_array() const {
            return m_data[0].array_ptr();
        }
    };



    // Default constructor.
    template <typename T>
    creducer_opadd_array<T>::creducer_opadd_array(int size_)
        : m_size(size_)
        , m_orig_array(NULL)
    {
        init(NULL);
    }

    // Construct, but link to an existing array.
    template <typename T>
    creducer_opadd_array<T>::creducer_opadd_array(T* orig_array_,
                                                  int size_)
        : m_size(size_)
        , m_orig_array(orig_array_)
    {
        init(m_orig_array);
    }

    // Destructor: free all the views.
    template <typename T>
    creducer_opadd_array<T>::~creducer_opadd_array() {
        if (m_orig_array) {
            fprintf(stderr,
                    "WARNING: destroying creducer_opadd_array %p without move_out?\n",
                    m_orig_array);
        }

        assert(m_data);
        cilk_for(int i = 0; i < m_P; ++i) {
            // Nothing special needs to happen at index 0 if that view
            // came from a user-specified array, because the
            // view object already knows where it came from.
            m_data[i].~creducer_opadd_array_view<T>();
        }
        free(m_data);
    }


    template <typename T>
    void creducer_opadd_array<T>::merge_internal(T* output,
                                                 int p_start,
                                                 int p_end) {
        cilk_for(int i = 0; i < m_size; ++i) {
            for (int p = p_start; p < p_end; ++p) {
                output[i] += m_data[p][i];
                creducer_opadd_array_view<T>::reset_array_slot(m_data[p].array_ptr(),
                                                               i);
            }
        }
    }



    template <typename T>
    void creducer_opadd_array<T>::move_in(T* a) {
        // Remember any array we pass in, if we don't already have one.
        if (!m_orig_array) {
            m_orig_array = a;
        }
        assert(m_data[0].size() == m_size);
        m_data[0].move_in(a);
    }

    template <typename T>
    void creducer_opadd_array<T>::merge() 
    {
        T* a = get_master_array();
        merge_internal(a, 1, m_P);
    }

    template <typename T>
    void creducer_opadd_array<T>::move_out(T* a) {
        // Check that the master array matches view 0.
        T* master = get_master_array();
        assert(m_data[0].array_ptr() == master);
        // Merge into view 0.
        merge_internal(master, 1, m_P);

        // Move out from view 0.
        m_data[0].move_out(a);

        // If we detect a move_out from the reducer, clear our original
        // array flag.
        if (m_orig_array == a) {
            m_orig_array = NULL;
        }
    }

};  // namespace cilkpub

#endif // defined(__CREDUCER_OPADD_ARRAY_H_)
