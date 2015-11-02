/****************************************************************
 *								*
 *	Copyright 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_SIZEOF_H_INCLUDED
#define GTM_SIZEOF_H_INCLUDED

/* Note: sizeof returns a "UNSIGNED long" type by default. When one of the operand in a relational operation
 * (<, <= etc.) is a SIGNED type and the other is UNSIGNED, both operands are typecast to UNSIGNED (NOT SIGNED)
 * before performing the comparison. This has unexpected consequences. For example, if a variable XYZ is signed
 * and has a negative value of -1, one would expect an "if (X < sizeof(char))" to return TRUE (because X is
 * negative and sizeof of anything is positive) but it actually returns FALSE. We have gotten bit by this at least
 * twice (C9E09-002635 and C9J10-003208). Therefore we define a SIZEOF macro that returns a "SIGNED long" type and
 * use it throughout the code base. This way "if (X < SIZEOF(char))" will evaluate correctly irrespective of
 * whether X is signed or unsigned (if X is signed, since SIZEOF is also signed, a signed comparison occurs;
 * if X is unsigned, since SIZEOF is signed, C's typecasting rules cause a unsigned comparison to occur).
 *
 * In a platform where long is 64-bits and SIZEOF is used in comparisons involving 32-bit variables, a compiler
 * warning is issued for every 64->32 bit auto cast (zOS compiler for now). To avoid this warning, we define the
 * SIZEOF macro to return a "SIGNED int" type on zOS. This will take care of the most common sizeof usages.
 * Whenever SIZEOF needs to be used in expressions involving 64-bit pointer quantities, use ((INTPTR_T)SIZEOF(...)).
 * Whenever SIZEOF needs to be used in expressions involving 64-bit integer quantities, use ((long)SIZEOF(...)).
 *
 * Some modules (like getcaps.c, gtm_tparm.c etc) do not include mdef.h for reasons stated in the corresponding
 * files but yet use sizeof. To enable those modules to use the safer SIZEOF macro as well, we create a separate
 * header file gtm_sizeof.h that is included explicitly by them as well as by mdef.h.
 */

#if defined(__MVS__)
# define SIZEOF(X) ((int)(sizeof(X)))
#else
# define SIZEOF(X) ((long)sizeof(X))
#endif

#endif
