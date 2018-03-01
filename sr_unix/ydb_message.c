/****************************************************************
 *								*
 * Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "error.h"
#include "gtmxc_types.h"
#include "gtmmsg.h"

#define SYSERRTAG	"%SYSTEM"
#define UNKWNERRTAG	"Unknown "

/* Routine to fetch a message from the error message library associated with the provided
 * error code/status.
 */
int ydb_message(ydb_int_t status, ydb_buffer_t *msg_buff)
{
	ydb_int_t	l_x;
	mstr		msg;
	char		*cp, *cptop;

	/* Note the value for YDB_MIN_ERROR_BUF_LEN is chosen so that when we parse the returned message, we don't
	 * have to worry about whether or not the buffer is big enough to hold enough of the message for us to
	 * determine if it is an error or not. So the minimum is so the following message can fit in the buffer
	 * (plus a couple extra characters to make the size an even boundary) which let's us definitively way this
	 * is/is-not an error.
	 *
	 *   %SYSTEM-E-UNKNOWN, Unknown
	 */
	if (YDB_MIN_ERROR_BUF_LEN > msg_buff->len_alloc)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MSGBUF2SMALL, 1, YDB_MIN_ERROR_BUF_LEN);
	l_x = status;
	/* If user supplied a return code from a simpleAPI call, it could be negative. Handle that here. */
	if (0 > l_x)
		l_x = -l_x;
	msg.len = msg_buff->len_alloc;
	msg.addr = msg_buff->buf_addr;
	gtm_getmsg(l_x, &msg);		/* Writes result to user buffer */
	/* At this point, we need to know whether we had an invalid error code or not. There's no indication of this
	 * from gtm_getmsg() so we look for some of the following things:
	 *
	 *   1. All unknown codes start with a facility of "SYSTEM" though these can have valid error messages also.
	 *   2. The first word of the message (after the message id and comma) will be "Unknown". No valid messages
	 *      have this word as the first word so we can check for that and call it day.
	 */
	cp = msg.addr;
	if (0 == MEMCMP_LIT(cp, SYSERRTAG))
	{	/* Might be an error, might not - check a bit further for message starting with "Unknown". First,
		 * find the start of the message just past the comma.
		 */
		for (cp += strlen(SYSERRTAG); ',' != *cp; cp++)
		{
			assert(cp < cptop);
		}
		cp++;				/* Past comma to separator space */
		assert(' ' == *cp);
		cp++;				/* Past separator space to start of message */
		cptop = cp + msg.len;
		if ((strlen(UNKWNERRTAG) <= (cptop - cp)) && MEMCMP_LIT(cp, UNKWNERRTAG))
		{
			msg_buff->len_used = msg.len;		/* Finish off message so is usable */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_UNKNOWN);
		}
	} /* else, if not SYSTEM facility, then is not an unknown value */
	msg_buff->len_used = msg.len;
	return YDB_OK;
}
