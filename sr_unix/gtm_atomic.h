/****************************************************************
 *								*
 * Copyright (c) 2024 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef GTM_ATOMIC_H_INCLUDED
#define GTM_ATOMIC_H_INCLUDED
#include <stddef.h>
#include <stdbool.h>
/* USAGE
 * This header provides a standard, C11-like interface to atomics for use in GT.M.
 * This includes atomic types, generally with names which are prefixed with 'gtm_' but otherwise identical to the c11 ones provided
 * in any stdatomic.h, along with macros for all the major atomic operations. These operations have the same interface across
 * all supported platforms, but may have different performance profiles due to architectural differences. The atomic operations are
 * generally of the form ATOMIC_$OP, where $OP is the operation to perform. The following operations are supported:
 *
 * 	ATOMIC_LOAD(PTR, ORD);
 * 	@returns value of *PTR
 * 	@param PTR - pointer to a gtm_atomic object
 * 	@param ORD - memory ordering constraint
 *
 * 	ATOMIC_STORE(PTR, VAL, ORD)
 * 	@returns void
 * 	@param PTR - pointer to a gtm_atomic object
 * 	@param VAL - value to store in the pointed-to object
 * 	@param ORD - memory ordering constraint
 *
 * 	ATOMIC_FETCH_{ADD,SUB,XOR,OR,AND}(PTR, VAL, ORD)
 * 	ATOMIC_EXCHANGE(PTR, VAL, ORD) (instead of performing an operation with VAL, this sets *PTR to VAL)
 * 	@returns value of *PTR just before the operation atomically took place
 * 	@param PTR - pointer to a gtm_atomic object
 * 	@param VAL - value to use in the operation on the pointed-to object (eg for ATOMIC_FETCH_ADD, this is the increment value)
 * 	@param ORD - memory ordering constraint
 *
 * 	ATOMIC_COMPARE_EXCHANGE_{STRONG,WEAK}(OBJ_PTR, EXP_PTR, VAL, SUCCESS_ORD, FAIL_ORD)
 * 	@returns bool - true if the compare-and-swap succeeded, false if it failed
 * 	@param OBJ_PTR - pointer to the gtm_atomic object on which to perform the compare-and-swap
 * 	@param EXP_PTR - pointer to the expected value. If *OBJ_PTR matches *EXP_PTR, it will atomically be set
 * 	to equal VAL. If it does not equal *EXP_PTR, !!!(*EXP_PTR) is set to equal the actual value of *OBJ_PTR!!!
 * 	@param VAL - value to set the object to if the comparison succeeds
 * 	@param SUCCESS_ORD - memory ordering constraint to adopt if the comparison succeeds
 * 	@param FAIL_ORD - memory ordering constraint to adopt if the comparison fails
 *
 * So far this has made no substantive comment on the memory orderings. The best source on them is the C or C++ standards, which
 * are broadly compatible on this subject. But some comment here on how and when to use them is necessary. The possible memory
 * orders are as follows:
 * 	memory_order_relaxed
 * 	memory_order_acquire
 * 	memory_order_release
 * 	memory_order_acq_rel
 * 	memory_order_seq_cst
 *
 * This implementation follows the major compilers in not implementing memory_order_consume, and GT.M developers should not use it.
 * The memory orders impose constraints on whether and how aggressively the CPU and/or compiler may do things which undermine a
 * sequential order of operations across all cores. However, such a consistency with respect to a single atomic variable is
 * guaranteed even in the case of a memory_order_relaxed operation. That is: if you have a reader reading an atomic variable
 * and a writer incrementing it atomically, memory_order_relaxed is sufficient to guarantee that the reader never sees descending
 * values (except for integer overflow). However memory_order_relaxed permits reordering of surrounding loads and stores around
 * the atomic operations in general, so atomicity itself only guarantees that operations on a single variable are serializeable,
 * not that operations on multiple are.
 *
 * memory_order_acquire and memory_order_release can be understood as the corresponding operations on a mutex (acquire and release)
 * from which they take their names. Memory_order_acquire can only apply to a read, and memory_order_release can only apply to a
 * write. An acquire in one thread synchronizes with a release in another thread to guarantee that if the acquire load observes
 * the state of the variable created by the release store or a later state then all side effects which were visible to the writer
 * at the time of the release will be visible to the reader at the time of the acquire. This can create a 'critical section',
 * since it guarantees essentially that the operations which took place before the releaser releases will be contained within
 * the critical section and visible to the next acquirer who reads the released value. memory_order_acq_rel is a combination
 * constraint that makes sense only for atomic read-modify-write operations, like compare_and_swap or fetch_and_add
 *
 * memory_order_seq_cst is a little trickier to understand, but is more costly: developers should not default to this due to
 * uncertainty. In addition to the guarantee provided by acquire/release orderings, memory_order_seq_cst guarantees a single
 * total order of modifications that contain this constraint, even across atomic objects, and that this order will be
 * observed by any loads which carry this constraint. Developers should consult the C and C++ standards for documentation.
 *
 * PITFALLS AND CAUTIONS
 * 	- Developers should not declare gtm_atomic variables as volatile, as every ATOMIC_* access is converted to volatile
 * 	by this code, either by a cast or function argument qualifier. Every access to the target gtm_atomic_* object through
 * 	these interfaces will be a volatile one, and developers are encouraged to use the ATOMIC_* macros for every access to
 * 	a so-declared object for this and other reasons.
 * 	- Accesses to gtm_atomic_* objects which do not take place through the ATOMIC_* macros are subject to different
 * 	semantics on different platforms. Where this header imports C11 atomics, for example, a ++gtm_atomic_var will compile
 * 	to an atomic increment, whereas on other platforms this will compile to a non-atomic increment. Developers should always
 * 	use the ATOMIC_* macros for any access of a gtm_atomic_* variable.
 * 	- Available performance data supports the contention that the ATOMIC_LOAD and ATOMIC_STORE macros are cost-free compared
 * 	to ordinary volatile loads and stores when given a memory_order_relaxed constraint.
 */
#ifdef __STDC_VERSION__
#	if (__STDC_VERSION__ >= 201112L) && !defined(__STDC_NO_ATOMICS__)
#		ifdef __has_include
#			if __has_include(<stdatomic.h>)
#				define GTM_USES_C11_ATOMICS
#			endif
#		endif
#	endif
#endif
#ifdef GTM_USES_C11_ATOMICS
#include <stdatomic.h>
/* gtm_atomic types below, which generally map to atomic_* counterparts from stdatomic.h. */
typedef atomic_bool gtm_atomic_bool;
typedef atomic_char gtm_atomic_char;
typedef atomic_schar gtm_atomic_schar;
typedef atomic_uchar gtm_atomic_uchar;
typedef atomic_short gtm_atomic_short;
typedef atomic_ushort gtm_atomic_ushort;
typedef atomic_int gtm_atomic_int;
typedef atomic_uint gtm_atomic_uint;
typedef atomic_long gtm_atomic_long;
typedef atomic_ulong gtm_atomic_ulong;
typedef atomic_llong gtm_atomic_llong;
typedef atomic_ullong gtm_atomic_ullong;
typedef atomic_char16_t gtm_atomic_char16_t;
typedef atomic_char32_t gtm_atomic_char32_t;
typedef atomic_wchar_t gtm_atomic_wchar_t;
typedef atomic_int_least8_t gtm_atomic_int_least8_t;
typedef atomic_uint_least8_t gtm_atomic_uint_least8_t;
typedef atomic_int_least16_t gtm_atomic_int_least16_t;
typedef atomic_uint_least16_t gtm_atomic_uint_least16_t;
typedef atomic_int_least32_t gtm_atomic_int_least32_t;
typedef atomic_uint_least32_t gtm_atomic_uint_least32_t;
typedef atomic_int_least64_t gtm_atomic_int_least64_t;
typedef atomic_uint_least64_t gtm_atomic_uint_least64_t;
typedef atomic_int_fast8_t gtm_atomic_int_fast8_t;
typedef atomic_uint_fast8_t gtm_atomic_uint_fast8_t;
typedef atomic_int_fast16_t gtm_atomic_int_fast16_t;
typedef atomic_uint_fast16_t gtm_atomic_uint_fast16_t;
typedef atomic_int_fast32_t gtm_atomic_int_fast32_t;
typedef atomic_uint_fast32_t gtm_atomic_uint_fast32_t;
typedef atomic_int_fast64_t gtm_atomic_int_fast64_t;
typedef atomic_uint_fast64_t gtm_atomic_uint_fast64_t;
typedef atomic_intptr_t gtm_atomic_intptr_t;
typedef atomic_uintptr_t gtm_atomic_uintptr_t;
typedef atomic_size_t gtm_atomic_size_t;
typedef atomic_ptrdiff_t gtm_atomic_ptrdiff_t;
typedef atomic_intmax_t gtm_atomic_intmax_t;
typedef atomic_uintmax_t gtm_atomic_uintmax_t;
#define DEFINE_ATOMIC_OP(...)
#define DEFINE_ALL_ATOMIC_OPS(TYPE)
/* Operations for use with pointers to gtm_atomic types; most of the standard C11 operations are covered. */
#define COMPILER_FENCE(ORD) atomic_signal_fence(ORD)
#define ATOMIC_FETCH_ADD(OBJ_PTR, VAL, ORD) atomic_fetch_add_explicit((volatile __typeof__(OBJ_PTR))OBJ_PTR, VAL, ORD)
#define ATOMIC_FETCH_SUB(OBJ_PTR, VAL, ORD) atomic_fetch_sub_explicit((volatile __typeof__(OBJ_PTR))OBJ_PTR, VAL, ORD)
#define ATOMIC_COMPARE_EXCHANGE_WEAK(OBJ_PTR, EXP_PTR, VAL, SUCCESS_ORD, FAIL_ORD) \
	atomic_compare_exchange_weak_explicit((volatile __typeof__(OBJ_PTR))OBJ_PTR, EXP_PTR, VAL, SUCCESS_ORD, FAIL_ORD)
#define ATOMIC_COMPARE_EXCHANGE_STRONG(OBJ_PTR, EXP_PTR, VAL, SUCCESS_ORD, FAIL_ORD) \
	atomic_compare_exchange_strong_explicit((volatile __typeof__(OBJ_PTR))OBJ_PTR, EXP_PTR, VAL, SUCCESS_ORD, FAIL_ORD)
#define ATOMIC_EXCHANGE(OBJ_PTR, VAL, ORD) atomic_exchange_explicit((volatile __typeof__(OBJ_PTR))OBJ_PTR, VAL, ORD)
#define ATOMIC_FETCH_OR(OBJ_PTR, VAL, ORD) atomic_fetch_or_explicit((volatile __typeof__(OBJ_PTR))OBJ_PTR, VAL, ORD)
#define ATOMIC_FETCH_XOR(OBJ_PTR, VAL, ORD) atomic_fetch_xor_explicit((volatile __typeof__(OBJ_PTR))OBJ_PTR, VAL, ORD)
#define ATOMIC_FETCH_AND(OBJ_PTR, VAL, ORD) atomic_fetch_and_explicit((volatile __typeof__(OBJ_PTR))OBJ_PTR, VAL, ORD)
#define ATOMIC_LOAD(OBJ_PTR, ORD) atomic_load_explicit((volatile __typeof__(OBJ_PTR))OBJ_PTR, ORD)
#define ATOMIC_STORE(OBJ_PTR, VAL, ORD) atomic_store_explicit((volatile __typeof__(OBJ_PTR))OBJ_PTR, VAL, ORD)
#else /* GTM_USES_C11_ATOMICS undefined */
#include <uchar.h>
#include <wchar.h>
#include <stdint.h>
#include <limits.h>
#include <inttypes.h>
/* On platforms where C11 atomics are not supported, the gtm_atomic types map to ordinary non-volatile equivalents.
 * Each access through the ATOMIC_* operations is guaranteed to be volatile, though, and developers should always use the ATOMIC_
 * prefixed operations for anything involving such a variable.
 */
