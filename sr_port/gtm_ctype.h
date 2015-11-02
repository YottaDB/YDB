/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* gtm_ctype.h - interlude to <ctype.h> system header file.	*/

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

#ifndef GTM_CTYPEH
#define GTM_CTYPEH

#include <ctype.h>

#ifdef ISALNUM
#undef ISALNUM
#endif
#define ISALNUM	isalnum

#ifdef ISALPHA
#undef ISALPHA
#endif
#define ISALPHA	isalpha

#ifdef ISCNTRL
#undef ISCNTRL
#endif
#define ISCNTRL	iscntrl

#ifdef ISDIGIT
#undef ISDIGIT
#endif
#define ISDIGIT	isdigit

#ifdef ISGRAPH
#undef ISGRAPH
#endif
#define ISGRAPH	isgraph

#ifdef ISLOWER
#undef ISLOWER
#endif
#define ISLOWER	islower

#ifdef ISPRINT
#undef ISPRINT
#endif
#define ISPRINT	isprint

#ifdef ISPUNCT
#undef ISPUNCT
#endif
#define ISPUNCT	ispunct

#ifdef ISSPACE
#undef ISSPACE
#endif
#define ISSPACE	isspace

#ifdef ISUPPER
#undef ISUPPER
#endif
#define ISUPPER	isupper

#ifdef ISXDIGIT
#undef ISXDIGIT
#endif
#define ISXDIGIT isxdigit

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
