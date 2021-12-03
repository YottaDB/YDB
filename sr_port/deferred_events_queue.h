/****************************************************************
 *								*
 * Copyright (c) 2018-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef DEFERRED_EVENTS_QUEUE_INCLUDED
#define DEFERRED_EVENTS_QUEUE_INCLUDED

#include "gtm_signal.h"
#include "op.h"				/* needed by outofband resolution of set event functions op_zstep and op_setzbrk */
#include "gvcmz_neterr.h"			/* needed by outofband resolution of gtcmz_net_error */

#define MAXQUEUELOCKWAIT		10000	/* 10sec  = 10000 1-msec waits */

GBLREF  boolean_t			blocksig_initialized;
GBLREF  sigset_t			block_sigsent;

#define OUTOFBAND_MSK	0x02000018
#define CTRLC_MSK	0x00000008
#define SIGHUP_MSK	0x00000010
#define CTRLC     3
#define CTRLD     4
#define CTRLY	  25
#define MAXOUTOFBAND 31

#define OUTOFBAND_RESTARTABLE(event)	(jobinterrupt == (event))

void outofband_action(boolean_t line_fetch_or_start);

/* ------------------------------------------------------------------
 * Perform action corresponding to the first async event that
 * was logged.
 * ------------------------------------------------------------------
 */
void ctrap_set(int4);
void ctrlc_set(int4);
void jobinterrupt_set(int4 dummy);
void tptimeout_set(int4 dummy_param);		/* Used to setup tptimeout error via out-of-band */
void ztimeout_set(int4 dummy_param);
void tt_write_error_set(int4);
void async_action(bool);
void outofband_clear(void);

#define D_EVENT(a,b) a
enum outofbands
{
#include "outofband.h"
};
#undef D_EVENT

enum event_in_play
{
	not_in_play,		/* 0 */
	signaled, 		/* 1 */
	queued,			/* 2 */
	pending,		/* 3 */
	active,			/* 4 */
	num_event_states	/* 5 */
};

typedef struct save_xfer_entry_struct
{
	struct
	{
		struct save_xfer_entry_struct	*fl,
						*bl;
	} ev_que;
	void		(*set_fn)(int4 param);
	int4		outofband;
	int4		param_val;
	volatile int4	event_state;
} save_xfer_entry;

void save_xfer_queue_entry(int4  event_type, int4 param_val);
void pop_real_xfer_queue_entry(int4* event_type, int4* param_val);
void pop_xfer_queue_entry(int4* event_type, int4* param_val);
void remove_xfer_queue_entry(int4 event_type);
void scan_xfer_queue_entries(boolean_t check4players);
void empty_xfer_queue_entries(void);
void set_events_from_signals(intrpt_state_t prev_intrpt_state);

#define SAVE_XFER_QUEUE_ENTRY(EVENT_TYPE, PARAM_VAL)				\
MBSTART {									\
		assert((INTRPT_IN_EVENT_HANDLING == intrpt_ok_state) 		\
				|| (multi_thread_in_use));			\
		save_xfer_queue_entry(EVENT_TYPE, PARAM_VAL);			\
} MBEND


#define POP_XFER_QUEUE_ENTRY(EVENT_TYPE, PARAM_VAL)				\
MBSTART {									\
		assert(INTRPT_IN_EVENT_HANDLING == intrpt_ok_state);		\
		pop_xfer_queue_entry(EVENT_TYPE, PARAM_VAL);			\
} MBEND

#define REMOVE_XFER_QUEUE_ENTRY(ID)						\
MBSTART {									\
		assert(INTRPT_IN_EVENT_HANDLING == intrpt_ok_state);		\
		remove_xfer_queue_entry(ID);					\
} MBEND

#define EMPTY_XFER_QUEUE_ENTRIES						\
MBSTART {									\
		assert(INTRPT_IN_EVENT_HANDLING == intrpt_ok_state);		\
		empty_xfer_queue_entries();					\
} MBEND

/* while the other macros check the protection state coming in, this one checks for proper cleanup going out */
#define ENABLE_EVENT_INTERRUPTS(PREV_INTRPT_STATE)				\
MBSTART {									\
		set_events_from_signals(PREV_INTRPT_STATE);			\
		assert(INTRPT_IN_EVENT_HANDLING != intrpt_ok_state);		\
} MBEND
#endif /* DEFERRED_EVENTS_QUEUE_INCLUDED */

#define TRY_EVENT_POP									\
MBSTART {										\
	int4		event_type, param_val;						\
	intrpt_state_t	prev_intrpt_state;						\
											\
	assert(INTRPT_IN_EVENT_HANDLING != intrpt_ok_state);				\
	DEFER_INTERRUPTS(INTRPT_IN_EVENT_HANDLING, prev_intrpt_state);			\
	if ((no_event == outofband)							\
		&& (no_event != (TREF(save_xfer_root_ptr))->ev_que.fl->outofband))	\
	{	/* perhaps pop an_event */						\
		POP_XFER_QUEUE_ENTRY(&event_type, &param_val);				\
		if (no_event != event_type)						\
			xfer_set_handlers(event_type, param_val, TRUE);			\
		DBGDFRDEVNT((stderr, "%d %s: pop_reset_xfer returned event %d\n",	\
			__LINE__, __FILE__, event_type));				\
	}										\
	ENABLE_EVENT_INTERRUPTS(prev_intrpt_state);					\
} MBEND
