/****************************************************************
 *								*
 * Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
/* Not an FIS authored routine but contains code inspired by FIS code */

#include "mdef.h"

#include "gtm_string.h"

#include "error.h"
#include "libyottadb_int.h"
#include "gtmmsg.h"

/* Routine to fetch a message from the error message library associated with the provided
 * error code/status and store it in the supplied buffer.
 */
int ydb_message(int errnum, ydb_buffer_t *msg_buff)
{
	boolean_t	error_encountered;
	int		l_x, status;
	mstr		msg;
	char		*cp, *cptop;
	char		msgbuf[YDB_MAX_ERRORMSG];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VERIFY_NON_THREADED_API;	/* clears a global variable "caller_func_is_stapi" set by SimpleThreadAPI caller
					 * so needs to be first invocation after SETUP_THREADGBL_ACCESS to avoid any error
					 * scenarios from not resetting this global variable even though this function returns.
					 */
	LIBYOTTADB_INIT(LYDB_RTN_MESSAGE, (int));	/* Note: macro could "return" from this function in case of errors */
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* Previously unused entries should have been cleared by that
							 * corresponding ydb_*_s() call.
							 */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{
		assert(0 == TREF(sapi_query_node_subs_cnt));	/* Should have been cleared by "ydb_simpleapi_ch" */
		assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* Should have been cleared by "ydb_simpleapi_ch" */
		REVERT;
		return  -(TREF(ydb_error_code));
	}
	l_x = errnum;
	/* If user supplied a return code from a simpleAPI call, it could be negative. Handle that here. */
	if (0 > l_x)
		l_x = -l_x;
	msg.len = SIZEOF(msgbuf);
	msg.addr = msgbuf;
	status = gtm_getmsg(l_x, &msg);			/* Writes result to our buffer */
	if (ERR_UNKNOWNSYSERR == status)		/* Message loopup failed */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_UNKNOWNSYSERR, 1, l_x);
	if (NULL == msg_buff)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			LEN_AND_LIT("NULL msg_buff"), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_MESSAGE)));
	/* Copy message to user's buffer if there is room */
	msg_buff->len_used = msg.len;
	if ((unsigned)msg.len > msg_buff->len_alloc)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVSTRLEN, 2, msg.len, msg_buff->len_alloc);
	if (msg.len)
	{
		if (NULL == msg_buff->buf_addr)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6)
				ERR_PARAMINVALID, 4, LEN_AND_LIT("NULL msg_buff->buf_addr"),
				LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_MESSAGE)));
		memcpy(msg_buff->buf_addr, msg.addr, msg.len);
	}
	LIBYOTTADB_DONE;
	REVERT;
	return YDB_OK;
}
