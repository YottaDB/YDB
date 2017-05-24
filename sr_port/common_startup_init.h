/****************************************************************
 *								*
 * Copyright (c) 2014-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef _COMMON_STARTUP_INIT_DEFINED
#define _COMMON_STARTUP_INIT_DEFINED

void	common_startup_init(enum gtmImageTypes img_type);

#if (defined(DEBUG) || defined(TIMER_DEBUGGING))
#	include "jnl_file_close_timer.h"
GBLREF void		(*jnl_file_close_timer_ptr)(void);
#	define	INIT_JNL_FILE_CLOSE_TIMER_FNPTR	jnl_file_close_timer_ptr = &jnl_file_close_timer
#else
#	define	INIT_JNL_FILE_CLOSE_TIMER_FNPTR
#endif

#ifdef TIMER_DEBUGGING
#	include "fake_enospc.h"
#	include "gt_timer.h"
GBLREF void		(*fake_enospc_ptr)(void);
GBLREF void		(*simple_timeout_timer_ptr)(TID tid, int4 hd_len, boolean_t **timedout);
#	define	INIT_FAKE_ENOSPC_FNPTR		fake_enospc_ptr = &fake_enospc
#	define	INIT_SIMPLE_TIMEOUT_TIMER_FNPTR	simple_timeout_timer_ptr = &simple_timeout_timer
#else
#	define	INIT_FAKE_ENOSPC_FNPTR
#	define	INIT_SIMPLE_TIMEOUT_TIMER_FNPTR
#endif

#ifdef DEBUG
#	include "error.h"
GBLREF ch_ret_type	(*t_ch_fnptr)();		/* Function pointer to t_ch */
GBLREF ch_ret_type	(*dbinit_ch_fnptr)();		/* Function pointer to dbinit_ch */
#	define	INIT_DBINIT_CH_FNPTR		dbinit_ch_fnptr = &dbinit_ch
#	define	INIT_T_CH_FNPTR			t_ch_fnptr = &t_ch
#else
#	define	INIT_DBINIT_CH_FNPTR
#	define	INIT_T_CH_FNPTR
#endif

#define	INIT_FNPTR_GLOBAL_VARIABLES			\
MBSTART {						\
	INIT_JNL_FILE_CLOSE_TIMER_FNPTR;		\
	INIT_FAKE_ENOSPC_FNPTR;				\
	INIT_SIMPLE_TIMEOUT_TIMER_FNPTR;		\
	INIT_DBINIT_CH_FNPTR;				\
	INIT_T_CH_FNPTR;				\
} MBEND

#endif