typedef _Bool gtm_atomic_bool;
typedef char gtm_atomic_char;
typedef signed char gtm_atomic_schar;
typedef unsigned char gtm_atomic_uchar;
typedef short gtm_atomic_short;
typedef unsigned short gtm_atomic_ushort;
typedef int gtm_atomic_int;
typedef unsigned int gtm_atomic_uint;
typedef long gtm_atomic_long;
typedef unsigned long gtm_atomic_ulong;
typedef long long gtm_atomic_llong;
typedef unsigned long long gtm_atomic_ullong;
typedef char16_t gtm_atomic_char16_t;
typedef char32_t gtm_atomic_char32_t;
typedef wchar_t gtm_atomic_wchar_t;
typedef int_least8_t gtm_atomic_int_least8_t;
typedef uint_least8_t gtm_atomic_uint_least8_t;
typedef int_least16_t gtm_atomic_int_least16_t;
typedef uint_least16_t gtm_atomic_uint_least16_t;
typedef int_least32_t gtm_atomic_int_least32_t;
typedef uint_least32_t gtm_atomic_uint_least32_t;
typedef int_least64_t gtm_atomic_int_least64_t;
typedef uint_least64_t gtm_atomic_uint_least64_t;
typedef int_fast8_t gtm_atomic_int_fast8_t;
typedef uint_fast8_t gtm_atomic_uint_fast8_t;
typedef int_fast16_t gtm_atomic_int_fast16_t;
typedef uint_fast16_t gtm_atomic_uint_fast16_t;
typedef int_fast32_t gtm_atomic_int_fast32_t;
typedef uint_fast32_t gtm_atomic_uint_fast32_t;
typedef int_fast64_t gtm_atomic_int_fast64_t;
typedef uint_fast64_t gtm_atomic_uint_fast64_t;
typedef intptr_t gtm_atomic_intptr_t;
typedef uintptr_t gtm_atomic_uintptr_t;
typedef size_t gtm_atomic_size_t;
typedef ptrdiff_t gtm_atomic_ptrdiff_t;
typedef intmax_t gtm_atomic_intmax_t;
typedef uintmax_t gtm_atomic_uintmax_t;
#if defined(__linux__) || defined(__open_xl__)
enum gtm_memory_order {
	memory_order_relaxed = __ATOMIC_RELAXED,
	memory_order_consume = __ATOMIC_CONSUME,
	memory_order_acquire = __ATOMIC_ACQUIRE,
	memory_order_release = __ATOMIC_RELEASE,
	memory_order_acq_rel = __ATOMIC_ACQ_REL,
	memory_order_seq_cst = __ATOMIC_SEQ_CST
};
#define DEFINE_ATOMIC_OP(...)
#define DEFINE_ALL_ATOMIC_OPS(TYPE)

#define COMPILER_FENCE(ORD) __atomic_signal_fence(ORD)
#define ATOMIC_FETCH_ADD(OBJ_PTR, VAL, ORD) __atomic_fetch_add((volatile __typeof__(OBJ_PTR))OBJ_PTR, VAL, ORD)
#define ATOMIC_FETCH_SUB(OBJ_PTR, VAL, ORD) __atomic_fetch_sub((volatile __typeof__(OBJ_PTR))OBJ_PTR, VAL, ORD)
#define ATOMIC_COMPARE_EXCHANGE_WEAK(OBJ_PTR, EXP_PTR, VAL, SUCCESS_ORD, FAIL_ORD) \
	__atomic_compare_exchange_n((volatile __typeof__(OBJ_PTR))OBJ_PTR, EXP_PTR, VAL, true, SUCCESS_ORD, FAIL_ORD)
#define ATOMIC_COMPARE_EXCHANGE_STRONG(OBJ_PTR, EXP_PTR, VAL, SUCCESS_ORD, FAIL_ORD) \
	__atomic_compare_exchange_n((volatile __typeof__(OBJ_PTR))OBJ_PTR, EXP_PTR, VAL, false, SUCCESS_ORD, FAIL_ORD)
#define ATOMIC_EXCHANGE(OBJ_PTR, VAL, ORD) __atomic_exchange_n((volatile __typeof__(OBJ_PTR))OBJ_PTR, VAL, ORD)
#define ATOMIC_FETCH_OR(OBJ_PTR, VAL, ORD) __atomic_fetch_or((volatile __typeof__(OBJ_PTR))OBJ_PTR, VAL, ORD)
#define ATOMIC_FETCH_XOR(OBJ_PTR, VAL, ORD) __atomic_fetch_xor((volatile __typeof__(OBJ_PTR))OBJ_PTR, VAL, ORD)
#define ATOMIC_FETCH_AND(OBJ_PTR, VAL, ORD) __atomic_fetch_and((volatile __typeof__(OBJ_PTR))OBJ_PTR, VAL, ORD)
#define ATOMIC_LOAD(OBJ_PTR, ORD) __atomic_load_n((volatile __typeof__(OBJ_PTR))OBJ_PTR, ORD)
#define ATOMIC_STORE(OBJ_PTR, VAL, ORD) __atomic_store_n((volatile __typeof__(OBJ_PTR))OBJ_PTR, VAL, ORD)
#elif defined (_AIX)
/* All code past this point is aix-specific, and makes extensive use of inline assembly.
 * Since, as of this writing, inline assembly is uncommon in the GT.M codebase, its use deserves some explanation and justification.
 * The ground truth source for all information on inline assembly syntax is the xlc user manual's page on the inline assembly
 * extension.
 *
 * The reason for using assembly at all is that there are specific necessary constructions that would be optimized out
 * by any C compiler but provide crucial ordering guarantees. The most important of these is the branch-isync construction,
 * where a branch (often of a condition that is guaranteed true) is followed by an isync; the branch makes sure that whatever
 * operations the branch depends on has completed prior to the isync and the isync discards all prefetched instructions. It's
 * impossible to write C code that can be guaranteed to compile down to this sequence. As for why the assembly is inline rather
 * than in its own files, the justification is performance: by allowing the compiler to choose the best registers for each
 * operand, this code avoids forcing use of one or more registers which have hot variables.
 *
 * Most of the basics necessary to understand and/or edit this assembly can be understood from the xlc documentation mentioned
 * above, but a few words here on some particular constructions used by this code:
 * 	- Powerpc in some cases expects an address in a register as an operand (such as for the load-linked/store-conditional
 * 	lwarx/stwcx. pair) but in other cases expects an offsettable memory address (also in a register but handled differently
 * 	in specifying it in the inline assembly), such as for a bare load and/or store. Whenever able, it is good to specify
 * 	the memory operand "m" as the output and/or input operand. Why? One important purpose of operand specification is to
 * 	tell the compiler which registers and/or memory locations will be changed by the assembly. If "+m" (*OBJ_PTR) is given,
 * 	then the compiler will know that the location of *OBJ_PTR is read and modified by the assembly, and any registers which
 * 	had cached that value will need to be refreshed. When it is impossible to use the memory operand directly (such as in
 * 	the load-linked/store-conditional cases) this code still passes the memory location written to and/or read from in this
 * 	way, even if the named operand is not actually invoked in the assembly, so as to inform the compiler that this location
 * 	is affected without needing to include a "memory" clobber.
 * 	- Generally this code has avoided passing a "memory" clobber on the basis of the effectiveness of what was described
 * 	above and the view that any access which is genuinely volatile will be made as such by the surrounding code. The volatile
 * 	specifier of the asm string itself guarantees no compiler reordering around the implicit fences at the beginning and end
 * 	of the assembly, but does not, for example, universally invalidate all pointer values held in registers as if the assembly
 * 	string could have written to any and every variable on the stack or in the heap. This decision rests on the justified
 * 	premise that the compiler will not ignore the clobber implications of an unused memory operand, which has been validated
 * 	by extensive testing. But the view should be re-examined if atomics testing starts to turn up failures.
 */
/* Internally we depend on the following invariants to ensure we're using the correct precsision of loads/stores and sign-extending
 * where necessary.
 */
#define GTM_ATOMIC_BOOL_WIDTH 1
#define GTM_ATOMIC_CHAR_WIDTH 1
#define GTM_ATOMIC_SCHAR_WIDTH 1
#define GTM_ATOMIC_UCHAR_WIDTH 1
#define GTM_ATOMIC_SHORT_WIDTH 2
#define GTM_ATOMIC_USHORT_WIDTH 2
#define GTM_ATOMIC_INT_WIDTH 4
#define GTM_ATOMIC_UINT_WIDTH 4
#define GTM_ATOMIC_LONG_WIDTH 8
#define GTM_ATOMIC_ULONG_WIDTH 8
#define GTM_ATOMIC_LLONG_WIDTH 8
#define GTM_ATOMIC_ULLONG_WIDTH 8
_Static_assert(GTM_ATOMIC_BOOL_WIDTH == sizeof(gtm_atomic_bool),
		"Preprocessor size of gtm_atomic_bool incorrect");
_Static_assert(GTM_ATOMIC_CHAR_WIDTH == sizeof(gtm_atomic_char),
		"Preprocessor size of gtm_atomic_char incorrect");
_Static_assert(GTM_ATOMIC_SCHAR_WIDTH == sizeof(gtm_atomic_schar),
		"Preprocessor size of gtm_atomic_schar incorrect");
_Static_assert(GTM_ATOMIC_UCHAR_WIDTH == sizeof(gtm_atomic_uchar),
		"Preprocessor size of gtm_atomic_uchar incorrect");
_Static_assert(GTM_ATOMIC_SHORT_WIDTH == sizeof(gtm_atomic_short),
		"Preprocessor size of gtm_atomic_short incorrect");
_Static_assert(GTM_ATOMIC_USHORT_WIDTH == sizeof(gtm_atomic_ushort),
		"Preprocessor size of gtm_atomic_ushort incorrect");
_Static_assert(GTM_ATOMIC_INT_WIDTH == sizeof(gtm_atomic_int),
		"Preprocessor size of gtm_atomic_int incorrect");
_Static_assert(GTM_ATOMIC_UINT_WIDTH == sizeof(gtm_atomic_uint),
		"Preprocessor size of gtm_atomic_uint incorrect");
_Static_assert(GTM_ATOMIC_LONG_WIDTH == sizeof(gtm_atomic_long),
		"Preprocessor size of gtm_atomic_long incorrect");
_Static_assert(GTM_ATOMIC_ULONG_WIDTH == sizeof(gtm_atomic_ulong),
		"Preprocessor size of gtm_atomic_ulong incorrect");
_Static_assert(GTM_ATOMIC_LLONG_WIDTH == sizeof(gtm_atomic_llong),
		"Preprocessor size of gtm_atomic_llong incorrect");
_Static_assert(GTM_ATOMIC_ULLONG_WIDTH == sizeof(gtm_atomic_ullong),
		"Preprocessor size of gtm_atomic_ullong incorrect");
_Static_assert(sizeof(gtm_atomic_short) == sizeof(gtm_atomic_char16_t),
		"Preprocessor size of gtm_atomic_char16_t incorrect");
_Static_assert(sizeof(gtm_atomic_int) == sizeof(gtm_atomic_char32_t),
		"Preprocessor size of gtm_atomic_char32_t incorrect");
