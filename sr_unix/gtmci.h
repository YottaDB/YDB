/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTMCI_H
#define GTMCI_H
#include <stdarg.h>

#include "mdef.h"

#define GTM_CIMOD       "GTM$CI" /* base call-in frame at level 1 */
/* Allow this many nested callin levels before generating an error.  Previously, the code
 * allowed a number of condition handlers per call-in invocation, but when we implemented
 * triggers they count as an invocation and caused varying behavior that made testing the limit
 * problematic, so we picked an arbitrary limit (10) that seems generous. */
#define CALLIN_MAX_LEVEL	10

#define CALLIN_HASHTAB_SIZE	32

#define SET_CI_ENV(g)								\
{										\
	frame_pointer->flags = SFF_CI; 						\
	frame_pointer->old_frame_pointer->ctxt = GTM_CONTEXT(g);		\
	IA64_ONLY(frame_pointer->old_frame_pointer->mpc = CODE_ADDRESS_C(g);)	\
	NON_IA64_ONLY(frame_pointer->old_frame_pointer->mpc = CODE_ADDRESS(g);)	\
}

void	ci_restart(void);
void	ci_ret_code(void);
void	ci_ret_code_exit(void);
void	ci_ret_code_quit(void);
void	gtmci_isv_save(void);
void	gtmci_isv_restore(void);
rhdtyp 	*make_cimode(void);
int 	gtm_ci_exec(const char *c_rtn_name, void *callin_handle, int populate_handle, va_list var);
#ifdef _AIX
void	gtmci_cleanup(void);
#endif

#endif
