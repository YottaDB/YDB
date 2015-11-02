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

#include "mdef.h"

#include "gtm_stdio.h"
#include <rtnhdr.h>
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

GBLREF stack_frame		*frame_pointer;
GBLREF spdesc			stringpool;
GBLREF spdesc			rts_stringpool;
GBLREF unsigned short		proc_act_type;
GBLREF volatile boolean_t	dollar_zininterrupt;
GBLREF mval			dollar_zstatus;
GBLREF dollar_ecode_type	dollar_ecode;			/* structure containing $ECODE related information */

error_def(ERR_ERRWZINTR);

/* Counterpart to trans_code_cleanup for job interrupt errors */
void jobinterrupt_process_cleanup(void)
{
	stack_frame	*fp;
	unsigned char	msgbuf[OUT_BUFF_SIZE], *mbptr;
	unsigned char	*zstptr;
	mstr		msgbuff;
	int		zstlen;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert((SFT_COUNT | SFT_ZINTR) == proc_act_type);
	assert(dollar_zininterrupt);
	if (TREF(compile_time))
	{	/* Make sure we are using the right stringpool */
		TREF(compile_time) = FALSE;
		if (stringpool.base != rts_stringpool.base)
			stringpool = rts_stringpool;
	}
	proc_act_type = 0;
	/* Note, no frames are unwound as in trans_code_cleanup() because we should be
	   able to resume exactly where we left off since (1) it was not an error that
	   caused us to drive the $zinterrupt handler and (2) we were already on a nice
	   statement boundary when we were called to run $zinterrupt.
	*/
	TREF(transform) = TRUE;
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
	{	/* Was a direct mode frame this message needs to go out to the console */
		dec_err(VARLSTCNT(1) ERR_ERRWZINTR);
	}
}
