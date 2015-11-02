/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTMCI_H
#define GTMCI_H

#include "mdef.h"

#define GTM_CIMOD       "GTM$CI" /* base call-in frame at level 1 */

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
#ifdef _AIX
void	gtmci_cleanup(void);
#endif

#endif
