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

/*
 * --------------------------------------------------------------
 * Read AST handler
 *
 * void gtcm_read_ast(c)
 * struct CLB *c;
 *	This handler is established by gtcm_init_ast upon
 *	detection of request for a new connection, which
 *	occured upon a receipt of cmj_mbox_ast AST, which
 *	was setup by the cmj_mbx_read_start, called by cm_init()
 *	from gtcm_server.
 * --------------------------------------------------------------
 */
#include <stropts.h>

#include "gtm_stdio.h"

#include "mdef.h"
#include "cmidef.h"
#include "cmmdef.h"

GBLREF	relque	action_que;

void gtcm_read_ast(c)
struct CLB *c;
{
/*
 * If status is NOT normal (successfull)
 */

	if (c->ios.status  != CMI_IO_ACCEPT)
	{
			/* If protocol error, retry */
		if (c->ios.status == (unsigned short)S_PROTOCOL)
		{
				/* Perform actual read */
			cmi_read(c);
			return;
		}
			/* If not a protocol error, terminate */
		*c->mbf = CMMS_E_TERMINATE;
	}
/*
 * Successful read status of I/O operation, or data overrun.
 * Insert a connection descriptor structure for this connection
 * into the action que to be processed by the gtcm_server's
 * main loop.
 */
#ifdef DP
fprintf(stderr,"--> 3: inserting in queue\n");
#endif
	insqt(c->usr,&action_que);
	((connection_struct *)c->usr)->new_msg = TRUE;
}

