/****************************************************************
 *								*
 * Copyright (c) 2015 Fidelity National Information 		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef UTFCGR_TRC_INCLUDED
#define UTFCGR_TRC_INCLUDED

/* Debugging macros for UTF8 caching. These are separate from other macros so they can be
 * included regardless of whether UNICODE_SUPPORTED is set or not.
 *
 * Uncomment below define to enable debugging macros
 */

/* #define DEBUG_UTF8CACHE */
#if defined(UNICODE_SUPPORTED) && defined(DEBUG_UTF8CACHE)
# define DBGUTFC(x) DBGFPF(x)
# define DBGUTFC_ONLY(x) x
# include "gtm_stdio.h"
# include "gtmio.h"
# include "have_crit.h" /* For DBGFPF/FFLUSH/INTRPT_IN_FFLUSH */
#else
# define DBGUTFC(x)
# define DBGUTFC_ONLY(x)
#endif

#endif