_Static_assert(sizeof(gtm_atomic_int) == sizeof(gtm_atomic_wchar_t),
		"Preprocessor size of gtm_atomic_wchar_t incorrect");
_Static_assert(sizeof(gtm_atomic_schar) == sizeof(gtm_atomic_int_least8_t),
		"Preprocessor size of gtm_atomic_int_least8_t incorrect");
_Static_assert(sizeof(gtm_atomic_uchar) == sizeof(gtm_atomic_uint_least8_t),
		"Preprocessor size of gtm_atomic_uint_least8_t incorrect");
_Static_assert(sizeof(gtm_atomic_short) == sizeof(gtm_atomic_int_least16_t),
		"Preprocessor size of gtm_atomic_int_least16_t incorrect");
_Static_assert(sizeof(gtm_atomic_ushort) == sizeof(gtm_atomic_uint_least16_t),
		"Preprocessor size of gtm_atomic_uint_least16_t incorrect");
_Static_assert(sizeof(gtm_atomic_int) == sizeof(gtm_atomic_int_least32_t),
		"Preprocessor size of gtm_atomic_int_least32_t incorrect");
_Static_assert(sizeof(gtm_atomic_uint) == sizeof(gtm_atomic_uint_least32_t),
		"Preprocessor size of gtm_atomic_uint_least32_t incorrect");
_Static_assert(sizeof(gtm_atomic_long) == sizeof(gtm_atomic_int_least64_t),
		"Preprocessor size of gtm_atomic_int_least64_t incorrect");
_Static_assert(sizeof(gtm_atomic_ulong) == sizeof(gtm_atomic_uint_least64_t),
		"Preprocessor size of gtm_atomic_uint_least64_t incorrect");
_Static_assert(sizeof(gtm_atomic_schar) == sizeof(gtm_atomic_int_fast8_t),
		"Preprocessor size of gtm_atomic_int_fast8_t incorrect");
_Static_assert(sizeof(gtm_atomic_uchar) == sizeof(gtm_atomic_uint_fast8_t),
		"Preprocessor size of gtm_atomic_uint_fast8_t incorrect");
_Static_assert(sizeof(gtm_atomic_short) == sizeof(gtm_atomic_int_fast16_t),
		"Preprocessor size of gtm_atomic_int_fast16_t incorrect");
_Static_assert(sizeof(gtm_atomic_ushort) == sizeof(gtm_atomic_uint_fast16_t),
		"Preprocessor size of gtm_atomic_uint_fast16_t incorrect");
_Static_assert(sizeof(gtm_atomic_int) == sizeof(gtm_atomic_int_fast32_t),
		"Preprocessor size of gtm_atomic_int_fast32_t incorrect");
_Static_assert(sizeof(gtm_atomic_uint) == sizeof(gtm_atomic_uint_fast32_t),
		"Preprocessor size of gtm_atomic_uint_fast32_t incorrect");
_Static_assert(sizeof(gtm_atomic_long) == sizeof(gtm_atomic_int_fast64_t),
		"Preprocessor size of gtm_atomic_int_fast64_t incorrect");
_Static_assert(sizeof(gtm_atomic_ulong) == sizeof(gtm_atomic_uint_fast64_t),
		"Preprocessor size of gtm_atomic_uint_fast64_t incorrect");
_Static_assert(sizeof(gtm_atomic_long) == sizeof(gtm_atomic_intptr_t),
		"Preprocessor size of gtm_atomic_intptr_t incorrect");
_Static_assert(sizeof(gtm_atomic_ulong) == sizeof(gtm_atomic_uintptr_t),
		"Preprocessor size of gtm_atomic_uintptr_t incorrect");
_Static_assert(sizeof(gtm_atomic_ulong) == sizeof(gtm_atomic_size_t),
		"Preprocessor size of gtm_atomic_size_t incorrect");
_Static_assert(sizeof(gtm_atomic_long) == sizeof(gtm_atomic_ptrdiff_t),
		"Preprocessor size of gtm_atomic_ptrdiff_t incorrect");
_Static_assert(sizeof(gtm_atomic_long) == sizeof(gtm_atomic_intmax_t),
		"Preprocessor size of gtm_atomic_intmax_t incorrect");
_Static_assert(sizeof(gtm_atomic_ulong) == sizeof(gtm_atomic_uintmax_t),
		"Preprocessor size of gtm_atomic_uintmax_t incorrect");
/* The following macros provide the name of the atomic function to call, given the operation, type, and memory order. */
#ifdef _ARCH_PWR8
#define FOP_TYPE_ORD_MNEMONIC(OP, OBJ_PTR, ORD) _Generic(((__typeof__(OBJ_PTR)){ 0 }),	\
		gtm_atomic_bool *: OP##__gtm_atomic_bool__##ORD,			\
		gtm_atomic_char *: OP##__gtm_atomic_char__##ORD,			\
		gtm_atomic_schar *: OP##__gtm_atomic_schar__##ORD,			\
		gtm_atomic_uchar *: OP##__gtm_atomic_uchar__##ORD,			\
		gtm_atomic_short *: OP##__gtm_atomic_short__##ORD,			\
		gtm_atomic_ushort *: OP##__gtm_atomic_ushort__##ORD,			\
		gtm_atomic_int *: OP##__gtm_atomic_int__##ORD,				\
		gtm_atomic_uint *: OP##__gtm_atomic_uint__##ORD,			\
		gtm_atomic_long *: OP##__gtm_atomic_long__##ORD,			\
		gtm_atomic_ulong *: OP##__gtm_atomic_ulong__##ORD,			\
		gtm_atomic_llong *: OP##__gtm_atomic_llong__##ORD,			\
		gtm_atomic_ullong *: OP##__gtm_atomic_ullong__##ORD)
#else
#define FOP_TYPE_ORD_MNEMONIC(OP, OBJ_PTR, ORD) _Generic(((__typeof__(OBJ_PTR)){ 0 }),	\
		gtm_atomic_int *: OP##__gtm_atomic_int__##ORD,				\
		gtm_atomic_uint *: OP##__gtm_atomic_uint__##ORD,			\
		gtm_atomic_long *: OP##__gtm_atomic_long__##ORD,			\
		gtm_atomic_ulong *: OP##__gtm_atomic_ulong__##ORD,			\
		gtm_atomic_llong *: OP##__gtm_atomic_llong__##ORD,			\
		gtm_atomic_ullong *: OP##__gtm_atomic_ullong__##ORD)
#endif
#define OP_TYPE_ORD_MNEMONIC(OP, OBJ_PTR, ORD) _Generic(((__typeof__(OBJ_PTR)){ 0 }),	\
		gtm_atomic_bool *: OP##__gtm_atomic_bool__##ORD,			\
		gtm_atomic_char *: OP##__gtm_atomic_char__##ORD,			\
		gtm_atomic_schar *: OP##__gtm_atomic_schar__##ORD,			\
		gtm_atomic_uchar *: OP##__gtm_atomic_uchar__##ORD,			\
		gtm_atomic_short *: OP##__gtm_atomic_short__##ORD,			\
		gtm_atomic_ushort *: OP##__gtm_atomic_ushort__##ORD,			\
		gtm_atomic_int *: OP##__gtm_atomic_int__##ORD,				\
		gtm_atomic_uint *: OP##__gtm_atomic_uint__##ORD,			\
		gtm_atomic_long *: OP##__gtm_atomic_long__##ORD,			\
		gtm_atomic_ulong *: OP##__gtm_atomic_ulong__##ORD,			\
		gtm_atomic_llong *: OP##__gtm_atomic_llong__##ORD,			\
		gtm_atomic_ullong *: OP##__gtm_atomic_ullong__##ORD)
/* Powerpc assembly for stores, for each supported type-width. */
#define PPC_ASM_ST__gtm_atomic_char__(OBJ_PTR, VAL, PRE_BARRIER, POST_BARRIER)		\
	asm volatile (	"0:"PRE_BARRIER				\
			"\tstb %[value],%[obj]\n"		\
			POST_BARRIER				\
			: [obj]		"=m" 	(*OBJ_PTR)	\
			: [value]	"r"	(VAL)		\
			:)
#define PPC_ASM_ST__gtm_atomic_short__(OBJ_PTR, VAL, PRE_BARRIER, POST_BARRIER)		\
	asm volatile (	"0:"PRE_BARRIER				\
			"\tsth %[value],%[obj]\n"		\
			POST_BARRIER				\
			: [obj]		"=m" 	(*OBJ_PTR)	\
			: [value]	"r"	(VAL)		\
			:)
#define PPC_ASM_ST__gtm_atomic_int__(OBJ_PTR, VAL, PRE_BARRIER, POST_BARRIER)		\
	asm volatile (	"0:"PRE_BARRIER				\
			"\tstw %[value],%[obj]\n"		\
			POST_BARRIER				\
			: [obj]		"=m" 	(*OBJ_PTR)	\
			: [value]	"r"	(VAL)		\
			:)
#define PPC_ASM_ST__gtm_atomic_long__(OBJ_PTR, VAL, PRE_BARRIER, POST_BARRIER)		\
	asm volatile (	"0:"PRE_BARRIER				\
			"\tstd %[value],%[obj]\n"		\
			POST_BARRIER				\
			: [obj]		"=m" 	(*OBJ_PTR)	\
			: [value]	"r"	(VAL)		\
			:)
#define PPC_ASM_ST__gtm_atomic_bool__ PPC_ASM_ST__gtm_atomic_char__
#define PPC_ASM_ST__gtm_atomic_schar__ PPC_ASM_ST__gtm_atomic_char__
#define PPC_ASM_ST__gtm_atomic_uchar__ PPC_ASM_ST__gtm_atomic_char__
#define PPC_ASM_ST__gtm_atomic_ushort__ PPC_ASM_ST__gtm_atomic_short__
#define PPC_ASM_ST__gtm_atomic_uint__ PPC_ASM_ST__gtm_atomic_int__
#define PPC_ASM_ST__gtm_atomic_ulong__ PPC_ASM_ST__gtm_atomic_long__
#define PPC_ASM_ST__gtm_atomic_llong__ PPC_ASM_ST__gtm_atomic_long__
#define PPC_ASM_ST__gtm_atomic_ullong__ PPC_ASM_ST__gtm_atomic_long__
/* Powerpc assembly for loads, for each supported sign/type-width combination. */
#define PPC_ASM_LD__gtm_atomic_uchar__(OBJ_PTR, RET, PRE_BARRIER, POST_BARRIER) 		\
	asm volatile ( 	"0:"PRE_BARRIER				\
			"\tlbz %[retval],%[obj]\n"		\
			POST_BARRIER				\
			: [retval]	"=r"	(RET)		\
			: [obj]		"m"	(*OBJ_PTR)	\
			:)
#define PPC_ASM_LD__gtm_atomic_schar__(OBJ_PTR, RET, PRE_BARRIER, POST_BARRIER) 		\
	asm volatile ( 	"0:"PRE_BARRIER				\
			"\tlbz %[retval],%[obj]\n"		\
			"\textsb %[retval],%[retval]\n"		\
			POST_BARRIER				\
			: [retval]	"=r"	(RET)		\
			: [obj]		"m"	(*OBJ_PTR)	\
			:)
#define PPC_ASM_LD__gtm_atomic_ushort__(OBJ_PTR, RET, PRE_BARRIER, POST_BARRIER) 		\
	asm volatile ( 	"0:"PRE_BARRIER				\
			"\tlhz %[retval],%[obj]\n"		\
			POST_BARRIER				\
			: [retval]	"=r"	(RET)		\
			: [obj]		"m"	(*OBJ_PTR)	\
			:)
