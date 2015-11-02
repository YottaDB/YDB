/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* gtm_ctype.h - interlude to <ctype.h> system header file.	*/

#ifndef GTM_CTYPEH
#define GTM_CTYPEH

#if defined(__osf__) && defined(__alpha)

/* On Tru64, <ctype.h> contains declarations of arrays of 64-bit pointers
 * in system library routines.  The following pragma's are necessary
 * to ensure that references to those arrays declare them as 64-bit
 * pointer arrays, even if the including C program is compiled with
  * 32-bit pointer options.
*/

#pragma pointer_size (save)
#pragma pointer_size (long)

#endif

#include <ctype.h>

/* The behavior of the system-defined ISxxxxx macros can vary based on the current locale and platform.
 * In the "C" locale, characters are classified according to the rules of the US-ASCII 7-bit coded character set.
 * In non-"C" locales (for example, UTF-8 mode) the result of ISXXXXX might not be what is expected
 * For example, ISALNUM(240) will return TRUE in UTF8 mode and FALSE in M mode (C locale) on Solaris.
 * Therefore define ISxxxxx_ASCII variant macros that additionally do check for ASCII.
 * Callers that need to check for a ISxxxxx property within the ASCII range should use the ISxxxxx_ASCII variants.
 * This makes the return value consistent across all platforms and independent of the locale.
 * We do not expect any callers of the non-ASCII macros in the GT.M codebase.
 */

#ifdef ISALNUM
#undef ISALNUM
#endif
#define ISALNUM	isalnum
#define ISALNUM_ASCII(CH)	(IS_ASCII(CH) && ISALNUM(CH))

#ifdef ISALPHA
#undef ISALPHA
#endif
#define ISALPHA	isalpha
#define ISALPHA_ASCII(CH)	(IS_ASCII(CH) && ISALPHA(CH))

#ifdef ISCNTRL
#undef ISCNTRL
#endif
#define ISCNTRL	iscntrl
#define ISCNTRL_ASCII(CH)	(IS_ASCII(CH) && ISCNTRL(CH))

#ifdef ISDIGIT
#undef ISDIGIT
#endif
#define ISDIGIT	isdigit
#define ISDIGIT_ASCII(CH)	(IS_ASCII(CH) && ISDIGIT(CH))

#ifdef ISGRAPH
#undef ISGRAPH
#endif
#define ISGRAPH	isgraph
#define ISGRAPH_ASCII(CH)	(IS_ASCII(CH) && ISGRAPH(CH))

#ifdef ISLOWER
#undef ISLOWER
#endif
#define ISLOWER	islower
#define ISLOWER_ASCII(CH)	(IS_ASCII(CH) && ISLOWER(CH))

#ifdef ISPRINT
#undef ISPRINT
#endif
#define ISPRINT	isprint
#define ISPRINT_ASCII(CH)	(IS_ASCII(CH) && ISPRINT(CH))

#ifdef ISPUNCT
#undef ISPUNCT
#endif
#define ISPUNCT	ispunct
#define ISPUNCT_ASCII(CH)	(IS_ASCII(CH) && ISPUNCT(CH))

#ifdef ISSPACE
#undef ISSPACE
#endif
#define ISSPACE	isspace
#define ISSPACE_ASCII(CH)	(IS_ASCII(CH) && ISSPACE(CH))

#ifdef ISUPPER
#undef ISUPPER
#endif
#define ISUPPER	isupper
#define ISUPPER_ASCII(CH)	(IS_ASCII(CH) && ISUPPER(CH))

#ifdef ISXDIGIT
#undef ISXDIGIT
#endif
#define ISXDIGIT isxdigit
#define ISXDIGIT_ASCII(CH)	(IS_ASCII(CH) && ISXDIGIT(CH))

#ifdef TOLOWER
#undef TOLOWER
#endif
#define TOLOWER	tolower

#ifdef TOUPPER
#undef TOUPPER
#endif
#define TOUPPER	toupper

#if defined(__osf__) && defined(__alpha)

#pragma pointer_size (restore)

#endif

#endif
