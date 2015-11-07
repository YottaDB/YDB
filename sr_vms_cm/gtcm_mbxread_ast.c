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
#include "msg.h"
#include "gtcm_int_unpack.h"

GBLREF relque action_que;
GBLREF connection_struct *curr_entry;
extern long gtcm_action_pending();

gtcm_mbxread_ast(struct NTD *tsk)
{

	cm_mbx *mp;
	unsigned char *msg;
	unsigned short procnum;
	struct CLB *tell;
	connection_struct *cnx;
	msgtype		err;
	unsigned char *transnum, *laflag;
	error_def(CMERR_INVINTMSG);
	struct CLB *gtcm_find_proc();
	void gtcm_write_ast();

	mp = tsk->mbx.dsc$a_pointer;
	msg = &mp->text[0];
	assert(*msg == CMMS_S_INTERRUPT);
	msg++;
	procnum = *((unsigned short *)msg)++;
	tell = gtcm_find_proc(tsk,procnum);
	assert (tell);

	switch(*msg)
	{
		case CMMS_L_LKCANCEL:
				laflag = transnum = msg + 1;
				transnum++;
				cnx = (connection_struct *) tell->usr;
				if (curr_entry == cnx || gtcm_write_ast == tell->ast)
				{    /* store information till end of server loop or write done */
				  cnx->int_cancel.laflag = *laflag | 1;  /* since laflag may be 0, x40, x80 */
				  cnx->int_cancel.transnum = *transnum;
				}
				else
				{   /* not curr_entry and not write pending */
				        cnx->lk_cancel = *transnum;
					memcpy(tell->mbf,msg, S_HDRSIZE + S_LAFLAGSIZE + 1);
					tell->cbl = S_HDRSIZE + S_LAFLAGSIZE + 1;
					(void)gtcm_action_pending(cnx);  /* checks if in queue already */
					cnx->new_msg = FALSE;
					sys$cantim(cnx,0);
				}
				break;
		default:
				err.arg_cnt = MID_MSG_SIZE;
				err.new_opts = err.def_opts = 1;
				err.msg_number = CMERR_INVINTMSG;
				err.fp_cnt = 2;
				sys$putmsg (&err, 0, 0, 0);
				break;
	}
}