#define PPC_ASM_LD__gtm_atomic_short__(OBJ_PTR, RET, PRE_BARRIER, POST_BARRIER) 		\
	asm volatile ( 	"0:"PRE_BARRIER				\
			"\tlha %[retval],%[obj]\n"		\
			POST_BARRIER				\
			: [retval]	"=r"	(RET)		\
			: [obj]		"m"	(*OBJ_PTR)	\
			:)
#define PPC_ASM_LD__gtm_atomic_uint__(OBJ_PTR, RET, PRE_BARRIER, POST_BARRIER) 		\
	asm volatile ( 	"0:"PRE_BARRIER				\
			"\tlwz %[retval],%[obj]\n"		\
			POST_BARRIER				\
			: [retval]	"=r"	(RET)		\
			: [obj]		"m"	(*OBJ_PTR)	\
			:)
#define PPC_ASM_LD__gtm_atomic_int__(OBJ_PTR, RET, PRE_BARRIER, POST_BARRIER) 		\
	asm volatile ( 	"0:"PRE_BARRIER				\
			"\tlwa %[retval],%[obj]\n"		\
			POST_BARRIER				\
			: [retval]	"=r"	(RET)		\
			: [obj]		"m"	(*OBJ_PTR)	\
			:)
#define PPC_ASM_LD__gtm_atomic_long__(OBJ_PTR, RET, PRE_BARRIER, POST_BARRIER) 		\
	asm volatile ( 	"0:"PRE_BARRIER				\
			"\tld %[retval],%[obj]\n"		\
			POST_BARRIER				\
			: [retval]	"=r"	(RET)		\
			: [obj]		"m"	(*OBJ_PTR)	\
			:)
#ifdef __CHAR_SIGNED__
#define PPC_ASM_LD__gtm_atomic_char__ PPC_ASM_LD__gtm_atomic_schar__
#else
#define PPC_ASM_LD__gtm_atomic_char__ PPC_ASM_LD__gtm_atomic_uchar__
#endif
#define PPC_ASM_LD__gtm_atomic_bool__ PPC_ASM_LD__gtm_atomic_char__
#define PPC_ASM_LD__gtm_atomic_ulong__ PPC_ASM_LD__gtm_atomic_long__
#define PPC_ASM_LD__gtm_atomic_llong__ PPC_ASM_LD__gtm_atomic_long__
#define PPC_ASM_LD__gtm_atomic_ullong__ PPC_ASM_LD__gtm_atomic_long__
/* Powerpc assembly for loads with trailing branch-conditionals, for each supported sign/type-width combination. */
#define PPC_ASM_LDCMP__gtm_atomic_uchar__(OBJ_PTR, RET, PRE_BARRIER, POST_BARRIER) 	\
	asm volatile ( 	"0:"PRE_BARRIER				\
			"\tlbz %[retval],%[obj]\n"		\
			"\tcmp 0,0,%[retval],%[retval]\n"	\
			"\tbne- 0b\n"				\
			POST_BARRIER				\
			: [retval]	"=r"	(RET)		\
			: [obj]		"m"	(*OBJ_PTR)	\
			: "cr0")
#define PPC_ASM_LDCMP__gtm_atomic_schar__(OBJ_PTR, RET, PRE_BARRIER, POST_BARRIER) 	\
	asm volatile ( 	"0:"PRE_BARRIER				\
			"\tlbz %[retval],%[obj]\n"		\
			"\textsb %[retval],%[retval]\n"		\
			"\tcmp 0,0,%[retval],%[retval]\n"	\
			"\tbne- 0b\n"				\
			POST_BARRIER				\
			: [retval]	"=r"	(RET)		\
			: [obj]		"m"	(*OBJ_PTR)	\
			: "cr0")
#define PPC_ASM_LDCMP__gtm_atomic_ushort__(OBJ_PTR, RET, PRE_BARRIER, POST_BARRIER) 	\
	asm volatile ( 	"0:"PRE_BARRIER				\
			"\tlha %[retval],%[obj]\n"		\
			"\tcmp 0,0,%[retval],%[retval]\n"	\
			"\tbne- 0b\n"				\
			POST_BARRIER				\
			: [retval]	"=r"	(RET)		\
			: [obj]		"m"	(*OBJ_PTR)	\
			: "cr0")
#define PPC_ASM_LDCMP__gtm_atomic_short__(OBJ_PTR, RET, PRE_BARRIER, POST_BARRIER) 	\
	asm volatile ( 	"0:"PRE_BARRIER				\
			"\tlhz %[retval],%[obj]\n"		\
			"\tcmp 0,0,%[retval],%[retval]\n"	\
			"\tbne- 0b\n"				\
			POST_BARRIER				\
			: [retval]	"=r"	(RET)		\
			: [obj]		"m"	(*OBJ_PTR)	\
			: "cr0")
#define PPC_ASM_LDCMP__gtm_atomic_uint__(OBJ_PTR, RET, PRE_BARRIER, POST_BARRIER) 		\
	asm volatile ( 	"0:"PRE_BARRIER				\
			"\tlwz %[retval],%[obj]\n"		\
			"\tcmp 0,0,%[retval],%[retval]\n"	\
			"\tbne- 0b\n"				\
			POST_BARRIER				\
			: [retval]	"=r"	(RET)		\
			: [obj]		"m"	(*OBJ_PTR)	\
			: "cr0")
#define PPC_ASM_LDCMP__gtm_atomic_int__(OBJ_PTR, RET, PRE_BARRIER, POST_BARRIER) 		\
	asm volatile ( 	"0:"PRE_BARRIER				\
			"\tlwa %[retval],%[obj]\n"		\
			"\tcmp 0,0,%[retval],%[retval]\n"	\
			"\tbne- 0b\n"				\
			POST_BARRIER				\
			: [retval]	"=r"	(RET)		\
			: [obj]		"m"	(*OBJ_PTR)	\
			: "cr0")
#define PPC_ASM_LDCMP__gtm_atomic_long__(OBJ_PTR, RET, PRE_BARRIER, POST_BARRIER) 		\
	asm volatile ( 	"0:"PRE_BARRIER				\
			"\tld %[retval],%[obj]\n"		\
			"\tcmp 0,1,%[retval],%[retval]\n"	\
			"\tbne- 0b\n"				\
			POST_BARRIER				\
			: [retval]	"=r"	(RET)		\
			: [obj]		"m"	(*OBJ_PTR)	\
			: "cr0")
#define PPC_ASM_LDCMP__gtm_atomic_bool__ PPC_ASM_LDCMP__gtm_atomic_uchar__
#ifdef __CHAR_SIGNED__
#define PPC_ASM_LDCMP__gtm_atomic_char__ PPC_ASM_LDCMP__gtm_atomic_schar__
#else
#define PPC_ASM_LDCMP__gtm_atomic_char__ PPC_ASM_LDCMP__gtm_atomic_uchar__
#endif
#define PPC_ASM_LDCMP__gtm_atomic_ulong__ PPC_ASM_LDCMP__gtm_atomic_long__
#define PPC_ASM_LDCMP__gtm_atomic_llong__ PPC_ASM_LDCMP__gtm_atomic_long__
#define PPC_ASM_LDCMP__gtm_atomic_ullong__ PPC_ASM_LDCMP__gtm_atomic_long__
/* Load-linked/store-conditional with (ATOMIC_FETCH_*) and without (ATOMIC_EXCHANGE) intervening operation.
 * Sign- vs zero- extension handling necessary here because the FETCH_* op returns the value, which surrounding assembly may expect
 * will be sign-or-zero-extended.
 */
#ifdef _ARCH_PWR8
#define PPC_ASM_XCHG__gtm_atomic_uchar__(OBJ_PTR, VAL, RET, PRE_BARRIER, POST_BARRIER)		\
	asm volatile ( 	PRE_BARRIER						\
			"0:\tlbarx %[retval],0,%[obj_ptr]\n"			\
			"\tstbcx. %[val],0,%[obj_ptr]\n"			\
			"\tbne- 0b\n"						\
			POST_BARRIER						\
			: [retval]	"=&r"	(RET),				\
			  [obj]		"+m"	(*OBJ_PTR)			\
			: [val]		"r"	(VAL),				\
			  [obj_ptr]	"r"	(OBJ_PTR)			\
			: "cr0")
#define PPC_ASM_LLSC__gtm_atomic_uchar__(OBJ_PTR, VAL, RET, ASM_OP, PRE_BARRIER, POST_BARRIER)		\
	asm volatile ( 	PRE_BARRIER						\
			"0:\tlbarx %[retval],0,%[obj_ptr]\n"			\
			ASM_OP							\
			"\tstbcx. %[scratch],0,%[obj_ptr]\n"			\
			"\tbne- 0b\n"						\
			POST_BARRIER						\
			: [retval]	"=&r"	(RET),				\
			  [obj]		"+m"	(*OBJ_PTR),			\
			  [scratch]	"=&r"	(((__typeof__(VAL)){ 0 }))	\
			: [val]		"r"	(VAL),				\
			  [obj_ptr]	"r"	(OBJ_PTR)			\
			: "cr0")
#define PPC_ASM_XCHG__gtm_atomic_schar__(OBJ_PTR, VAL, RET, PRE_BARRIER, POST_BARRIER)		\
	asm volatile ( 	PRE_BARRIER						\
			"0:\tlbarx %[retval],0,%[obj_ptr]\n"			\
			"\tstbcx. %[val],0,%[obj_ptr]\n"			\
			"\tbne- 0b\n"						\
			"\textsb %[retval],%[retval]\n"				\
			POST_BARRIER						\
			: [retval]	"=&r"	(RET),				\
			  [obj]		"+m"	(*OBJ_PTR)			\
			: [val]		"r"	(VAL),				\
			  [obj_ptr]	"r"	(OBJ_PTR)			\
			: "cr0")
#define PPC_ASM_LLSC__gtm_atomic_schar__(OBJ_PTR, VAL, RET, ASM_OP, PRE_BARRIER, POST_BARRIER)		\
	asm volatile ( 	PRE_BARRIER						\
			"0:\tlbarx %[retval],0,%[obj_ptr]\n"			\
			"\textsb %[retval],%[retval]\n"				\
			ASM_OP							\
			"\tstbcx. %[scratch],0,%[obj_ptr]\n"			\
			"\tbne- 0b\n"						\
			POST_BARRIER						\
			: [retval]	"=&r"	(RET),				\
			  [obj]		"+m"	(*OBJ_PTR),			\
			  [scratch]	"=&r"	(((__typeof__(VAL)){ 0 }))	\
			: [val]		"r"	(VAL),				\
			  [obj_ptr]	"r"	(OBJ_PTR)			\
			: "cr0")
#define PPC_ASM_XCHG__gtm_atomic_ushort__(OBJ_PTR, VAL, RET, PRE_BARRIER, POST_BARRIER)		\
	asm volatile ( 	PRE_BARRIER						\
			"0:\tlharx %[retval],0,%[obj_ptr]\n"			\
			"\tsthcx. %[val],0,%[obj_ptr]\n"			\
			"\tbne- 0b\n"						\
			POST_BARRIER						\
			: [retval]	"=&r"	(RET),				\
			  [obj]		"+m"	(*OBJ_PTR)			\
			: [val]		"r"	(VAL),				\
			  [obj_ptr]	"r"	(OBJ_PTR)			\
			: "cr0")
