/* pedigrees.h                  -*-C++-*-
 *
 *************************************************************************
 *
 * Copyright (C) 2012-13 Intel Corporation
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
 * @file pedigrees.h
 *
 * @brief Classes for manipulating pedigree objects.
 *
 * @defgroup Pedigrees Pedigrees
 *
 * @author Jim Sukha
 * @version 1.03
 
 * @req Requires a compiler that implements Cilk Plus ABI 1 or later.
 * Also uses the pedigree API as implemented in Intel Cilk
 * Plus runtime, build 2561 or higher.
 *
 *
 *
 * @section pedigree_overview Overview
 *
 * In Cilk Plus, a strand of execution can be identfied by a @b
 * pedigree, a variable-length sequence of @c uint64_t values that
 * reflect the sequence of spawns and syncs in the computation leading
 * up to that strand.
 *
 * 
 *
 * Conceptually, as a program executes, the pedigree for the currently
 * executing strand changes at the boundaries of (maximal) strands.
 * The boundaries of maximal strands occur at each @c cilk_spawn, @c
 * cilk_sync, and in each grain of a @c cilk_for.  A call to @c
 * __cilkrts_bump_worker_rank() also corresponds to a strand boundary.
 * See the following <a href=http://software.intel.com/sites/products/documentation/doclib/stdxe/2013/composerxe/compiler/cpp-mac/hh_goto.htm#GUID-54163CE9-E866-4C6D-B0D4-0613DD2EA984.htm>online documentation</a> for more details.
 *
 * 
 * Pedigrees in Cilk Plus preserve three important properties for any
 * execution of a Cilk Plus program:
 *
 * -# <b>Uniqueness</b>: Every maximal strand in the execution DAG has
 *    a distinct pedigree.
 * -# <b>Lexicographic ordering</b>: In any serial execution of a Cilk Plus program, 
 *    the pedigrees of strands increase in a "dictionary" ordering.
 * -# <b>Determinism</b>: For any "DAG-invariant" Cilk Plus program,
 *    i.e., a program that always generates the same execution DAG
 *    when executed on any number of workers, the pedigree for each
 *    strand in the DAG is always the same across multiple runs of the
 *    program.
 *
 * 
 * @section pedigree_example An Example
 *
 * The following program conceptually illustrates pedigrees, and also
 * provides an example program using cilkpub::pedigree objects.
 *
 * @code
 *   int main(void) {
 *      cilkpub::pedigree ped0 = cilkpub::pedigree::current();  // Gets the current pedigree
 *      int x = 0;       // ped0 starts at [0, 0].
 *
 *      cilk_spawn f();  // Pedigree inside f() starts at [0, 0, 0].
 *
 *      cilkpub::pedigree ped1 = cilkpub::pedigree::current();  // Gets the current pedigree
 *      x++;             // ped1 in this continuation is [0, 1].
 *
 *      cilk_spawn g();  // Pedigree inside g() starts at [0, 1, 0].
 *
 *      cilkpub::pedigree ped2 = cilkpub::pedigree::current();  // Gets the current pedigree
 *      x++;             // ped2 in this continuation is [0, 2].
 *
 *      h();             // Pedigree in h() starts at [0, 2].
 *                       // In h(), the pedigree may have been incremented by k, to [0, k+2].

 *      cilkpub::pedigree ped3 = cilkpub::pedigree::current();  // Gets the current pedigree
 *                       // ped3 is now [0, k+2]
 *
 *      cilk_sync;
 *
 *      cilkpub::pedigree ped4 = cilkpub::pedigree::current();  // Gets the current pedigree
 *                       // ped4 is now [0, k+3]
 *
 *   }
 * @endcode
 *
 *
 *
 * @section pedigree_implementation Implementation Details
 *
 * @note This section describes the current implementation of
 * pedigrees in Cilk Plus. In general, one should not write code that
 * depends on pedigrees have specific numerical values, as the exact
 * values may change with different compiler optimizations or
 * potentially in other pedigree implementations.
 * 
 * Pedigrees can be defined in terms of a <b>pedigree stack</b> for
 * the serial execution of a Cilk program, with the least-significant
 * term of the pedigree being stored at the active end of the stack.
 * The pedigree for a strand is defined as the value of the pedigree
 * stack at the the time the strand would be executed during a serial
 * execution.
 *
 * The pedigree stack is maintained using a few simple rules.  For a
 * Cilk program that has a single user pthread, (the common case), the
 * initial pedigree stack begins with the sequence [0, 0].  The
 * pedigree stack is updated as follows:
 *  - @c cilk_spawn of @c g(): When a function @c f() spawns a
 *                             function @c g(), push an new least significant term
 *                             of "0" onto the stack.
 *  - Return from @c g(): Pop the least significant term off the pedigree stack,
 *                        then increment the new least significant term on the
 *                        stack for the continuation of the spawn of
 *                        @c g() in @c f().
 *  - Reach a @c cilk_sync: Increment the least significant term of the pedigree.
 *  - Call to @c __cilkrts_bump_worker_rank(): Increments the least significant term in the pedigree.
 *  .
 *
 *  The above description assumes the code has no @c cilk_for loops.
 *  @ref pedigree_other_details describes how @c cilk_for
 *  loops are handled.
 *
 *  Note that no action is taken to change the pedigree stack on the
 *  call of a function @c h(), even if that function @c h() itself
 *  spawns.

 *
 * The file pedigrees.h contains the @ref cilkpub::opt_pedigree class,
 * which stores a pedigree object.  This class requires a template
 * argument that specifies the number of terms to statically allocate
 * for each pedigree object.
 *
 * The cilkpub::opt_pedigree class also provides various utility
 * methods for comparing and reading a pedigree object.  This object
 * is immutable once it is constructed.
 *
 * For convenience, cilkpub::pedigree is defined as a
 * cilkpub::opt_pedigree object with a default size.
 *
 * @ref pedigrees.h also implements scoped pedigrees.  For more
 * information, see @ref cilkpub::opt_pedigree_scope.
 *
 * Note that it is possible to read in the current pedigree directly
 * using the API described in @c cilk/cilk_api.h, without using the
 * classes in Cilkpub classes.  The cilkpub::opt_pedigree and other
 * classes are provided only to make working with pedigrees simpler
 * and more convenient.
 *
 *

 * @section pedigree_other_details Other Special Cases
 *
 *  Technically, maintaining pedigrees in a @c cilk_for loop requires
 *  no special effort, since a @c cilk_for loop naturally recursively
 *  decomposes into @c cilk_spawn and @c cilk_sync statements.  This
 *  decomposition adds more terms to the pedigree than is strictly
 *  necessary, however.
 *
 *  In Cilk Plus, pedigrees for a @c cilk_for are appropriately
 *  "flattened", so that entering the body of the loop adds only
 *  lengthens the pedigree by two terms.
 *
 * @code
 *   // Suppose the initial pedigree here is [a, b, c]
 *   // Assume grain size of loop is 1.
 *   cilk_for(int i = 0; i < 100; ++i) {
 *       int x = 0;       // The pedigree for iteration i begins at  [a, b, c, i, 0]
 *       cilk_spawn f(i);
 *       x++;             // The pedigree here is [a, b, c, i, 1]
 *       cilk_sync;
 *       x++;             // The pedigree here is [a, b, c, i, 2]
 *   }
 * @endcode
 *
 * When a @c cilk_for has a grain size which is greater than 1, by
 * default, we may only get a single pedigree for the beginning of
 * each grain, rather than each iteration.  For example, in the above
 * example, if the grain size is 10, we may have pedigrees  of
 * [a, b, c, i, 0], [a, b, c, i, 10], [a, b, c, i, 20], etc.
 * 
 * To ensure a distinct pedigree for each iteration, one can
 * explicitly call @c __cilkrts_bump_loop_rank() at the beginning of the
 * body of the @c cilk_for.
 *
 * For programs with multiple user pthreads, the most significant term
 * of the pedigree counts the pthreads created.  Thus, strands that
 * are created in Cilk computations from different pthreads still have
 * distinct pedigrees.  These pedigrees may not be deterministic,
 * however, unless the programmer creates pthreads in a deterministic
 * fashion.

 *
 * For more information about pedigrees, see @ref pedigree_refs below.
 *
 *
 *  @todo Add more extensive testing of assignment and copy construction.
 *  @todo Add performance tests.
 *

 * @section pedigree_refs References on Pedigrees
 * 
 * For a more precise description of how a Cilk computation generates
 * pedigrees, see the paper:
 *
 *   -  <I>Deterministic parallel random-number generation for dynamic-multithreading platforms</I>
 *      by Charles E. Leiserson, Tao B. Schardl, and Jim Sukha.
 *
 * This paper is available at:
 *   -  http://dl.acm.org/citation.cfm?id=2145841
 *   -  http://supertech.csail.mit.edu/papers/dprng.pdf
 *
 * @see Pedigrees
 * @see DotMix
 *
 */


