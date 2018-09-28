/*  reducer_vector.h                  -*- C++ -*-
 *
 *  Copyright (C) 2013 Intel Corporation.  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *    * Neither the name of Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *  
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 *  AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 *  WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/** @file reducer_vector.h
 *
 *  @brief  Defines the monoid and view classes for reducers to create a
 *          standard vector in parallel.
 *
 *  @see @ref VectorReducers
 *
 *  @ingroup VectorReducers
 */

#ifndef REDUCER_VECTOR_H_INCLUDED
#define REDUCER_VECTOR_H_INCLUDED

#include <cilk/reducer.h>
#include <vector>
#include <list>

#if !(CILK_LIBRARY_VERSION >= 100)
#error "The vector reducer definitions require Cilk library version 1.0 or later"
#endif

/** @defgroup VectorReducers Vector Reducers
 *
 *  @author Neil Faiman
 *
 *  @version 1.03
 *
 *  @req Cilk Library version 1.0
 *
 *  A vector reducer allows the creation of a standard vector by appending
 *  elements to it in parallel.
 *
 *  You should be familiar with Cilk reducers, described in the “Cilk Plus
 *  Reducers” page of the Cilk Plus Include Directory documentation.
 *
 *  @section redvector_usage Usage Example
 *
 *      typedef ... SourceData;
 *      typedef ... ResultData;
 *      vector<SourceData> input;
 *      ResultData expensive_computation(const SourceData& x);
 *      cilk::reducer< cilkpub::op_vector<ResultData> > r;
 *      cilk_for (int i = 0; i != input.size(); ++i) {
 *          r->push_back(expensive_computation(input[i]));
 *      }
 *      vector result;
 *      r.move_out(result);
 *
 *  @section redvector_monoid The Monoid
 *
 *  @subsection redvector_monoid_values Value Set
 *
 *  The value set of a vector reducer is the set of values of the class
 *  `std::vector<Type, Alloc>`, which we refer to as “the reducer’s vector 
 *  type”.
 *
 *  @subsection redvector_monoid_operator Operator
 *
 *  The operator of a vector reducer is vector concatenation.
 *
 *  @subsection redvector_monoid_identity Identity
 *
 *  The identity value of a vector reducer is the empty vector, which is the
 *  value of the expression `std::vector<Type, Alloc>([allocator])`.
 *
 *  @section redvector_operations Operations
 *
 *  In the operation descriptions below, the type name `Vector` refers to 
 *  the reducer’s vector type, `std::vector<Type, Alloc>`.
 *
 *  @subsection redvector_constructors Constructors
 *
 *  Any argument list which is valid for a `std::vector` constructor is valid
 *  for a vector reducer constructor. The usual move-in constructor is also 
 *  provided:
 *
 *      reducer(move_in(Vector& variable))
 *
 *  @subsection redvector_get_set Set and Get
 *
 *      void r.set_value(const Vector& value)
 *      const Vector& r.get_value() const
 *      Vector& r.get_value()
 *      void r.move_in(Vector& variable)
 *      void r.move_out(Vector& variable)
 *
 *  @subsection redvector_initial Initial Values
 *
 *  A vector reducer with no constructor arguments, or with only an allocator 
 *  argument, will initially contain the identity value, an empty vector.
 *
 *  @subsection redvector_view_ops View Operations
 *
 *  The view of a vector reducer provides the following member functions:
 *
 *      void push_back(const Type& element) 
 *      void insert_back(const Type& element) 
 *      void insert_back(Vector::size_type n, const Type& element) 
 *      template <typename Iter> void insert_back(Iter first, Iter last)
 *
 *  The `push_back` functions is the same as the corresponding `std::vector`
 *  function. The `insert_back` function is the same as the `std::vector` 
 *  `insert` function, with the first parameter fixed to the end of the vector.
 *
 *  @section redvector_performance Performance Considerations
 *
 *  Vector reducers work by creating a vector for each view, collecting those
 *  vectors in a list, and then concatenating them into a single result vector 
 *  at the end of the computation. This last step takes place in serial code, 
 *  and necessarily takes time proportional to the length of the result vector. 
 *  Thus, a parallel vector reducer cannot actually speed up the time spent
 *  directly creating the vector. This trivial example would probably be slower 
 *  (because of reducer overhead) than the corresponding serial code:
 *
 *      vector<T> a;
 *      reducer<op_vector<T> > r;
 *      cilk_for (int i = 0; i != a.length(); ++i) {
 *          r->push_back(a[i]);
 *      }
 *      vector<T> result;
 *      r.move_out(result);
 *
 *  What a vector reducer _can_ do is to allow the _remainder_ of the 
 *  computation to be done in parallel, without having to worry about
 *  managing the vector computation.
 *
 *  The vectors for new views are created (by the view identity constructor) 
 *  using the same allocator as the vector that was created when the reducer 
 *  was constructed. Note that this allocator is determined when the reducer 
 *  is constructed. The following two examples may have very different 
 *  behavior:
 *
 *      vector<Type, Allocator> a_vector;
 *
 *      reducer< op_vector<Type, Allocator> reducer1(move_in(a_vector));
 *      ... parallel computation ...
 *      reducer1.move_out(a_vector);
 *
 *      reducer< op_vector<Type, Allocator> reducer2;
 *      reducer2.move_in(a_vector);
 *      ... parallel computation ...
 *      reducer2.move_out(a_vector);
 *
 *  *   `reducer1` will be constructed with the same allocator as `a_vector`, 
 *      because the vector was specified in the constructor. The `move_in` 
 *      and`move_out` can therefore be done with a `swap` in constant time.
 *  *   `reducer2` will be constructed with a _default_ allocator of type
 *      `Allocator`, which may not be the same as the allocator of `a_vector`.
 *      Therefore, the `move_in` and `move_out` may have to be done with a 
 *      copy in _O(N)_ time.
 *
 *  (All instances of an allocator class with no internal state (like 
 *  `std::allocator`) are “the same”. You only need to worry about the “same
 *  allocator” issue when you create vector reducers with a custom allocator 
 *  class that has data members.)
 *
 *  @section redvector_types Type and Operator Requirements
 *
 *  `std::vector<Type, Alloc>` must be a valid type.
*/

