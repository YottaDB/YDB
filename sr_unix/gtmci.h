/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTMCI_H
#define GTMCI_H

#define SET_CI_ENV(g)						\
{								\
	frame_pointer->flags = SFF_CI; 				\
	frame_pointer->old_frame_pointer->ctxt = CONTEXT(g);	\
	frame_pointer->old_frame_pointer->mpc = CODE_ADDRESS(g);\
}

void	ci_start(void);
void	ci_restart(void);
void	ci_ret_code(void);
void	ci_ret_code_exit(void);
void	save_intrinsic_var(void);

#endif