#define PPC_ASM_LLSC__gtm_atomic_ushort__(OBJ_PTR, VAL, RET, ASM_OP, PRE_BARRIER, POST_BARRIER)		\
	asm volatile ( 	PRE_BARRIER						\
			"0:\tlharx %[retval],0,%[obj_ptr]\n"			\
			ASM_OP							\
			"\tsthcx. %[scratch],0,%[obj_ptr]\n"			\
			"\tbne- 0b\n"						\
			POST_BARRIER						\
			: [retval]	"=&r"	(RET),				\
			  [obj]		"+m"	(*OBJ_PTR),			\
			  [scratch]	"=&r"	(((__typeof__(VAL)){ 0 }))	\
			: [val]		"r"	(VAL),				\
			  [obj_ptr]	"r"	(OBJ_PTR)			\
			: "cr0")
#define PPC_ASM_XCHG__gtm_atomic_short__(OBJ_PTR, VAL, RET, PRE_BARRIER, POST_BARRIER)		\
	asm volatile ( 	PRE_BARRIER						\
			"0:\tlharx %[retval],0,%[obj_ptr]\n"			\
			"\tsthcx. %[val],0,%[obj_ptr]\n"			\
			"\tbne- 0b\n"						\
			"\textsh %[retval],%[retval]\n"				\
			POST_BARRIER						\
			: [retval]	"=&r"	(RET),				\
			  [obj]		"+m"	(*OBJ_PTR)			\
			: [val]		"r"	(VAL),				\
			  [obj_ptr]	"r"	(OBJ_PTR)			\
			: "cr0")
#define PPC_ASM_LLSC__gtm_atomic_short__(OBJ_PTR, VAL, RET, ASM_OP, PRE_BARRIER, POST_BARRIER)		\
	asm volatile ( 	PRE_BARRIER						\
			"0:\tlharx %[retval],0,%[obj_ptr]\n"			\
			"\textsh %[retval],%[retval]\n"				\
			ASM_OP							\
			"\tsthcx. %[scratch],0,%[obj_ptr]\n"			\
			"\tbne- 0b\n"						\
			POST_BARRIER						\
			: [retval]	"=&r"	(RET),				\
			  [obj]		"+m"	(*OBJ_PTR),			\
			  [scratch]	"=&r"	(((__typeof__(VAL)){ 0 }))	\
			: [val]		"r"	(VAL),				\
			  [obj_ptr]	"r"	(OBJ_PTR)			\
			: "cr0")
#endif
#define PPC_ASM_XCHG__gtm_atomic_uint__(OBJ_PTR, VAL, RET, PRE_BARRIER, POST_BARRIER)		\
	asm volatile ( 	PRE_BARRIER						\
			"0:\tlwarx %[retval],0,%[obj_ptr]\n"			\
			"\tstwcx. %[val],0,%[obj_ptr]\n"			\
			"\tbne- 0b\n"						\
			POST_BARRIER						\
			: [retval]	"=&r"	(RET),				\
			  [obj]		"+m"	(*OBJ_PTR)			\
			: [val]		"r"	(VAL),				\
			  [obj_ptr]	"r"	(OBJ_PTR)			\
			: "cr0")
#define PPC_ASM_LLSC__gtm_atomic_uint__(OBJ_PTR, VAL, RET, ASM_OP, PRE_BARRIER, POST_BARRIER)		\
	asm volatile ( 	PRE_BARRIER						\
			"0:\tlwarx %[retval],0,%[obj_ptr]\n"			\
			ASM_OP							\
			"\tstwcx. %[scratch],0,%[obj_ptr]\n"			\
			"\tbne- 0b\n"						\
			POST_BARRIER						\
			: [retval]	"=&r"	(RET),				\
			  [obj]		"+m"	(*OBJ_PTR),			\
			  [scratch]	"=&r"	(((__typeof__(VAL)){ 0 }))	\
			: [val]		"r"	(VAL),				\
			  [obj_ptr]	"r"	(OBJ_PTR)			\
			: "cr0")
#define PPC_ASM_XCHG__gtm_atomic_int__(OBJ_PTR, VAL, RET, PRE_BARRIER, POST_BARRIER)		\
	asm volatile ( 	PRE_BARRIER						\
			"0:\tlwarx %[retval],0,%[obj_ptr]\n"			\
			"\tstwcx. %[val],0,%[obj_ptr]\n"			\
			"\tbne- 0b\n"						\
			"\textsw %[retval],%[retval]\n"				\
			POST_BARRIER						\
			: [retval]	"=&r"	(RET),				\
			  [obj]		"+m"	(*OBJ_PTR)			\
			: [val]		"r"	(VAL),				\
			  [obj_ptr]	"r"	(OBJ_PTR)			\
			: "cr0")
#define PPC_ASM_LLSC__gtm_atomic_int__(OBJ_PTR, VAL, RET, ASM_OP, PRE_BARRIER, POST_BARRIER)		\
	asm volatile ( 	PRE_BARRIER						\
			"0:\tlwarx %[retval],0,%[obj_ptr]\n"			\
			"\textsw %[retval],%[retval]\n"				\
			ASM_OP							\
			"\tstwcx. %[scratch],0,%[obj_ptr]\n"			\
			"\tbne- 0b\n"						\
			POST_BARRIER						\
			: [retval]	"=&r"	(RET),				\
			  [obj]		"+m"	(*OBJ_PTR),			\
			  [scratch]	"=&r"	(((__typeof__(VAL)){ 0 }))	\
			: [val]		"r"	(VAL),				\
			  [obj_ptr]	"r"	(OBJ_PTR)			\
			: "cr0")
#define PPC_ASM_XCHG__gtm_atomic_long__(OBJ_PTR, VAL, RET, PRE_BARRIER, POST_BARRIER)		\
	asm volatile ( 	PRE_BARRIER						\
			"0:\tldarx %[retval],0,%[obj_ptr]\n"			\
			"\tstdcx. %[val],0,%[obj_ptr]\n"			\
			"\tbne- 0b\n"						\
			POST_BARRIER						\
			: [retval]	"=&r"	(RET),				\
			  [obj]		"+m"	(*OBJ_PTR)			\
			: [val]		"r"	(VAL),				\
			  [obj_ptr]	"r"	(OBJ_PTR)			\
			: "cr0")
#define PPC_ASM_LLSC__gtm_atomic_long__(OBJ_PTR, VAL, RET, ASM_OP, PRE_BARRIER, POST_BARRIER)		\
	asm volatile ( 	PRE_BARRIER						\
			"0:\tldarx %[retval],0,%[obj_ptr]\n"			\
			ASM_OP							\
			"\tstdcx. %[scratch],0,%[obj_ptr]\n"			\
			"\tbne- 0b\n"						\
			POST_BARRIER						\
			: [retval]	"=&r"	(RET),				\
			  [obj]		"+m"	(*OBJ_PTR),			\
			  [scratch]	"=&r"	(((__typeof__(VAL)){ 0 }))	\
			: [val]		"r"	(VAL),				\
			  [obj_ptr]	"r"	(OBJ_PTR)			\
			: "cr0")
#ifdef _ARCH_PWR8
#define PPC_ASM_LLSC__gtm_atomic_bool__ PPC_ASM_LLSC__gtm_atomic_uchar__
#define PPC_ASM_XCHG__gtm_atomic_bool__ PPC_ASM_XCHG__gtm_atomic_uchar__
#ifdef __CHAR_SIGNED__
#define PPC_ASM_LLSC__gtm_atomic_char__ PPC_ASM_LLSC__gtm_atomic_schar__
#define PPC_ASM_XCHG__gtm_atomic_char__ PPC_ASM_XCHG__gtm_atomic_schar__
#else
#define PPC_ASM_LLSC__gtm_atomic_char__ PPC_ASM_LLSC__gtm_atomic_uchar__
#define PPC_ASM_XCHG__gtm_atomic_char__ PPC_ASM_XCHG__gtm_atomic_uchar__
#endif
#endif
#define PPC_ASM_LLSC__gtm_atomic_ulong__ PPC_ASM_LLSC__gtm_atomic_long__
#define PPC_ASM_LLSC__gtm_atomic_llong__ PPC_ASM_LLSC__gtm_atomic_long__
#define PPC_ASM_LLSC__gtm_atomic_ullong__ PPC_ASM_LLSC__gtm_atomic_long__
#define PPC_ASM_XCHG__gtm_atomic_ulong__ PPC_ASM_XCHG__gtm_atomic_long__
#define PPC_ASM_XCHG__gtm_atomic_llong__ PPC_ASM_XCHG__gtm_atomic_long__
#define PPC_ASM_XCHG__gtm_atomic_ullong__ PPC_ASM_XCHG__gtm_atomic_long__
/* Compare-and-swap assembly for powerpc for all supported type-width/sign combinations.
 * Since we return a bool and write to EXP_PTR using the fixed-with store operations, sign-handling
 * is only necessary to make sure the comparison is correct and we leave the exp_reg in an alright state
 * if we are identifying it as an output register (even though the compiler should force a re-load from memory
 * since the location is always a write-out "=m" operand). So for the widths that don't have width-compares
 * (cmp x,1,x,x means long-compare and cmp x,0,x,x means 32-bit compare), we use clrldi to zero out everything
 * but the bits inside the width, so compares with l{h,b}arx-loaded values will come out right. In case the compiler
 * thinks the register is still valid, we also reset it with an extsh or extsb as appropriate at the end.
 */
#ifdef _ARCH_PWR8
#define PPC_ASM_CMPSWAP__gtm_atomic_uchar__(OBJ_PTR, EXP_PTR, VAL, RET, PRE_BARRIER, SUCCESS_BARRIER, FAIL_BARRIER)	\
	asm volatile (	"\tli %[retval],1\n"										\
			PRE_BARRIER											\
			"0:\tlbarx %[scratch],0,%[obj_ptr]\n"								\
			"\tcmp 0,0,%[scratch],%[exp_reg]\n"								\
			"\tbne- 1f\n"											\
			"\tstbcx. %[val],0,%[obj_ptr]\n"								\
			"\tbne- 0b\n"											\
			SUCCESS_BARRIER											\
			"\tb 2f\n"											\
			"1:"FAIL_BARRIER										\
			"\tli %[retval],0\n"										\
			"\tstb %[scratch],%[exp]\n"									\
			"2:\t\n"											\
			: [exp]		"=m"	(*EXP_PTR),								\
			  [obj]		"+m"	(*OBJ_PTR),								\
			  [scratch]	"=&r"	(((__typeof__(VAL)){ 0 })),						\
			  [retval]	"=&r"	(RET)									\
			: [exp_reg]	"r"	(*(volatile __typeof__(EXP_PTR))EXP_PTR),				\
			  [val]		"r"	(VAL),									\
			  [obj_ptr]	"r"	(OBJ_PTR)								\
			: "cr0")
#define PPC_ASM_CMPSWAP__gtm_atomic_schar__(OBJ_PTR, EXP_PTR, VAL, RET, PRE_BARRIER, SUCCESS_BARRIER, FAIL_BARRIER)	\
	asm volatile (	"\tli %[retval],1\n"										\
			"\tclrldi %[exp_reg],%[exp_reg],56\n"								\
			PRE_BARRIER											\
			"0:\tlbarx %[scratch],0,%[obj_ptr]\n"								\
			"\tcmp 0,0,%[scratch],%[exp_reg]\n"								\
			"\tbne- 1f\n"											\
			"\tstbcx. %[val],0,%[obj_ptr]\n"								\
			"\tbne- 0b\n"											\
			SUCCESS_BARRIER											\
			"\tb 2f\n"											\
			"1:"FAIL_BARRIER										\
			"\tli %[retval],0\n"										\
			"\tstb %[scratch],%[exp]\n"									\
			"2:\textsb %[exp_reg],%[exp_reg]\n"								\
			: [exp_reg]	"+&r"	(*(volatile __typeof__(EXP_PTR))EXP_PTR),				\
			  [exp]		"=m"	(*EXP_PTR),								\
			  [obj]		"+m"	(*OBJ_PTR),								\
			  [scratch]	"=&r"	(((__typeof__(VAL)){ 0 })),						\
			  [retval]	"=&r"	(RET)									\
			: [val]		"r"	(VAL),									\
			  [obj_ptr]	"r"	(OBJ_PTR)								\
			: "cr0")
