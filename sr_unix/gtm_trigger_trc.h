/****************************************************************
 *								*
 *	Copyright 2010, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_TRIGGER_TRC_H
#define GTM_TRIGGER_TRC_H

/* Provide some tracing for triggers but due to the somewhat heavy requirements for include
 * gtm_trigger.h, these have been moved out to this supplemental header file with no
 * pre-reqs.
 * Debugging macros for triggers. In open code so are always defined (at least as null).
 * Uncomment below include and define to enable debugging macros (and set GTM_TRIGGER of course)
 */
/* #include "have_crit.h" for DBGFPF/FFLUSH/INTRPT_IN_FFLUSH */
/* #define DEBUG_TRIGR */
#if defined(DEBUG_TRIGR) && defined(GTM_TRIGGER)
# define DBGTRIGR(x) DBGFPF(x)
# define DBGTRIGR_ONLY(x) x
# include "gtm_stdio.h"
# include "gtmio.h"
#else
# define DBGTRIGR(x)
# define DBGTRIGR_ONLY(x)
#endif

#endif