namespace cilkpub {

/** @brief The vector reducer view class.
 *
 *  This is the view class for reducers created with
 *  `cilk::reducer< cilkpub::op_vector<Type, Allocator> >`. It holds the 
 *  accumulator variable for the reduction, and allows only append operations 
 *  to be performed on it.
 *
 *  @note   The reducer “dereference” operation (`reducer::operator *()`) 
 *          yields a reference to the view. Thus, for example, the view 
 *          class’s `push_back` operation would be used in an expression like
 *          `r->push_back(a)`, where `r` is a vector reducer variable.
 *
 *  @tparam Type        The vector element type (not the vector type).
 *  @tparam Alloc       The vector allocator type.
 *
 *  @see @ref VectorReducers
 *  @see op_vector
 *  @ingroup VectorReducers
 */
template<typename Type, typename Alloc>
class op_vector_view
{
    typedef std::vector<Type, Alloc>                vector_type;
    typedef std::list<vector_type>                  list_type;
    typedef typename vector_type::size_type         size_type;

    // The view's value is represented by a list of vectors. The value is the
    // concatenation of the vectors in the list. All vector operations apply 
    // to the last vector in the list; reduce operations cause lists of partial
    // vectors from multiple strands to be combined.
    //
    mutable list_type                               m_list;

    // Before returning the value of the reducer, concatenate all the vectors 
    // in the list. Leaves the list with a single vector containing all the
    // elements of the concatenated vectors.
    //
    void flatten() const
    {
        if (m_list.size() == 1) return;
        
        typename list_type::iterator i;
        vector_type& head = m_list.front();

        // Compute the total number of elements in all the vectors in the list.
        size_type len = 0;
        for (i = m_list.begin(); i != m_list.end(); ++i)
            len += i->size();

        // Reserve enough space in the first vector to hold all the elements of
        // all the vectors.
        head.reserve(len);

        // Copy all the elements of all the vectors in the list into the
        // first vector.
        for (i = ++m_list.begin(); i != m_list.end(); ++i)
            head.insert(head.end(), i->begin(), i->end());
        
        // Erase all but the first vector from the list.
        m_list.erase(++m_list.begin(), m_list.end());
    }
    
    // This view’s vector; or _the_ vector, if the view has been flattened.
    //
    const vector_type& vector() const { return m_list.back(); }
          vector_type& vector()       { return m_list.back(); }

public:

    /** @name Monoid support.
     */
    //@{

    /// Required by cilk::monoid_with_view
    typedef vector_type value_type;

    /// Required by @ref op_vector
    Alloc get_allocator() const
    {
        return vector().get_allocator();
    }

    /** Reduce operation. Required by cilk::monoid_with_view.
     */
    void reduce(op_vector_view* other)
    {
        m_list.splice(m_list.end(), other->m_list);
    }
    
    //@}

    /** @name Pass constructor arguments through to the vector constructor.
     */
    //@{

    op_vector_view() :
        m_list(1, vector_type()) {}

    template <typename T1>
    op_vector_view(const T1& x1) :
        m_list(1, vector_type(x1)) {}

    template <typename T1, typename T2>
    op_vector_view(const T1& x1, const T2& x2) :
        m_list(1, vector_type(x1, x2)) {}

    template <typename T1, typename T2, typename T3>
    op_vector_view(const T1& x1, const T2& x2, const T3& x3) :
        m_list(1, vector_type(x1, x2, x3)) {}

    template <typename T1, typename T2, typename T3, typename T4>
    op_vector_view(const T1& x1, const T2& x2, const T3& x3, const T4& x4) :
        m_list(1, vector_type(x1, x2, x3, x4)) {}

    //@}

    /** Move-in constructor.
     */
    explicit op_vector_view(cilk::move_in_wrapper<value_type> w)
        : m_list(1, vector_type(w.value().get_allocator()))
    {
        vector().swap(w.value());
    }

    /** @name Reducer support.
     */
    //@{