#define PPC_ASM_CMPSWAP__gtm_atomic_ushort__(OBJ_PTR, EXP_PTR, VAL, RET, PRE_BARRIER, SUCCESS_BARRIER, FAIL_BARRER)	\
	asm volatile (	"\tli %[retval],1\n"										\
			PRE_BARRIER											\
			"0:\tlharx %[scratch],0,%[obj_ptr]\n"								\
			"\tcmp 0,0,%[scratch],%[exp_reg]\n"								\
			"\tbne- 1f\n"											\
			"\tsthcx. %[val],0,%[obj_ptr]\n"								\
			"\tbne- 0b\n"											\
			SUCCESS_BARRIER											\
			"\tb 2f\n"											\
			"1:"FAIL_BARRIER										\
			"\tli %[retval],0\n"										\
			"\tsth %[scratch],%[exp]\n"									\
			"2:\t\n"											\
			: [exp]		"=m"	(*EXP_PTR),								\
			  [obj]		"+m"	(*OBJ_PTR),								\
			  [scratch]	"=&r"	(((__typeof__(VAL)){ 0 })),						\
			  [retval]	"=&r"	(RET)									\
			: [exp_reg]	"r"	(*(volatile __typeof__(EXP_PTR))EXP_PTR),				\
			  [val]		"r"	(VAL),									\
			  [obj_ptr]	"r"	(OBJ_PTR)								\
			: "cr0")
#define PPC_ASM_CMPSWAP__gtm_atomic_short__(OBJ_PTR, EXP_PTR, VAL, RET, PRE_BARRIER, SUCCESS_BARRIER, FAIL_BARRIER)	\
	asm volatile (	"\tli %[retval],1\n"										\
			"\tclrldi %[exp_reg],%[exp_reg],48\n"								\
			PRE_BARRIER											\
			"0:\tlharx %[scratch],0,%[obj_ptr]\n"								\
			"\tcmp 0,0,%[scratch],%[exp_reg]\n"								\
			"\tbne- 1f\n"											\
			"\tsthcx. %[val],0,%[obj_ptr]\n"								\
			"\tbne- 0b\n"											\
			SUCCESS_BARRIER											\
			"\tb 2f\n"											\
			"1:"FAIL_BARRIER										\
			"\tli %[retval],0\n"										\
			"\tsth %[scratch],%[exp]\n"									\
			"2:\textsh %[exp_reg],%[exp_reg]\n"								\
			: [exp_reg]	"+&r"	(*(volatile __typeof__(EXP_PTR))EXP_PTR),				\
			  [exp]		"=m"	(*EXP_PTR),								\
			  [obj]		"+m"	(*OBJ_PTR),								\
			  [scratch]	"=&r"	(((__typeof__(VAL)){ 0 })),						\
			  [retval]	"=&r"	(RET)									\
			: [val]		"r"	(VAL),									\
			  [obj_ptr]	"r"	(OBJ_PTR)								\
			: "cr0")
#endif
#define PPC_ASM_CMPSWAP__gtm_atomic_int__(OBJ_PTR, EXP_PTR, VAL, RET, PRE_BARRIER, SUCCESS_BARRIER, FAIL_BARRIER)	\
	asm volatile (	"\tli %[retval],1\n"										\
			PRE_BARRIER											\
			"0:\tlwarx %[scratch],0,%[obj_ptr]\n"								\
			"\tcmp 0,0,%[scratch],%[exp_reg]\n"								\
			"\tbne- 1f\n"											\
			"\tstwcx. %[val],0,%[obj_ptr]\n"								\
			"\tbne- 0b\n"											\
			SUCCESS_BARRIER											\
			"\tb 2f\n"											\
			"1:"FAIL_BARRIER										\
			"\tli %[retval],0\n"										\
			"\tstw %[scratch],%[exp]\n"									\
			"2:\t\n"											\
			: [exp]		"=m"	(*EXP_PTR),								\
			  [obj]		"+m"	(*OBJ_PTR),								\
			  [scratch]	"=&r"	(((__typeof__(VAL)){ 0 })),						\
			  [retval]	"=&r"	(RET)									\
			: [exp_reg]	"r"	(*(volatile __typeof__(EXP_PTR))EXP_PTR),				\
			  [val]		"r"	(VAL),									\
			  [obj_ptr]	"r"	(OBJ_PTR)								\
			: "cr0")
#define PPC_ASM_CMPSWAP__gtm_atomic_long__(OBJ_PTR, EXP_PTR, VAL, RET, PRE_BARRIER, SUCCESS_BARRIER, FAIL_BARRIER)	\
	asm volatile (	"\tli %[retval],1\n"										\
			PRE_BARRIER											\
			"0:\tldarx %[scratch],0,%[obj_ptr]\n"								\
			"\tcmp 0,1,%[scratch],%[exp_reg]\n"								\
			"\tbne- 1f\n"											\
			"\tstdcx. %[val],0,%[obj_ptr]\n"								\
			"\tbne- 0b\n"											\
			SUCCESS_BARRIER											\
			"\tb 2f\n"											\
			"1:"FAIL_BARRIER										\
			"\tli %[retval],0\n"										\
			"\tstd %[scratch],%[exp]\n"									\
			"2:\t\n"											\
			: [exp]		"=m"	(*EXP_PTR),								\
			  [obj]		"+m"	(*OBJ_PTR),								\
			  [scratch]	"=&r"	(((__typeof__(VAL)){ 0 })),						\
			  [retval]	"=&r"	(RET)									\
			: [exp_reg]	"r"	(*(volatile __typeof__(EXP_PTR))EXP_PTR),				\
			  [val]		"r"	(VAL),									\
			  [obj_ptr]	"r"	(OBJ_PTR)								\
			: "cr0")
#ifdef _ARCH_PWR8
#define PPC_ASM_CMPSWAP__gtm_atomic_bool__ PPC_ASM_CMPSWAP__gtm_atomic_uchar__
#ifdef __CHAR_SIGNED__
#define PPC_ASM_CMPSWAP__gtm_atomic_char__ PPC_ASM_CMPSWAP__gtm_atomic_schar__
#else
#define PPC_ASM_CMPSWAP__gtm_atomic_char__ PPC_ASM_CMPSWAP__gtm_atomic_uchar__
#endif
#endif
#define PPC_ASM_CMPSWAP__gtm_atomic_uint__ PPC_ASM_CMPSWAP__gtm_atomic_int__
#define PPC_ASM_CMPSWAP__gtm_atomic_ulong__ PPC_ASM_CMPSWAP__gtm_atomic_long__
#define PPC_ASM_CMPSWAP__gtm_atomic_llong__ PPC_ASM_CMPSWAP__gtm_atomic_long__
#define PPC_ASM_CMPSWAP__gtm_atomic_ullong__ PPC_ASM_CMPSWAP__gtm_atomic_long__
/* Powerpc assembly strings for the various FETCH_* combinations. */
#define PPC_ASMSTR_ADD 	"\tadd %[scratch],%[val],%[retval]\n"
#define PPC_ASMSTR_SUB 	"\tsubf %[scratch],%[val],%[retval]\n"
#define PPC_ASMSTR_AND	"\tand %[scratch],%[val],%[retval]\n"
#define PPC_ASMSTR_OR	"\tor %[scratch],%[val],%[retval]\n"
#define PPC_ASMSTR_XOR	"\txor %[scratch],%[val],%[retval]\n"
/* Utility macros to define the inline functions which map to the ATOMIC_* macros, necessary because some of them return a value. */
#define DEFINE_ATOMIC_LOAD(OBJ_TYPE, ORD, PRE_BARRIER, POST_BARRIER)			\
static inline OBJ_TYPE atomic_load__##OBJ_TYPE##__##ORD(const volatile OBJ_TYPE *p)	\
{											\
	OBJ_TYPE ret;									\
	PPC_ASM_LD##__##OBJ_TYPE##__(p, ret, PRE_BARRIER, POST_BARRIER);		\
	return ret;									\
}
#define DEFINE_CMP_ATOMIC_LOAD(OBJ_TYPE, ORD, PRE_BARRIER, POST_BARRIER)		\
static inline OBJ_TYPE atomic_load__##OBJ_TYPE##__##ORD(const volatile OBJ_TYPE *p)	\
{											\
	OBJ_TYPE ret;									\
	PPC_ASM_LDCMP##__##OBJ_TYPE##__(p, ret, PRE_BARRIER, POST_BARRIER);		\
	return ret;									\
}
#define DEFINE_ATOMIC_STORE(OBJ_TYPE, ORD, PRE_BARRIER, POST_BARRIER)			\
static inline void atomic_store__##OBJ_TYPE##__##ORD(volatile OBJ_TYPE *p, OBJ_TYPE v)	\
{											\
	PPC_ASM_ST##__##OBJ_TYPE##__(p, v, PRE_BARRIER, POST_BARRIER);			\
	return;										\
}
#define DEFINE_ATOMIC_LL_SC_OP(OBJ_TYPE, ORD, OPNAME, ASM, PRE_BARRIER, POST_BARRIER)		\
static inline OBJ_TYPE OPNAME##OBJ_TYPE##__##ORD(volatile OBJ_TYPE *p, OBJ_TYPE v)		\
{												\
	OBJ_TYPE ret;										\
	PPC_ASM_LLSC##__##OBJ_TYPE##__(p, v, ret, ASM, PRE_BARRIER, POST_BARRIER);		\
	return ret;										\
}
#define DEFINE_ATOMIC_XCHG(OBJ_TYPE, ORD, PRE_BARRIER, POST_BARRIER)				\
static inline OBJ_TYPE atomic_exchange__##OBJ_TYPE##__##ORD(volatile OBJ_TYPE *p, OBJ_TYPE v)	\
{												\
	OBJ_TYPE ret;										\
	PPC_ASM_XCHG##__##OBJ_TYPE##__(p, v, ret, PRE_BARRIER, POST_BARRIER);			\
	return ret;										\
}
#define DEFINE_ATOMIC_COMPARE_EXCHANGE(OBJ_TYPE, SUCCESS_ORD, FAIL_ORD, PRE_BARRIER, SUCCESS_BARRIER, FAIL_BARRIER)		\
static inline bool atomic_compare_exchange__##OBJ_TYPE##__##SUCCESS_ORD##__##FAIL_ORD(volatile OBJ_TYPE *p, OBJ_TYPE *e,	\
		OBJ_TYPE d)													\
{																\
	bool ret;														\
	PPC_ASM_CMPSWAP##__##OBJ_TYPE##__(p, e, d, ret, PRE_BARRIER, SUCCESS_BARRIER, FAIL_BARRIER);				\
	return ret;														\
}
#define DEFINE_ATOMIC_FETCH_ADD(OBJ_TYPE, ORD, PRE_BARRIER, POST_BARRIER) DEFINE_ATOMIC_LL_SC_OP(OBJ_TYPE, ORD, \
		atomic_fetch_add__, PPC_ASMSTR_ADD, PRE_BARRIER, POST_BARRIER)
#define DEFINE_ATOMIC_FETCH_SUB(OBJ_TYPE, ORD, PRE_BARRIER, POST_BARRIER) DEFINE_ATOMIC_LL_SC_OP(OBJ_TYPE, ORD, \
		atomic_fetch_sub__, PPC_ASMSTR_SUB, PRE_BARRIER, POST_BARRIER)
