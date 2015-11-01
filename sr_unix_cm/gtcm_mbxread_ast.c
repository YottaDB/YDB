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
 * --------------------------------------------------------------------
 *	NET level Interrupt message handler.
 *	This handler is setup by the gtcm_server().
 *	It is dispatched by the cmj_mbx_ast() routine upon receipt
 *	of an Interrrupt Message on the Network level as
 *	(*tsk->mbx_ast)(tsk).
 *
 *	The interrupt message is sent whenever we need to cancell the
 *	lock operation in progress.
 *	In case we were able to obtain some locks before this message
 *	arrives, we need to cancel only those locks, that belong to the
 *	client, identified by his pid. The pid is carried in the message
 *	and is saved in (connection_struct *)->usr->procnum.
 * --------------------------------------------------------------------
 */
#include <stropts.h>
#include "mdef.h"
#include "cmidef.h"
#include "cmmdef.h"
#include "gt_timer.h"

GBLREF relque action_que;
struct CLB *gtcm_find_proc();

gtcm_mbxread_ast(tsk)
struct NTD *tsk;
{
	cm_mbx		*mp;
	unsigned char	*msg;
	unsigned short	procnum;
	struct CLB	*tell;
	connection_struct *cnx;
	unsigned char	*transnum, *laflag;

	error_def(CMERR_INVINTMSG);

/* --- code start --- */

		/* Get the message header (1 byte) */
/* DCK - Do something intelligent with this */
	mp = (cm_mbx *)tsk->mbx.addr;
	msg = &mp->text[0];
	assert(*msg == CMMS_S_INTERRUPT);
	msg++;
/* Get the process number of the client for whom we need to
 * cancell the locks from first 2 bytes of the message buffer
 */
	procnum = *((unsigned short *)msg);
	msg += sizeof(unsigned short);
/*
 * Find the CLB having the process # specified in the message
 */
	tell = gtcm_find_proc(tsk, procnum);
	assert (tell);

	switch(*msg)
	{
	case CMMS_L_LKCANCEL:
		laflag = transnum = msg + 1;
		transnum++;
		cnx = (connection_struct *) tell->usr;
		if (((connection_struct *)
		  RELQUE2PTR(cnx->qent.fl))->qent.bl + cnx->qent.fl
		  != 0)
		{
			cnx->lk_cancel = *transnum;
			memcpy(tell->mbf,msg, S_HDRSIZE + S_LAFLAGSIZE + 1);
/*
 * Link the message into the action que
 */
			insqt(cnx,&action_que);
			cnx->new_msg = FALSE;
			cancel_timer((TID) cnx);
		}
		else
		{
			cnx->lk_cancel = *transnum;
		}
		break;

	default:
		dec_err(VARLSTCNT(1) CMERR_INVINTMSG);
#ifdef OLD_VAX_ERR
		err.arg_cnt = MID_MSG_SIZE;
		err.new_opts = err.def_opts = 1;
		err.msg_number = CMERR_INVINTMSG;
		err.fp_cnt = 2;
		sys$putmsg (&err, 0, 0, 0);
#endif
		break;
	}
}

