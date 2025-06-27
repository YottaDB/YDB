/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "stack_frame.h"
#include "stringpool.h"
#include "objlabel.h"
#include "cache.h"
#include "dm_setup.h"
#include "error.h"
#include "error_trap.h"
#include "util.h"
#include "gtm_string.h"
#include "gtmmsg.h"
#include "jobinterrupt_process_cleanup.h"
#include "have_crit.h"
#include "deferred_events_queue.h"
#include "deferred_events.h"
#include "try_event_pop.h"

GBLREF boolean_t		ztrap_explicit_null;		/* whether $ZTRAP was explicitly set to NULL in this frame */
GBLREF mval			dollar_zstatus;
GBLREF spdesc			indr_stringpool, rts_stringpool, stringpool;
GBLREF unsigned short		proc_act_type;
GBLREF stack_frame		*frame_pointer;
GBLREF dollar_ecode_type	dollar_ecode;			/* structure containing $ECODE related information */
GBLREF volatile boolean_t	dollar_zininterrupt;
GBLREF volatile int4		outofband;
GBLREF boolean_t		tref_transform;

error_def(ERR_ERRWZINTR);

/* Counterpart to trans_code_cleanup for job interrupt errors */
void jobinterrupt_process_cleanup(void)
{
	int		zstlen;
	mstr            msgbuff;
	stack_frame	*fp;
	unsigned char	msgbuf[OUT_BUFF_SIZE], *mbptr;
	unsigned char	*zstptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert((SFT_COUNT | SFT_ZINTR) == proc_act_type);
	assert(dollar_zininterrupt);
	if (TREF(compile_time))
		TREF(compile_time) = FALSE;
	if (stringpool.base != rts_stringpool.base)
	{	/* Make sure we are using the right stringpool */
		indr_stringpool = stringpool;
		stringpool = rts_stringpool;
	}
	proc_act_type = 0;
	/* Note, no frames are unwound as in trans_code_cleanup() because we should be
	   able to resume exactly where we left off since (1) it was not an error that
	   caused us to drive the $zinterrupt handler and (2) we were already on a nice
	   statement boundary when we were called to run $zinterrupt.
	*/
	tref_transform = TRUE;
	dollar_zininterrupt = FALSE;	/* No longer in a $zinterrupt */
	/* Now build message for operator log with the form ERRWZINTR, compiler-error */
	util_out_print(NULL, RESET);
	msgbuff.addr = (char *)msgbuf;
	msgbuff.len = SIZEOF(msgbuf);
	gtm_getmsg(ERR_ERRWZINTR, &msgbuff);
	mbptr = msgbuf + strlen((char *)msgbuf);
	/* Find the beginning of the compiler error (look for "%") */
	zstptr = (unsigned char *)dollar_zstatus.str.addr;
	for (zstlen = dollar_zstatus.str.len; zstlen; zstptr++, zstlen--)
	{
		if ('%' == *zstptr)
			break;
	}
	if (zstlen)
	{	/* If found some message, add it to our operator missive */
		*mbptr++ = ',';
		*mbptr++ = ' ';
		memcpy(mbptr, zstptr, zstlen);
		mbptr += zstlen;
	}
	*mbptr++ = 0;
	util_out_print((caddr_t)msgbuf, OPER);
	if (NULL == dollar_ecode.error_last_b_line)
		dec_err(VARLSTCNT(1) ERR_ERRWZINTR);	/* Was a direct mode frame - this message needs to go out to the console */
	TAREF1(save_xfer_root, jobinterrupt).event_state = not_in_play;
	TRY_EVENT_POP;			/* leaving interrupt and not in error handling, so check for pending timed events */
}
