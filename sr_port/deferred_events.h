/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef DEFERRED_EVENTS_INCLUDED
#define DEFERRED_EVENTS_NCLUDED

<<<<<<< HEAD
#ifndef DEFERRED_EVENTS_included
#define DEFERRED_EVENTS_included

=======

#ifdef DEBUG
>>>>>>> 52a92dfd (GT.M V7.0-001)
/* Uncomment below to enable tracing of deferred events */
/* #define DEBUG_DEFERRED_EVENT	*/
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
/*  Prototypes for transfer table callback functions, only called by routine that manages xfer_table. */
boolean_t xfer_set_handlers(int4, int4 param, boolean_t popped_entry);
/* Reset transfer table to normal settings.
 * Puts back most things back that could have been changed, excepting timeouts waiting for a jobinterrupt to complete
 * Return value indicates success/failure representing whether the type of reset is the same as event type of set.
 */
boolean_t xfer_reset_handlers(int4 event_type);
/* This version resets the handlers only if they were set by the same event type. */
boolean_t xfer_reset_if_setter(int4 event_type);
<<<<<<< HEAD

/* -------------------------------------------------------
 * Prototypes for transfer table callback functions.
 * Only called by routine that manages xfer_table.
 *
 * Should use these (with enums?) to ensure type checking
 * is done.
 * -------------------------------------------------------
 */

void ctrap_set(int4);
void ctrlc_set(int4);
void ctrly_set(int4);
void tt_write_error_set(int4);

/* ------------------------------------------------------------------
 * Perform action corresponding to the first async event that
 * was logged.
 * ------------------------------------------------------------------
 */
void async_action(bool);
boolean_t xfer_table_changed(void);

#define IS_VALID_TRAP ((set_fn == (&ctrlc_set)) || (set_fn == (&ctrap_set)) || (set_fn == (&ctrly_set)))

#endif /* DEFERRED_EVENTS_included */
=======
/* This version resets a handler */
boolean_t real_xfer_reset(int4 event_type);
#endif /* DEFERRED_EVENTS_INCLUDED */
>>>>>>> 52a92dfd (GT.M V7.0-001)