#ifndef __CILKPUB_PEDIGREES_H_
#define __CILKPUB_PEDIGREES_H_

#include <cstdio>
#include <cstdlib>
#include <iterator>

#include <cilk/cilk_api.h>

#if defined(__INTEL_COMPILER) && (__INTEL_COMPILER > 1200) && (__INTEL_COMPILER < 1300)
// For v12.1 of the Intel compiler, explicitly include an extra header
// for backwards compatibility.
extern "C" {
#    include "internal/pedigrees_12_1_compat.h"
}

#elif (__CILKRTS_ABI_VERSION < 1) || (__INTEL_COMPILER <= 1200)
// Check for other compilers.  Technically, this check should subsume
// the one above, but I'll leave both in for now.
#   error "<cilkpub/pedigrees.h> only works on compilers supporting Cilk Plus ABI 1 or higher."
#endif

namespace cilkpub {

    /// The default number of pedigree terms that a pedigree object
    /// will statically allocates space for.
    const int DEFAULT_STATIC_PED_LENGTH = 16;
    
    /// Forward declaration of a pedigree scope.
    template <int STATIC_PED_LENGTH>
    class opt_pedigree_scope;

    /**
     * @brief An object that stores a pedigree.
     *
     * A pedigree is stored in an array, in reverse order.  This order
     * is chosen for storage because the pedigree API in @c cilk_api.h
     * can only extract the terms of the current pedigree in this
     * order, i.e., from leaf to root.
     *
     * In this implementation, a pedigree object statically allocates
     * enough space for @c STATIC_PED_LENGTH terms.  If the pedigree
     * is longer than this length, the static buffer is unused, and
     * the pedigree is stored in a buffer allocated from the heap.
     *
     * This class is templated so that users can specify the size of
     * the static array with the template parameter.
     */
    template <int STATIC_PED_LENGTH=DEFAULT_STATIC_PED_LENGTH>
    class opt_pedigree {
    public:
        /**
         * @brief Creates an empty pedigree.
         *
         * An empty pedigree has length 0. 
         */
        opt_pedigree();

