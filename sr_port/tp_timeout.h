/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef TP_TIMEOUT_H_INCLUDED
#define TP_TIMEOUT_H_INCLUDED

/* Uncomment below to enable tptimeout defer trace */
/* #define DEBUG_TPTIMEOUT_DEFERRAL */
#ifdef DEBUG_TPTIMEOUT_DEFERRAL
#  include "io.h"	/* Defines flush_pio */
# define DBGTPTDFRL(x) DBGFPF(x)
#else
# define DBGTPTDFRL(x)
#endif

/* Routines to perform state transitions */

/* Start timer (Clear -> Set) */
void tp_start_timer(int4 timer_milliseconds);

/* Transaction done, clear any pending timeout:
 *     (Set -> Clear)
 *     (Expired -> Clearing -> Clear)
 * Valid even if no timeout was set.
 */
void tp_clear_timeout(boolean_t clear_in_timed_tn);

/* Used in transfer table for signaling exception */
void tp_timeout_action(void);
void tp_expire_now(void);
void tptimeout_set(int4 dummy_param);

#endif
