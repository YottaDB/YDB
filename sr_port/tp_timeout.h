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

#ifndef __TP_TIMEOUT_H__
#define __TP_TIMEOUT_H__

/* Note: comments need to be updated, and possibly prototypes */

/*
 * --------------------------------------------------------
 * Externally available declarations for
 * transaction processing timeouts
 * --------------------------------------------------------
 */

/* --------------------------------------------
 * Routines to perform state transitions
 * --------------------------------------------
 */

/* ------------------------------------------------------------------
 * Start timer (Clear -> Set)
 * ------------------------------------------------------------------
 */
void tp_start_timer(int4 timer_seconds);

/* ------------------------------------------------------------------
 * Transaction done, clear any pending timeout:
 *     (Set -> Clear)
 *     (Expired -> Clearing -> Clear)
 * Valid even if no timeout was set.
 * ------------------------------------------------------------------
 */
void tp_clear_timeout(void);

/*
 * Used in transfer table for signaling exception
 */
void tp_timeout_action(void);

#endif