        /**
         * @brief Constructs a pedigree from an array of @c d pedigree
         * terms.
         *
         * @param buffer      Buffer of pedigree terms
         * @param d           Number of terms in buffer
         * @param is_reversed Should be true if the buffer stores
         *   terms from leaf to root (the order they can be
         *   extracted).  Otherwise, the buffer stores terms from root
         *   to leaf.
         */
        opt_pedigree(uint64_t* buffer, int d, bool is_reversed);

        /// Copy constructor.
        opt_pedigree(const opt_pedigree& ped);

        /// Destructor.
        ~opt_pedigree();

        /// Assignment operator
        opt_pedigree<STATIC_PED_LENGTH>& operator=(const opt_pedigree<STATIC_PED_LENGTH>& ped);
        
        /// Returns a pedigree object matching the current pedigree.
        static opt_pedigree<STATIC_PED_LENGTH> current();

        /**
         * @brief Returns a pedigree object matching the current
         * pedigree in the specified scope.
         *
         * @pre The current pedigree must fall into the scope of @c
         * scope, AND @c scope must still be active.
         * @see pedigree_scope
         *
         * @warning An optimized implementation may fail in subtle
         * ways if the preconditions are not satisfied.
         *
         * @todo We could implement a debug build of pedigrees that
         * could validate whether the current pedigree is in scope or
         * not, by always reading the entire pedigree.
         *
         * @param scope  The scope to apply to the current pedigree.
         * @return The current pedigree, scoped appropriately. 
         */
        static opt_pedigree<STATIC_PED_LENGTH> current(const opt_pedigree_scope<STATIC_PED_LENGTH>& scope);

        /// Returns the length of the pedigree.
        inline int length() const;

        /**
         * @brief Const array indexing operator.
         *
         *   <tt>(*this)[k]</tt> returns the kth term in this pedigree.
         *   <tt>(*this)[0]</tt> is the first / root term.
         *
         * @param   k  Index into the pedigree.
         * @return     Term @c k in this pedigree.
         * @pre        0 <= @c k < @c this->length()
         */
        inline const uint64_t& operator[] (int k) const;

