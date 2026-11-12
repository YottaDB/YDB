/****************************************************************
 *								*
 * Copyright 2001 Sanchez Computer Associates, Inc.		*
 *								*
 * Copyright (c) 2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef LCKCLR_INCLUDED
#define LCKCLR_INCLUDED

void lckclr(void);

/* "lks_this_cmd" counts the lock names of the LOCK command (or SimpleAPI lock call) in progress, and the "trans" flag of each of
 * those names is set until the LOCK completes. "lks_this_cmd_fp" is the M frame in which that LOCK started. A LOCK can be left in
 * progress while other code runs, for example an extrinsic that a lock name calls, and the LOCK resumes when that code returns,
 * as long as execution goes on in a frame newer than "lks_this_cmd_fp" (frames have lower addresses the newer they are). Code
 * that makes execution go on in frame CONT_FP (after a ZGOTO, a TP restart or an error, or as a frame is unwound) invokes this
 * macro, which abandons the LOCK in progress unless the LOCK resumes. That clears the "trans" flags (else a later incremental
 * LOCK of one of those names neither counts nor acquires the name) and the count (else the next lock operation issues
 * BADLOCKNEST). A SimpleAPI lock call, or a LOCK command none of whose lock names calls an extrinsic, runs no M code while it
 * locks. So anything that interrupts such a lock call or LOCK makes execution go on in frame "lks_this_cmd_fp" or an older frame,
 * which abandons the lock call or LOCK.
 * op_lkinit() sets "lks_this_cmd_fp" at the start of every LOCK, before any lock name is counted, and "lks_this_cmd_fp" is read
 * only while "lks_this_cmd" is non-zero. So nothing needs to clear "lks_this_cmd_fp".
 * The caller needs GBLREFs for "lks_this_cmd" and "lks_this_cmd_fp".
 */
#define ABANDON_LOCK_CMD_UNLESS_RESUMED(CONT_FP)		\
MBSTART {							\
	if (lks_this_cmd && ((CONT_FP) >= lks_this_cmd_fp))	\
	{							\
		lckclr();					\
		lks_this_cmd = 0;				\
	}							\
} MBEND

#endif /* LCKCLR_INCLUDED */
