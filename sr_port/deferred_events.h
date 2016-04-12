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

/* Uncomment below to enable tracing of deferred events */
/* #define DEBUG_DEFERRED_EVENT */
#ifdef DEBUG_DEFERRED_EVENT
# define DBGDFRDEVNT(x) DBGFPF(x)
#else
# define DBGDFRDEVNT(x)
#endif

/* --------------------------------------------
 * Async. events that can be deferred
 * --------------------------------------------
 */
enum deferred_event
{
	no_event = 0,
	outofband_event,
	network_error_event,
	zstp_or_zbrk_event,
	tt_write_error_event,
	DEFERRED_EVENTS
};

/* =============================================================================
 * EXPORTED VARIABLES
 * =============================================================================
 */

/* -------------------------------------------------------
 * - Used by the substitute xfer_table functions.
 * - Keeping_count => should change name to num_logged.
 * - Should be made static and wrapped with a function for weaker
 * intermodule coupling; however, it's accessed from assembly
 * language => not worth the trouble to add a layer.
 * -------------------------------------------------------
 */
#if defined(UNIX)
GBLREF volatile int4	num_deferred;
#elif defined(VMS)
GBLREF volatile short	num_deferred;
#else
# error "Unsupported Platform"
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
boolean_t xfer_set_handlers(int4, void (*callback)(int4), int4 param);

/* ------------------------------------------------------------------
 * Reset transfer table to normal settings.
 *
 * Puts back everything that could/would have been changed, assuming
 * that no xfer_table changes have been added since it was written.
 *
 * Return value indicates success/failure. Succeeds only if event
 * type of reset is the same as event type of set.
 * ------------------------------------------------------------------
 */
boolean_t xfer_reset_handlers(int4 event_type);

/* ------------------------------------------------------------------
 * This version resets the handlers only if they were set by the
 * same event type.
 * ------------------------------------------------------------------
 */
boolean_t xfer_reset_if_setter(int4 event_type);

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
