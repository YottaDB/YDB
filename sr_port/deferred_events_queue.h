/****************************************************************
 *								*
 * Copyright (c) 2018 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "gtm_signal.h"

#define MAXQUEUELOCKWAIT		10000 /* 10sec  = 10000 1-msec waits */

GBLREF  boolean_t			blocksig_initialized;
GBLREF  sigset_t			block_sigsent;
GBLREF	sigset_t			block_sigusr;

typedef struct save_xfer_entry_struct
{
	struct save_xfer_entry_struct	*next;
	int4           			outofband;
	void (*set_fn)(int4 param);
	int4 param_val;
} save_xfer_entry;

void save_xfer_queue_entry(int4  event_type, void (*set_fn)(int4 param), int4 param_val);
void pop_reset_xfer_entry(int4*  event_type, void (**set_fn)(int4 param), int4* param_val);
void remove_queue_entry(void (*set_fn)(int4 param));
save_xfer_entry* find_queue_entry(void (*set_fn)(int4 param), save_xfer_entry **qprev);
void empty_queue(void);

#define SAVE_XFER_ENTRY(EVENT_TYPE, SET_FN, PARAM_VAL)				\
MBSTART {									\
		sigset_t	SAVEMASK;					\
		int		RC;						\
		if (blocksig_initialized)					\
		{								\
			SIGPROCMASK(SIG_BLOCK, &block_sigusr, &SAVEMASK, RC);	\
		}								\
		save_xfer_queue_entry(EVENT_TYPE, SET_FN, PARAM_VAL);		\
		if (blocksig_initialized)					\
		{								\
			SIGPROCMASK(SIG_SETMASK, &SAVEMASK, NULL, RC);		\
		}								\
} MBEND


#define POP_XFER_ENTRY(EVENT_TYPE, SET_FN, PARAM_VAL)				\
MBSTART {									\
		sigset_t	SAVEMASK;					\
		int		RC;						\
		if (blocksig_initialized)					\
		{								\
			SIGPROCMASK(SIG_BLOCK, &block_sigusr, &SAVEMASK, RC);	\
		}								\
		pop_reset_xfer_entry(EVENT_TYPE, SET_FN, PARAM_VAL);		\
		if (blocksig_initialized)					\
		{								\
			SIGPROCMASK(SIG_SETMASK, &SAVEMASK, NULL, RC);		\
		}								\
} MBEND

#define REMOVE_QUEUE_ENTRY(ID)							\
MBSTART {									\
		sigset_t	SAVEMASK;					\
		int		RC;						\
		if (blocksig_initialized)					\
		{								\
			SIGPROCMASK(SIG_BLOCK, &block_sigusr, &SAVEMASK, RC);	\
		}								\
		remove_queue_entry(ID);						\
		if (blocksig_initialized)					\
		{								\
			SIGPROCMASK(SIG_SETMASK, &SAVEMASK, NULL, RC);		\
		}								\
} MBEND

#define EMPTY_QUEUE_ENTRIES							\
MBSTART {									\
		sigset_t	SAVEMASK;					\
		int		RC;						\
		if (blocksig_initialized)					\
		{								\
			SIGPROCMASK(SIG_BLOCK, &block_sigusr, &SAVEMASK, RC);	\
		}								\
		empty_queue();						\
		if (blocksig_initialized)					\
		{								\
			SIGPROCMASK(SIG_SETMASK, &SAVEMASK, NULL, RC);		\
		}								\
} MBEND