        /**
         * @brief Returns number of terms in common prefix of @c this pedigree and @c b.
         */
        int common_prefix_length(const opt_pedigree<STATIC_PED_LENGTH>& b) const;
                
        /**
         * @brief Comparator function for pedigrees.
         *
         * Pedigrees are compared lexicographically in order, with the
         * root term being most significant.
         *
         * Thus, we have
         *   <tt>[1, 0] <  [1, 0, 0]  <  [1, 0, 4]  < [2]</tt> .
         *
         * @return  < 0  for <tt> *this  < b </tt>
         * @return    0  for <tt> *this == b </tt>
         * @return  > 0  for <tt> *this  > b </tt>
         *
         * @todo It might be nice to allow to templatize the
         *  comparison methods of cilkpub::opt_pedigree objects to
         *  allow comparisons between objects with different
         *  values of @c STATIC_PED_LENGTH. 
         */
        int compare(const opt_pedigree<STATIC_PED_LENGTH>& b) const;

        // Comparison.
        
        // Template argument is STATIC_PED_L to avoid a naming with the template argument
        // on this class.  Really, I think we only care that they are the same?
        template <int STATIC_PED_L>
        friend bool operator< (const opt_pedigree<STATIC_PED_L>& a,
                               const opt_pedigree<STATIC_PED_L>& b);  ///<  @c a <  @c b.

        template <int STATIC_PED_L>
        friend bool operator<=(const opt_pedigree<STATIC_PED_L>& a,
                               const opt_pedigree<STATIC_PED_L>& b);  ///<  @c a <= @c b.

        template <int STATIC_PED_L>
        friend bool operator> (const opt_pedigree<STATIC_PED_L>& a,
                               const opt_pedigree<STATIC_PED_L>& b);  ///<  @c a >  @c b.

        template <int STATIC_PED_L>
        friend bool operator>=(const opt_pedigree<STATIC_PED_L>& a,
                               const opt_pedigree<STATIC_PED_L>& b);  ///<  @c a >= @c b.

        // Equality tests
        template <int STATIC_PED_L>
        friend bool operator==(const opt_pedigree<STATIC_PED_L>& a,
                               const opt_pedigree<STATIC_PED_L>& b);  ///<  @c a == @c b.

        template <int STATIC_PED_L>
        friend bool operator!=(const opt_pedigree<STATIC_PED_L>& a,
                               const opt_pedigree<STATIC_PED_L>& b);  ///<  @c a != @c b.

        /// Returns true if this pedigree is a prefix of "ped".
        bool is_prefix_of(const opt_pedigree<STATIC_PED_LENGTH>& ped) const;

        /**
         * @brief Returns true if this pedigree is in the scope of @c ped.
         * 
         * This pedigree is in the scope of @c ped if either:
         *   1. @c ped is a prefix of @c this, or
         *   2. All but the last term of @c ped is a prefix of
         *      @c this, and the last term of @c ped is less than or equal
         *      corresponding term in @c ped.
         * For example, any pedigree  <tt>[1, 2, 4+k, ...]</tt> is in the scope of
         *  <tt>[1, 2, 4]</tt> for any nonnegative @c k.
         */
        bool in_scope_of(const opt_pedigree<STATIC_PED_LENGTH>& ped) const;

        // @warning The iterator typedef's work only because we know
        // that the pedigrees are stored as arrays.

        /**
         * @brief Iterator for const access to terms, in forward order
         * (from root to leaf).
         *
         * We are using std::reverse_iterator since the underlying
         * implementation actually stores the array in reverse order.
         * Ideally, we would be hiding this implementation detail.
         */
        typedef std::reverse_iterator<const uint64_t*> const_iterator;

        /**
         * @brief Iterator for const access to terms, in reverse order
         * (from leaf to root).
         *
         * We can use a normal array pointer, since the underlying
         * implementation actually stores the array in reverse order.
         * Ideally, we would be hiding this implementation detail.
         */
        typedef const uint64_t*                        const_reverse_iterator;

        // Read the pedigree in forward order.
        const_iterator begin()  const;         ///< Begin forward iteration (const).
        const_iterator end()    const;         ///< End forward iteration (const).
        