#define DEFINE_ATOMIC_FETCH_OR(OBJ_TYPE, ORD, PRE_BARRIER, POST_BARRIER) DEFINE_ATOMIC_LL_SC_OP(OBJ_TYPE, ORD, \
		atomic_fetch_or__, PPC_ASMSTR_OR, PRE_BARRIER, POST_BARRIER)
#define DEFINE_ATOMIC_FETCH_XOR(OBJ_TYPE, ORD, PRE_BARRIER, POST_BARRIER) DEFINE_ATOMIC_LL_SC_OP(OBJ_TYPE, ORD, \
		atomic_fetch_xor__, PPC_ASMSTR_XOR, PRE_BARRIER, POST_BARRIER)
#define DEFINE_ATOMIC_FETCH_AND(OBJ_TYPE, ORD, PRE_BARRIER, POST_BARRIER) DEFINE_ATOMIC_LL_SC_OP(OBJ_TYPE, ORD, \
		atomic_fetch_and__, PPC_ASMSTR_AND, PRE_BARRIER, POST_BARRIER)
#define DEFINE_ALL_LL_SC_OPS(OBJ_TYPE, ORD, PRE_BARRIER, POST_BARRIER)	\
DEFINE_ATOMIC_FETCH_ADD(OBJ_TYPE, ORD, PRE_BARRIER, POST_BARRIER)		\
DEFINE_ATOMIC_FETCH_SUB(OBJ_TYPE, ORD, PRE_BARRIER, POST_BARRIER)		\
DEFINE_ATOMIC_FETCH_OR(OBJ_TYPE, ORD, PRE_BARRIER, POST_BARRIER) 		\
DEFINE_ATOMIC_FETCH_XOR(OBJ_TYPE, ORD, PRE_BARRIER, POST_BARRIER)		\
DEFINE_ATOMIC_FETCH_AND(OBJ_TYPE, ORD, PRE_BARRIER, POST_BARRIER)		\
DEFINE_ATOMIC_XCHG(OBJ_TYPE, ORD, PRE_BARRIER, POST_BARRIER)
/* What follows is the definitions themselves. The PRE_BARRIER and POST_BARRIER are generally powerpc memory ordering primitives
 * used to create the various ordering constraints.
 */
#define DEFINE_memory_order_relaxed_ATOMIC_STORE(TYPE)	\
	DEFINE_ATOMIC_STORE(TYPE, memory_order_relaxed, "\tnop\n", "\tnop\n")
#define DEFINE_memory_order_release_ATOMIC_STORE(TYPE)	\
	DEFINE_ATOMIC_STORE(TYPE, memory_order_release, "\tlwsync\n", "\tnop\n")
#define DEFINE_memory_order_seq_cst_ATOMIC_STORE(TYPE)	\
	DEFINE_ATOMIC_STORE(TYPE, memory_order_seq_cst, "\tsync\n", "\tnop\n")
#define DEFINE_memory_order_relaxed_ATOMIC_LOAD(TYPE)	\
	DEFINE_ATOMIC_LOAD(TYPE, memory_order_relaxed, "\tnop\n", "\tnop\n")
#define DEFINE_memory_order_acquire_ATOMIC_LOAD(TYPE)	\
	DEFINE_CMP_ATOMIC_LOAD(TYPE, memory_order_acquire, "\tnop\n", "\tisync\n")
#define DEFINE_memory_order_seq_cst_ATOMIC_LOAD(TYPE)	\
	DEFINE_CMP_ATOMIC_LOAD(TYPE, memory_order_seq_cst, "\tsync\n", "\tisync\n")
#define DEFINE_memory_order_relaxed_ATOMIC_XCHG(TYPE)	\
	DEFINE_ATOMIC_XCHG(TYPE, memory_order_relaxed, "\tnop\n", "\tnop\n")
#define DEFINE_memory_order_acquire_ATOMIC_XCHG(TYPE)	\
	DEFINE_ATOMIC_XCHG(TYPE, memory_order_acquire, "\tnop\n", "\tisync\n")
#define DEFINE_memory_order_release_ATOMIC_XCHG(TYPE)	\
	DEFINE_ATOMIC_XCHG(TYPE, memory_order_release, "\tlwsync\n", "\tnop\n")
#define DEFINE_memory_order_acq_rel_ATOMIC_XCHG(TYPE)	\
	DEFINE_ATOMIC_XCHG(TYPE, memory_order_acq_rel, "\tlwsync\n", "\tisync\n")
#define DEFINE_memory_order_seq_cst_ATOMIC_XCHG(TYPE)	\
	DEFINE_ATOMIC_XCHG(TYPE, memory_order_seq_cst, "\tsync\n", "\tisync\n")
#define DEFINE_memory_order_relaxed_ATOMIC_FETCH_ADD(TYPE)	\
	DEFINE_ATOMIC_FETCH_ADD(TYPE, memory_order_relaxed, "\tnop\n", "\tnop\n")
#define DEFINE_memory_order_acquire_ATOMIC_FETCH_ADD(TYPE)	\
	DEFINE_ATOMIC_FETCH_ADD(TYPE, memory_order_acquire, "\tnop\n", "\tisync\n")
#define DEFINE_memory_order_release_ATOMIC_FETCH_ADD(TYPE)	\
	DEFINE_ATOMIC_FETCH_ADD(TYPE, memory_order_release, "\tlwsync\n", "\tnop\n")
#define DEFINE_memory_order_acq_rel_ATOMIC_FETCH_ADD(TYPE)	\
	DEFINE_ATOMIC_FETCH_ADD(TYPE, memory_order_acq_rel, "\tlwsync\n", "\tisync\n")
#define DEFINE_memory_order_seq_cst_ATOMIC_FETCH_ADD(TYPE)	\
	DEFINE_ATOMIC_FETCH_ADD(TYPE, memory_order_seq_cst, "\tsync\n", "\tisync\n")
#define DEFINE_memory_order_relaxed_ATOMIC_FETCH_SUB(TYPE)	\
	DEFINE_ATOMIC_FETCH_SUB(TYPE, memory_order_relaxed, "\tnop\n", "\tnop\n")
#define DEFINE_memory_order_acquire_ATOMIC_FETCH_SUB(TYPE)	\
	DEFINE_ATOMIC_FETCH_SUB(TYPE, memory_order_acquire, "\tnop\n", "\tisync\n")
#define DEFINE_memory_order_release_ATOMIC_FETCH_SUB(TYPE)	\
	DEFINE_ATOMIC_FETCH_SUB(TYPE, memory_order_release, "\tlwsync\n", "\tnop\n")
#define DEFINE_memory_order_acq_rel_ATOMIC_FETCH_SUB(TYPE)	\
	DEFINE_ATOMIC_FETCH_SUB(TYPE, memory_order_acq_rel, "\tlwsync\n", "\tisync\n")
#define DEFINE_memory_order_seq_cst_ATOMIC_FETCH_SUB(TYPE)	\
	DEFINE_ATOMIC_FETCH_SUB(TYPE, memory_order_seq_cst, "\tsync\n", "\tisync\n")
#define DEFINE_memory_order_relaxed_ATOMIC_FETCH_OR(TYPE)	\
	DEFINE_ATOMIC_FETCH_OR(TYPE, memory_order_relaxed, "\tnop\n", "\tnop\n")
#define DEFINE_memory_order_acquire_ATOMIC_FETCH_OR(TYPE)	\
	DEFINE_ATOMIC_FETCH_OR(TYPE, memory_order_acquire, "\tnop\n", "\tisync\n")
#define DEFINE_memory_order_release_ATOMIC_FETCH_OR(TYPE)	\
	DEFINE_ATOMIC_FETCH_OR(TYPE, memory_order_release, "\tlwsync\n", "\tnop\n")
#define DEFINE_memory_order_acq_rel_ATOMIC_FETCH_OR(TYPE)	\
	DEFINE_ATOMIC_FETCH_OR(TYPE, memory_order_acq_rel, "\tlwsync\n", "\tisync\n")
#define DEFINE_memory_order_seq_cst_ATOMIC_FETCH_OR(TYPE)	\
	DEFINE_ATOMIC_FETCH_OR(TYPE, memory_order_seq_cst, "\tsync\n", "\tisync\n")
#define DEFINE_memory_order_relaxed_ATOMIC_FETCH_XOR(TYPE)	\
	DEFINE_ATOMIC_FETCH_XOR(TYPE, memory_order_relaxed, "\tnop\n", "\tnop\n")
#define DEFINE_memory_order_acquire_ATOMIC_FETCH_XOR(TYPE)	\
	DEFINE_ATOMIC_FETCH_XOR(TYPE, memory_order_acquire, "\tnop\n", "\tisync\n")
#define DEFINE_memory_order_release_ATOMIC_FETCH_XOR(TYPE)	\
	DEFINE_ATOMIC_FETCH_XOR(TYPE, memory_order_release, "\tlwsync\n", "\tnop\n")
#define DEFINE_memory_order_acq_rel_ATOMIC_FETCH_XOR(TYPE)	\
	DEFINE_ATOMIC_FETCH_XOR(TYPE, memory_order_acq_rel, "\tlwsync\n", "\tisync\n")
#define DEFINE_memory_order_seq_cst_ATOMIC_FETCH_XOR(TYPE)	\
	DEFINE_ATOMIC_FETCH_XOR(TYPE, memory_order_seq_cst, "\tsync\n", "\tisync\n")
#define DEFINE_memory_order_relaxed_ATOMIC_FETCH_AND(TYPE)	\
	DEFINE_ATOMIC_FETCH_AND(TYPE, memory_order_relaxed, "\tnop\n", "\tnop\n")
#define DEFINE_memory_order_acquire_ATOMIC_FETCH_AND(TYPE)	\
	DEFINE_ATOMIC_FETCH_AND(TYPE, memory_order_acquire, "\tnop\n", "\tisync\n")
#define DEFINE_memory_order_release_ATOMIC_FETCH_AND(TYPE)	\
	DEFINE_ATOMIC_FETCH_AND(TYPE, memory_order_release, "\tlwsync\n", "\tnop\n")
#define DEFINE_memory_order_acq_rel_ATOMIC_FETCH_AND(TYPE)	\
	DEFINE_ATOMIC_FETCH_AND(TYPE, memory_order_acq_rel, "\tlwsync\n", "\tisync\n")
#define DEFINE_memory_order_seq_cst_ATOMIC_FETCH_AND(TYPE)	\
	DEFINE_ATOMIC_FETCH_AND(TYPE, memory_order_seq_cst, "\tsync\n", "\tisync\n")
#define DEFINE_memory_order_relaxed_memory_order_relaxed_ATOMIC_COMPARE_EXCHANGE(TYPE)	\
	DEFINE_ATOMIC_COMPARE_EXCHANGE(TYPE, memory_order_relaxed, memory_order_relaxed, "\tnop\n", "\tnop\n", "\tnop\n")
#define DEFINE_memory_order_relaxed_memory_order_acquire_ATOMIC_COMPARE_EXCHANGE(TYPE)	\
	DEFINE_ATOMIC_COMPARE_EXCHANGE(TYPE, memory_order_relaxed, memory_order_acquire, "\tnop\n", "\tnop\n", "\tisync\n")
#define DEFINE_memory_order_acquire_memory_order_relaxed_ATOMIC_COMPARE_EXCHANGE(TYPE)	\
	DEFINE_ATOMIC_COMPARE_EXCHANGE(TYPE, memory_order_acquire, memory_order_relaxed, "\tnop\n", "\tisync\n", "\tnop\n")
