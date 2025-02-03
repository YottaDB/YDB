/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef DEFERRED_EVENTS_INCLUDED
#define DEFERRED_EVENTS_INCLUDED

#ifdef DEBUG
/* Uncomment below to enable tracing of deferred events */
/* #define DEBUG_DEFERRED_EVENT */
#endif
#ifdef DEBUG_DEFERRED_EVENT
GBLREF	volatile int4		fast_lock_count;
#define DBGDFRDEVNT(x) if (0 == fast_lock_count) DBGFPF(x)	/* fast_lock_count check prevents unsafe output */
#include "gtmio.h"
#include "io.h"
#include "gtm_time.h"
/* If debugging timeout deferral, it is helpful to timestamp the messages. Encapsulate our debugging macro with
 * enough processing to be able to do that.
 */
GBLREF char	asccurtime[10];
#  define SHOWTIME(ASCCURTIME)								\
MBSTART {										\
	time_t		CT;								\
	struct tm	*TM_STRUCT;							\
	size_t		LEN;								\
											\
	CT = time(NULL);								\
	GTM_LOCALTIME(TM_STRUCT, &CT);							\
	STRFTIME(ASCCURTIME, SIZEOF(ASCCURTIME), "%T", TM_STRUCT, LEN);			\
} MBEND
#else
#define DBGDFRDEVNT(x)
#define SHOWTIME(ASCCURTIME)
#endif

/* ------------------------------------------------------------------
 * Sets up transfer table changes needed for:
 *   - Synchronous handling of asynchronous events.
 *   - Single-stepping and breakpoints
 * Return value indicates success (e.g. if first to attempt).
 *
 * Notes:
 *   - mdb_condition_handler is different.
 *     Should change it to use this function (CAREFULLY!).
 *   - So are routines related to zbreak and zstep.
 *     ==> Need to update them too (also carefully -- needs
 *         a thorough redesign or rethinking).
 * ------------------------------------------------------------------
 */
/* To prevent GTMSECSHR from pulling in the function xfer_set_handlers currently used in gtm_malloc_src.h and gtm_test_alloc.c,
 * and in turn the entire event codebase, we define a function-pointer variable and initialize it at startup to NULL only in
 * GTMSECSHR and thereby not pull in other unneeded / unwanted executables.
 */
boolean_t	xfer_set_handlers(int4 event_type, int4 param, boolean_t popped_entry);
typedef	boolean_t	(*xfer_set_handlers_fnptr_t)(int4 event_type, int4 param, boolean_t popped_entry);
GBLREF	xfer_set_handlers_fnptr_t	xfer_set_handlers_fnptr;	/* see comment above about this typedef */
/* other prototypes for transfer table callback functions, only called by routine that manages xfer_table. */
/* Reset transfer table to normal settings.
 * Puts back most things back that could have been changed, excepting timeouts waiting for a jobinterrupt to complete
 * Return value indicates success/failure representing whether the type of reset is the same as event type of set.
 */
/* This version resets the handlers only if they were set by the same event type. */
boolean_t xfer_reset_if_setter(int4 event_type);
/* This version resets a handler */
boolean_t real_xfer_reset(int4 event_type);
#endif /* DEFERRED_EVENTS_INCLUDED */