        // Read the pedigree in reverse order.
        const_reverse_iterator rbegin() const; ///< Begin reverse iteration (const).
        const_reverse_iterator rend()   const; ///< End reverse iteration (const).


// TBD: Some day, if we want this pedigree object to be mutable, we can
//       expose these mutating iterators.  But I'll leave it out for now.
//         
//        /// Iterator for access to terms, in forward order (from root to leaf).
//        typedef std::reverse_iterator<uint64_t*>       iterator;

//        /// Iterator for access to terms, in reverse order (from leaf to root).
//        typedef uint64_t*                              reverse_iterator;
        
//        iterator       begin();                ///< Begin forward iteration.
//        iterator       end();                  ///< End forward iteration.
//        reverse_iterator       rbegin();       ///< Begin reverse iteration.
//        reverse_iterator       rend();         ///< End reverse iteration.

        
        /**
         * @brief Fill up an array with up to d terms of the current
         * reverse pedigree.
         *
         * @pre buffer[0:d] must be available for writing the pedigree.
         * @post This method stores as many terms of the reverse
         *  pedigree into the buffer as possible.
         *  - If the return value falls in the interval <tt>[0, d]</tt>, then all terms in the
         *    pedigree have been stored, and the return value is the
         *    length of the pedigree.  
         *  - If the return value is greater than @c d, then @c d terms of the
         *     pedigree have been filled in, but the current pedigree contains more than
         *     @c d terms.
         *
         * @param buffer   Pointer to buffer to store pedigree terms.
         * @param d        Size of buffer.
         * @return A value in <tt>[0, d]</tt>, if the pedigree has at most @c d terms.
         * @return A value > @c d if the pedigree has more than @c d terms.
         */
        static int get_current_reverse_pedigree(uint64_t* buffer, int d);

        /**
         * @brief Stores up to the first @c d terms of this pedigree into
         * the buffer, in order from root to leaf.
         *
         * @param buffer   Pointer to buffer to store pedigree terms.
         * @param d        Size of buffer.
         * @return         The number of terms stored.
         */
        int copy_to_array(uint64_t* buffer, int d) const;

        /**
         * @brief Stores up to the first @c d terms of this pedigree into
         * the buffer, in order of leaf to root.
         *
         * @param buffer   Pointer to buffer to store pedigree terms.
         * @param d        Size of buffer.
         * @return         The number of terms stored.
         */
        int copy_reverse_to_array(uint64_t* buffer, int d) const;

        /**
         * @brief Print pedigree to specified file, with prepended
         * header string.  FOR DEBUGGING ONLY.
         *
         * @warning This method is used for debugging only.  This
         * method may be eliminated in future versions.
         *
         * @param f       File to print to.
         * @param header  Message to prepend to pedigree. 
         */
        void fprint(FILE* f, const char* header) const;

    private:
        /// Number of terms in this pedigree.
        int m_length;

        /**
         * @brief Default buffer for pedigree terms.
         * Short pedigrees are stored in this buffer.
         * Longer pedigrees are stored on the heap.
         * The pedigree is always stored in a contiguous buffer.
         */
        uint64_t m_rev_ped[STATIC_PED_LENGTH];

        /**
         * @brief Pointer to the contiguous buffer for this pedigree.
         * This pointer is either &m_rev_ped[0], or a buffer allocated
         * on the heap.
         */
        uint64_t* m_rev_ped_full;

        /// Returns a pointer to m_rev_ped_full.
        uint64_t* get_rev_ped_array() const;

        /// Resets this pedigree to empty.
        void clear();

        /**
         * @brief Clears the current pedigree, and fills it with the
         * current pedigree (the pedigree of the strand calling this
         * function).
         *
         * @return The length of the current pedigree.
         */
        int get_current_pedigree();

        /**
         * @brief Same as @c get_current_pedigree(), except it scopes the current pedigree.
         */ 
        int get_current_scoped_pedigree(const opt_pedigree_scope<STATIC_PED_LENGTH>& scope);
    };
    

