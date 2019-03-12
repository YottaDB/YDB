/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2019 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
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

#include "libyottadb.h"

/* Allow this many nested callin levels before generating an error.  Previously, the code
 * allowed a number of condition handlers per call-in invocation, but when we implemented
 * triggers they count as an invocation and caused varying behavior that made testing the limit
 * problematic, so we picked an arbitrary limit (10) that seems generous. */
#define CALLIN_MAX_LEVEL	10

#define CALLIN_HASHTAB_SIZE	32

#define SET_CI_ENV(g)				\
{						\
	frame_pointer->type |= SFT_CI; 		\
	frame_pointer->ctxt = GTM_CONTEXT(g);	\
	frame_pointer->mpc = CODE_ADDRESS(g);	\
}

/* Macro that allows temp_fgncal_stack to override fgncal_stack. This is used in gtm_init (in gtmci.c)
 * during creation of a new level to solve chicken and egg problem that old fgncal_stack needs to be
 * saved but cannot be put into an mv_stent until after the level is actually created. This temp serves
 * that purpose so only has a non-NULL value for the duration of level creation.
 */
#define FGNCAL_STACK ((NULL == TREF(temp_fgncal_stack)) ? fgncal_stack : TREF(temp_fgncal_stack))

/* Call-ins uses the fgncal_stack global as a marker in the stack for where to unwind the stack back to. This preserves
 * the call-in base frame(s) but removes any other frames left on the stack as well as the parameter related mv_stents
 * and any other mv_stents no longer needed. This macro saves the current value of fgncal_stack on the M stack in an
 * MVST_STCK_SP type mv_stent. Note MVST_STCK_SP is chosen (instead of MVST_STCK) because MVST_STCK_SP doesn't get removed
 * if the frame is rewritten by a ZGOTO for instance.
 */
# define SAVE_FGNCAL_STACK								\
{											\
	if (msp != fgncal_stack)							\
	{										\
		push_stck(fgncal_stack, 0, (void **)&fgncal_stack, MVST_STCK_SP);	\
		fgncal_stack = msp;							\
	}										\
}

void	ci_ret_code_quit(void);
void	gtmci_isv_save(void);
void	gtmci_isv_restore(void);
void	ydb_nested_callin(void);
int 	ydb_ci_exec(const char *c_rtn_name, void *callin_handle, int populate_handle, va_list var, boolean_t internal_use);
#ifdef _AIX
void	gtmci_cleanup(void);
#endif

void gtm_levl_ret_code(void);

#include "gtmxc_types.h"	/* for "gtm_status_t" */

gtm_status_t	gtm_ci_filter(const char *c_rtn_name, ...);

int		ydb_cip_helper(int calltyp, ci_name_descriptor *ci_info, va_list *var);

#endif
