/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gtcm_action_pending.h"
#include "gtcm_urgread_ast.h"

void gtcm_urgread_ast(struct CLB *cp, unsigned char transnum)
{
	connection_struct	*cnx;

	/* Note. earlier code here tested (curr_entry == cnx || gtcm_write_ast == cp->ast) and
	   if false, built a message in the existing mbf. The thought now is that this could
	   cause a message overlay problem so urgent messages are always flagged and executed
	   later rather than attempting an inline execution.
	*/
	cnx = (connection_struct *) cp->usr;
	/* store information till end of server loop */
	cnx->int_cancel.laflag = 1; 	/* signal pending info */
	cnx->int_cancel.transnum = transnum;
	gtcm_action_pending(cnx);
}