    /**
     * @brief An object that stores a pedigree scope.
     *
     * A pedigree scope is intuitively a pedigree prefix that "scopes"
     * a set of pedigrees.
     *
     * More precisely, a pedigree <tt>[x_0, x_1, ... x_d]</tt>
     * \b scopes all pedigrees <tt> [x_0, x_1, ..., x_d + k, y_0, y_1, ... y_n] </tt> for any
     *  nonnegative @c k, The scoped pedigree in this case is
     * <tt> [k, y_0, y_1, ... y_n] </tt>.
     *
     * A pedigree scope @c s also saves a <b>stop node</b>, the @c
     * __cilkrts_pedigree node that contains the last term of the
     * pedigree for the scope.  The stop node can be used to stop
     * walking the pedigree chain early when trying to get a scoped
     * pedigree.
     *
     * A pedigree scope @c s is considered <b>active</b> if its stop
     * node is still a valid pedigree node for the computation (i.e.,
     * it hasn't been popped yet).  More precisely, a scope @c s with
     * pedigree with pedigree <tt> [x_0, x_1, ... x_{d-1}, x_d] </tt>
     * is active if there exists at least any executing strand with
     * pedigree of the form <tt>[x_0, x_1, ... x_{d-1}, z]</tt> for
     * any @c z.
     *
     * It is valid to use a @c pedigree_scope object to scope the
     * current pedigree only if it is active. If we know the current
     * pedigree falls in the scope of @c s, we can lookup the current
     * scoped pedigree by only walking up the chain of pedigrees nodes
     * up to the stop node of @c s.
     *
     * @warning The implementation of scoped pedigrees currently uses
     * an undocumented interface about the lifetimes of pointers to @c
     * __cilkrts_pedigree nodes in the pedigree chain.  We could
     * implement scoped pedigrees using only the documented pedigree
     * interface, if we were willing to walk the entire pedigree list
     * to read in a scoped pedigree.  In general, use caution with
     * scoped pedigrees.  The safest way to use a pedigree scope
     * object is to get the current scope at the beginning of a
     * function and use it only within that function.  This pedigree
     * scope is active throughout this entire function.
     *
     * @note It does not seem obvious how to check whether the current
     * pedigree is out of scope without actually walking up and
     * reading the entire current pedigree.
     */
    template <int STATIC_PED_LENGTH>
    class opt_pedigree_scope {
    public:
        /// Creates an empty "global" scope (i.e., with an empty
        /// pedigree).
        opt_pedigree_scope();

        /// Copy constructor.
        opt_pedigree_scope(const opt_pedigree_scope<STATIC_PED_LENGTH>& other);

        /// Destructor.
        ~opt_pedigree_scope();

        /// Assignment operator.
        opt_pedigree_scope<STATIC_PED_LENGTH>& operator=(const opt_pedigree_scope<STATIC_PED_LENGTH>& other);
        
        /**
         * @brief Creates a pedigree scope corresponding to the current pedigree.
         * @post Sets the stop node for this scope to a copy of the
         * current leaf pedigree node.
         */
        static opt_pedigree_scope current();

        /**
         * @brief Returns true if @c this scope is within the scope of
         * @c other.
         *
         * @note This method does read in the entire current pedigree.
         * Thus, this method is useful primarily for debugging.
         */
        static bool current_is_in_scope(const opt_pedigree_scope& other);

        /**
         * @brief Returns the stop node for this scope.
         *
         * @warning The pointer in the stop node of this scope becomes
         * invalid when control exits this scope.  There does not seem
         * to be an obvious way to efficiently check that this pointer
         * is still valid.
         */
        inline __cilkrts_pedigree get_stop_node() const;

        /**
         * @brief Print this scope to @c f, prepending @c header.  FOR DEBUGGING ONLY. 
         */
        void fprint(FILE* f, const char* header) const;

    private:
        /// A scope is really just a pedigree.
        opt_pedigree<STATIC_PED_LENGTH> m_ped;

        /**
         * A copy of the pedigree node that contains the last term of
         * the pedigree for this scope.
         */
        __cilkrts_pedigree m_stop_node;
    };

    /**
     * @brief A default cilkpub::opt_pedigree object, with a fixed size.
     *
     * This type is defined for convenience, so that programmers that
     * do not care about the static space allocated for pedigree
     * objects can declare and manipulate pedigrees without specifying
     * template parameters.
     *
     * @see cilkpub::opt_pedigree
     */
    typedef opt_pedigree<DEFAULT_STATIC_PED_LENGTH> pedigree;

    /**
     * @brief The default cilkpub::opt_pedigree_scope object, with a fixed size.
     *
     * This type is defined for convenience, so that programmers that
     * don't care about the static space allocated for pedigree
     * objects can declare and manipulate pedigrees without specifying
     * template parameters.
     *
     * @see cilkpub::opt_pedigree_scope
     */
    typedef opt_pedigree_scope<DEFAULT_STATIC_PED_LENGTH> pedigree_scope;
};  // namespace cilkpub

#include "internal/opt_pedigree.h"
#include "internal/pedigree_scope.h"

#endif // !defined(CILKPUB_PEDIGREES)