#define DEFINE_memory_order_acquire_memory_order_acquire_ATOMIC_COMPARE_EXCHANGE(TYPE)	\
	DEFINE_ATOMIC_COMPARE_EXCHANGE(TYPE, memory_order_acquire, memory_order_acquire, "\tnop\n", "\tisync\n", "\tnop\n")
#define DEFINE_memory_order_release_memory_order_relaxed_ATOMIC_COMPARE_EXCHANGE(TYPE)	\
	DEFINE_ATOMIC_COMPARE_EXCHANGE(TYPE, memory_order_release, memory_order_relaxed, "\tlwsync\n", "\tnop\n", "\tnop\n")
#define DEFINE_memory_order_release_memory_order_acquire_ATOMIC_COMPARE_EXCHANGE(TYPE)	\
	DEFINE_ATOMIC_COMPARE_EXCHANGE(TYPE, memory_order_release, memory_order_acquire, "\tlwsync\n", "\tnop\n", "\tisync\n")
#define DEFINE_memory_order_acq_rel_memory_order_relaxed_ATOMIC_COMPARE_EXCHANGE(TYPE)	\
	DEFINE_ATOMIC_COMPARE_EXCHANGE(TYPE, memory_order_acq_rel, memory_order_relaxed, "\tlwsync\n", "\tisync\n", "\tnop\n")
#define DEFINE_memory_order_acq_rel_memory_order_acquire_ATOMIC_COMPARE_EXCHANGE(TYPE)	\
	DEFINE_ATOMIC_COMPARE_EXCHANGE(TYPE, memory_order_acq_rel, memory_order_acquire, "\tlwsync\n", "\tisync\n", "\tisync\n")
#define DEFINE_memory_order_seq_cst_memory_order_relaxed_ATOMIC_COMPARE_EXCHANGE(TYPE)	\
	DEFINE_ATOMIC_COMPARE_EXCHANGE(TYPE, memory_order_seq_cst, memory_order_relaxed, "\tsync\n", "\tisync\n", "\tnop\n")
#define DEFINE_memory_order_seq_cst_memory_order_acquire_ATOMIC_COMPARE_EXCHANGE(TYPE)	\
	DEFINE_ATOMIC_COMPARE_EXCHANGE(TYPE, memory_order_seq_cst, memory_order_acquire, "\tsync\n", "\tisync\n", "\tisync\n")
#define DEFINE_memory_order_seq_cst_memory_order_seq_cst_ATOMIC_COMPARE_EXCHANGE(TYPE)	\
	DEFINE_ATOMIC_COMPARE_EXCHANGE(TYPE, memory_order_seq_cst, memory_order_seq_cst, "\tsync\n", "\tisync\n", "\tisync\n")
#define DEFINE_REGULAR_ATOMIC_OP(TYPE, OPERATION, MEM_ORDER) DEFINE_##MEM_ORDER##_##OPERATION(TYPE)
#define DEFINE_CE_ATOMIC_OP(TYPE, OPERATION, MEM_ORDER_1, MEM_ORDER_2) \
	DEFINE_##MEM_ORDER_1##MEM_ORDER_2##_##ATOMIC_COMPARE_EXCHANGE(TYPE)
#define GET_OPDEF_TYPE(_1, _2, _3, _4, OPNAME, ...) DEFINE_##OPNAME##_ATOMIC_OP
#define DEFINE_ATOMIC_OP(...) GET_OPDEF_TYPE(__VA_ARGS__, CE, REGULAR)(__VA_ARGS__)
#define DEFINE_ALL_ATOMIC_OPS(TYPE)		\
DEFINE_memory_order_relaxed_ATOMIC_STORE(TYPE) 		\
DEFINE_memory_order_release_ATOMIC_STORE(TYPE) 		\
DEFINE_memory_order_seq_cst_ATOMIC_STORE(TYPE)		\
DEFINE_memory_order_relaxed_ATOMIC_LOAD(TYPE)		\
DEFINE_memory_order_acquire_ATOMIC_LOAD(TYPE)		\
DEFINE_memory_order_seq_cst_ATOMIC_LOAD(TYPE)		\
DEFINE_memory_order_relaxed_ATOMIC_XCHG(TYPE)		\
DEFINE_memory_order_acquire_ATOMIC_XCHG(TYPE)		\
DEFINE_memory_order_release_ATOMIC_XCHG(TYPE)		\
DEFINE_memory_order_acq_rel_ATOMIC_XCHG(TYPE)		\
DEFINE_memory_order_seq_cst_ATOMIC_XCHG(TYPE)		\
DEFINE_memory_order_relaxed_ATOMIC_FETCH_ADD(TYPE)		\
DEFINE_memory_order_acquire_ATOMIC_FETCH_ADD(TYPE)		\
DEFINE_memory_order_release_ATOMIC_FETCH_ADD(TYPE)		\
DEFINE_memory_order_acq_rel_ATOMIC_FETCH_ADD(TYPE)		\
DEFINE_memory_order_seq_cst_ATOMIC_FETCH_ADD(TYPE)		\
DEFINE_memory_order_relaxed_ATOMIC_FETCH_SUB(TYPE)		\
DEFINE_memory_order_acquire_ATOMIC_FETCH_SUB(TYPE)		\
DEFINE_memory_order_release_ATOMIC_FETCH_SUB(TYPE)		\
DEFINE_memory_order_acq_rel_ATOMIC_FETCH_SUB(TYPE)		\
DEFINE_memory_order_seq_cst_ATOMIC_FETCH_SUB(TYPE)		\
DEFINE_memory_order_relaxed_ATOMIC_FETCH_OR(TYPE)		\
DEFINE_memory_order_acquire_ATOMIC_FETCH_OR(TYPE)		\
DEFINE_memory_order_release_ATOMIC_FETCH_OR(TYPE)		\
DEFINE_memory_order_acq_rel_ATOMIC_FETCH_OR(TYPE)		\
DEFINE_memory_order_seq_cst_ATOMIC_FETCH_OR(TYPE)		\
DEFINE_memory_order_relaxed_ATOMIC_FETCH_XOR(TYPE)		\
DEFINE_memory_order_acquire_ATOMIC_FETCH_XOR(TYPE)		\
DEFINE_memory_order_release_ATOMIC_FETCH_XOR(TYPE)		\
DEFINE_memory_order_acq_rel_ATOMIC_FETCH_XOR(TYPE)		\
DEFINE_memory_order_seq_cst_ATOMIC_FETCH_XOR(TYPE)		\
DEFINE_memory_order_relaxed_ATOMIC_FETCH_AND(TYPE)		\
DEFINE_memory_order_acquire_ATOMIC_FETCH_AND(TYPE)		\
DEFINE_memory_order_release_ATOMIC_FETCH_AND(TYPE)		\
DEFINE_memory_order_acq_rel_ATOMIC_FETCH_AND(TYPE)		\
DEFINE_memory_order_seq_cst_ATOMIC_FETCH_AND(TYPE)		\
DEFINE_memory_order_relaxed_memory_order_relaxed_ATOMIC_COMPARE_EXCHANGE(TYPE)	\
DEFINE_memory_order_relaxed_memory_order_acquire_ATOMIC_COMPARE_EXCHANGE(TYPE)	\
DEFINE_memory_order_acquire_memory_order_relaxed_ATOMIC_COMPARE_EXCHANGE(TYPE)	\
DEFINE_memory_order_acquire_memory_order_acquire_ATOMIC_COMPARE_EXCHANGE(TYPE)	\
DEFINE_memory_order_release_memory_order_relaxed_ATOMIC_COMPARE_EXCHANGE(TYPE)	\
DEFINE_memory_order_release_memory_order_acquire_ATOMIC_COMPARE_EXCHANGE(TYPE)	\
DEFINE_memory_order_acq_rel_memory_order_relaxed_ATOMIC_COMPARE_EXCHANGE(TYPE)	\
DEFINE_memory_order_acq_rel_memory_order_acquire_ATOMIC_COMPARE_EXCHANGE(TYPE)	\
DEFINE_memory_order_seq_cst_memory_order_relaxed_ATOMIC_COMPARE_EXCHANGE(TYPE)	\
DEFINE_memory_order_seq_cst_memory_order_acquire_ATOMIC_COMPARE_EXCHANGE(TYPE)	\
DEFINE_memory_order_seq_cst_memory_order_seq_cst_ATOMIC_COMPARE_EXCHANGE(TYPE)

/* Mapping of ATOMIC_* macros to inline functions defined above. */
#define COMPILER_FENCE(ORD) __fence()
#define ATOMIC_LOAD(OBJ_PTR, ORD) OP_TYPE_ORD_MNEMONIC(atomic_load, OBJ_PTR, ORD)(OBJ_PTR)
#define ATOMIC_STORE(OBJ_PTR, VAL, ORD) OP_TYPE_ORD_MNEMONIC(atomic_store, OBJ_PTR, ORD)(OBJ_PTR, VAL)
#define ATOMIC_FETCH_ADD(OBJ_PTR, VAL, ORD) FOP_TYPE_ORD_MNEMONIC(atomic_fetch_add, OBJ_PTR, ORD)(OBJ_PTR, VAL)
#define ATOMIC_FETCH_SUB(OBJ_PTR, VAL, ORD) FOP_TYPE_ORD_MNEMONIC(atomic_fetch_sub, OBJ_PTR, ORD)(OBJ_PTR, VAL)
#define ATOMIC_FETCH_OR(OBJ_PTR, VAL, ORD) FOP_TYPE_ORD_MNEMONIC(atomic_fetch_or, OBJ_PTR, ORD)(OBJ_PTR, VAL)
#define ATOMIC_FETCH_XOR(OBJ_PTR, VAL, ORD) FOP_TYPE_ORD_MNEMONIC(atomic_fetch_xor, OBJ_PTR, ORD)(OBJ_PTR, VAL)
#define ATOMIC_FETCH_AND(OBJ_PTR, VAL, ORD) FOP_TYPE_ORD_MNEMONIC(atomic_fetch_and, OBJ_PTR, ORD)(OBJ_PTR, VAL)
#define ATOMIC_EXCHANGE(OBJ_PTR, VAL, ORD) FOP_TYPE_ORD_MNEMONIC(atomic_exchange, OBJ_PTR, ORD)(OBJ_PTR, VAL)
#define ATOMIC_COMPARE_EXCHANGE(OBJ_PTR, EXP_PTR, VAL, SUCCESS_ORD, FAIL_ORD) \
		FOP_TYPE_ORD_MNEMONIC(atomic_compare_exchange, OBJ_PTR, SUCCESS_ORD##__##FAIL_ORD)(OBJ_PTR, EXP_PTR, VAL)
#define ATOMIC_COMPARE_EXCHANGE_STRONG(OBJ_PTR, EXP_PTR, VAL, SUCCESS_ORD, FAIL_ORD) \
		ATOMIC_COMPARE_EXCHANGE(OBJ_PTR, EXP_PTR, VAL, SUCCESS_ORD, FAIL_ORD)
#define ATOMIC_COMPARE_EXCHANGE_WEAK(OBJ_PTR, EXP_PTR, VAL, SUCCESS_ORD, FAIL_ORD) \
		ATOMIC_COMPARE_EXCHANGE_STRONG(OBJ_PTR, EXP_PTR, VAL, SUCCESS_ORD, FAIL_ORD)
#else /* If not AIX, but also no GCC or C11 atomics */
#error "Unsupported platform"
#endif /* If linux elif aix else unsupported */
#endif /* if C11 atomics */
#endif /* ifndef GTM_ATOMIC_H_INCLUDED */
