/****************************************************************
 *								*
 *	Copyright 2010, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/


#ifndef GTM_THREADGBL_included
#define GTM_THREADGBL_included

#ifndef NO_THREADGBL_DEFTYPES
# include "gtm_threadgbl_deftypes.h"	/* Avoided for gtm_threadgbl_deftypes.c which builds this header file */
#endif

/* The following three macros allow access to global variables located in the thread-global array. The ultimate
 * goal of this array is to permit GTM to become thread-safe. That is not yet true but this framework is the first
 * step in taming our global variable use. In the future, these macros will change to their thread-safe counterparts
 * such that we can have different arrays per thread.
 *
 * The macros can be classified as declaration (DCL_THREADGBL_ACCESS), definition (SETUP_THREADGBL_ACCESS), and
 * usage (TADR, TREF, TAREF1, TAREF2, RFPTR, SFPTR, IVFPTR).
 *
 * DCL_THREADGBL_ACCESS     - simple job is to declare the local variable used as an anchor to the thread global
 *                            array. It should be located in the declaration section of an entry point.
 *
 * SETUP_THREAD_GBL_ACCESS  - Sets the value of the local variable used as an anchor. Today, this is just a
 *			      global var de-reference. In the future, it will use thread-logic to access a
 *			      thread related base var. This should be placed near the top of the executable
 *			      code for the routine - certainly before the first TREF macro.
 * TADR			    - Used to obtain the address of the element.
 * TREF			    - Used to dereference a global var (see notes at TREF macro for usage).
 * TAREF1/TAREF2	    - Used to access array elements - TAREF1 for vectors, TAREF2 for 2 subscript access.
 * RFPTR		    - Used to reference a function pointer in an expression or conditional.
 * SFPTR		    - Used to set a new value into a function pointer.
 * IVFPTR		    - Used to invoke a function pointer.
 */

/* Declare local thread global anchor */
#define DCL_THREADGBL_ACCESS	void 	*lcl_gtm_threadgbl

/* Setup the local thread global anchor giving a value (and purpose in life) */
#define SETUP_THREADGBL_ACCESS	lcl_gtm_threadgbl = gtm_threadgbl

/* Reference a given global in the thread global array. There are a couple of different ways this macro can be
 * used. Note that its first term is a "de-reference" (aka "*"). As an example, say we have a global name defined
 * as follows:
 *
 *	somestruct *gblname;
 *
 * and we wish to access gblname->subfield. If we code "TREF(gblname)->subfield", that would generate
 *
 *      *((somestruct **)((char *)lcl_gtm_threadgbl + nnn))->subfield
 *
 * Note that the first dereference covers the entire expression instead of just the TREF expression. This is an incorrect
 * reference that hopefully produces a compiler error instead of a hard-to-find bug. What needs to be coded instead is
 * (TREF(gblname))->subfield which generates:
 *
 * 	(*((somestruct **)((char *)lcl_gtm_threadgbl + nnn)))->subfield
 *
 * This isolates the dereference to the expression it needs to refer to and produces an expression to correctly access the
 * desired field. However, if the global is not a pointer, but say, a boolean, then you CANNOT surround the TREF macro with
 * parens if the expression is on the left hand side of the "=" operator because that is not a valid LREF expression. So if
 * the field were a boolean, you could modify it as "TREF(somebool) = TRUE;".
 *
 * The general rule of thumb is if the global is a pointer being used to access a subfield, or if the global is a structure
 * itself and you are accessing subfields with a "." operator (e.g. (TREF(fnpca)).fnpcs[i].indx = i;) surround the TREF with ().
 * Otherwise* you can leave it unadorned.
 *
 * Note that function pointers present some odd constraints. You can cast something to a function pointer type but only
 * when you are going to actually invoke it (as the IVFPTR() macro does below). But this means that non-invoking references
 * (or assignments) cannot use casts to make types match. So we need 2 additional macros for reference in an expression (RFPTR)
 * or setting a function pointer (SFPTR) so we can control the necessary aspects of the expressions involved. See
 * $sr_unix/generic_signal_handler.c for uses of most of these macros used with function pointers.
 */
#define TREF(name) *((ggt_##name *)((char *)lcl_gtm_threadgbl + ggo_##name))

/* For address of item use TADR(name) macro */
#define TADR(name) ((ggt_##name *)((char *)lcl_gtm_threadgbl + ggo_##name))

/* For access to single dimension array (vector), use TAREF1 macro */
#define TAREF1(name, indx1) (TADR(name))[indx1]

/* For access to 2 dimension array, use TAREF2 macro */
#define TAREF2(name, indx1, indx2) ((ggt_##name *)((char *)lcl_gtm_threadgbl + ggo_##name +	/* Base addr of array */	\
						   (((indx1) - 1) * SIZEOF(ggt_##name) * ggd_##name)))[indx2]  /* Row offset */

/* To set a function pointer, use SFPTR(name) macro - Used to copy an address INTO a function pointer */
#define SFPTR(name, value) *((char **)((char *)lcl_gtm_threadgbl + ggo_##name)) = (char *)(value)

/* To reference a function pointer's value, use RFPTR(name) macro. Use only in rvalue RHS expressions. Does not produce
 * an lvalue suitable for LHS usage. See SFPTR() above if function pointer needs a new value.
 */
#define RFPTR(name) (ggt_##name (*)gga_##name)(*(char **)((char *)lcl_gtm_threadgbl + ggo_##name))

/* To invoke a function pointer, use IVFPTR(name) macro */
#define IVFPTR(name) ((ggf_##name)(*(char **)((char *)lcl_gtm_threadgbl + ggo_##name)))

/* In the main routines for GTM and the utilities that need to initialize the thread global structure all other routines access,
 * this macro should be used in lieu of SETUP_THREADGBL_ACCESS.
 */
#define GTM_THREADGBL_INIT 			\
{						\
	gtm_threadgbl_init();			\
	SETUP_THREADGBL_ACCESS;			\
}

#endif
