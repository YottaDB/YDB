/****************************************************************
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

#include "mdef.h"

#include "libyottadb_int.h"
#include "mdq.h"

GBLREF	GPCallback	go_panic_callback;
GBLREF	sig_pending	sigPendingFreeQue;	/* Queue of free pending signal blocks - should be a short queue */
GBLREF	sig_pending	sigPendingQue;		/* Queue of pending signals handled in alternate signal handling mode */
GBLREF	sig_pending	sigInProgressQue;	/* Anchor for queue of signals being processed */

/* This routine initializes alternate signal handling in the YottaDB engine. Currently, the only language that is supported with
 * this signal handling method is Go. This routine is not intended for use (call) by user code.
 */
int ydb_main_lang_init(int langid, void *parm)
{

	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	switch(langid)
	{	/* Add new languages as needed */
		case YDB_MAIN_LANG_GO:			/* Currently, only the Go wrapper is supported through this interface.
							 * Other languages should use ydb_init().
							 */
			ydb_main_lang = langid;
			go_panic_callback = parm;	/* Parm for Go is the address of a callback routine to drive a panic */
			DQINIT(&sigPendingFreeQue, que); /* Initialize signal handling queue headers */
			DQINIT(&sigPendingQue, que);
			DQINIT(&sigInProgressQue, que);
			break;
		default:				/* We are uninitialized and don't hold an engine lock so cannot run
							 * rts_error().
							 */
			return YDB_ERR_INVMAINLANG;
	}
	return ydb_init();				/* Now do the rest of (normal) initialization */
}