    void view_move_in(vector_type& v)
    {
        m_list.clear();
        if (m_list.back().get_allocator() == v.get_allocator()) {
            // Equal allocators. Do a (fast) swap.
            m_list.push_back(vector_type(get_allocator()));
            vector().swap(v);
        }
        else {
            // Unequal allocators. Do a (slow) copy.
            m_list.push_back(v);
        }
        v.clear();
    }

    void view_move_out(vector_type& v)
    {
        flatten();
        if (m_list.back().get_allocator() == v.get_allocator()) {
            // Equal allocators.  Do a (fast) swap.
            vector().swap(v);
        }
        else {
            // Unequal allocators.  Do a (slow) copy.
            v = vector();
            vector().clear();
        }
    }

    void view_set_value(const vector_type& v) 
    {
        m_list.clear();
        m_list.push_back(v); 
    }

    vector_type const& view_get_value()     const 
    {
        flatten(); 
        return vector(); 
    }

    //@}

    /** @name View modifier operations.
     *
     *  @details These simply wrap the corresponding operations on the
     *  underlying vector.
     */
    //@{

    /** Add an element at the end of the list.
     *
     *  Equivalent to `vector.push_back(…)`
     */
    void push_back(const Type x) 
    {
        vector().push_back(x); 
    }

    /** @name Insert elements at the end of the vector.
     *
     *  Equivalent to `vector.insert(vector.end(), …)`
     */
    //@{

    void insert_back(const Type& element) 
        { vector().insert(vector().end(), element); }

    void insert_back(typename vector_type::size_type n, const Type& element) 
        { vector().insert(vector().end(), n, element); }

    template <typename Iter>
    void insert_back(Iter first, Iter last)
        { vector().insert(vector().end(), first, last); }

    //@}
    

    //@}
};


/** @brief The vector reducer monoid class. 
 *
 *  Instantiate the cilk::reducer template class with an op_vector monoid to
 *  create a vector reducer class. For example, to concatenate a
 *  collection vectors of integers:
 *
 *      cilk::reducer< cilkpub::op_vector<int> > r;
 *
 *  @tparam Type        The vector element type (not the vector type).
 *  @tparam Alloc       The vector allocator type.
 *
 *  @see VectorReducers
 *  @see op_vector_view
 *  @ingroup VectorReducers
 */
template<typename Type, typename Alloc = std::allocator<Type> >
class op_vector : 
    public cilk::monoid_with_view< op_vector_view<Type, Alloc>, false >
{
    typedef cilk::monoid_with_view< op_vector_view<Type, Alloc>, false > base;
    
    // The allocator to be used when constructing new views.
    Alloc m_allocator;
    
    using base::provisional;

public:

    /// View type.
    typedef typename base::view_type view_type;

    /** Constructor.
     *
     *  There is no default constructor for vector monoids, because the
     *  allocator must always be specified.
     *
     *  @param  allocator   The list allocator to be used when
     *                      identity-constructing new views.
     */
    op_vector(const Alloc& allocator = Alloc()) : m_allocator(allocator) {}

    /** Create an identity view.
     *
     *  Vector view identity constructors take the vector allocator as an
     *  argument.
     *
     *  @param v    The address of the uninitialized memory in which the view 
     *              will be constructed.
     */
    void identity(view_type *v) const
    {
        ::new((void*) v) view_type(m_allocator); 
    }

    /** @name construct functions
     *
     *  A vector reduction monoid must have a copy of the allocator of 
     *  the leftmost view’s vector, so that it can use it in the `identity`
     *  operation. This, in turn, requires that vector reduction monoids have a
     *  specialized `construct()` function.
     *
     *  All vector reducer monoid `construct()` functions first construct the
     *  leftmost view, using the arguments that were passed in from the reducer
     *  constructor. They then call the view’s `get_allocator()` function to 
     *  get the vector allocator from the vector in the leftmost view, and pass
     *  that to the monoid constructor.
     */
    //@{

    static void construct(op_vector* monoid, view_type* view)
        { provisional( new ((void*)view) view_type() ).confirm_if(
            new ((void*)monoid) op_vector(view->get_allocator()) ); }

    template <typename T1>
    static void construct(op_vector* monoid, view_type* view, const T1& x1)
        { provisional( new ((void*)view) view_type(x1) ).confirm_if(
            new ((void*)monoid) op_vector(view->get_allocator()) ); }

    template <typename T1, typename T2>
    static void construct(op_vector* monoid, view_type* view, const T1& x1, const T2& x2)
        { provisional( new ((void*)view) view_type(x1, x2) ).confirm_if(
            new ((void*)monoid) op_vector(view->get_allocator()) ); }

    template <typename T1, typename T2, typename T3>
    static void construct(op_vector* monoid, view_type* view, const T1& x1, const T2& x2,
                            const T3& x3)
        { provisional( new ((void*)view) view_type(x1, x2, x3) ).confirm_if(
            new ((void*)monoid) op_vector(view->get_allocator()) ); }

    //@}
};


} // namespace cilkpub

#endif //  REDUCER_VECTOR_H_INCLUDED
