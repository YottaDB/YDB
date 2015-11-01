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
 * -----------------------------------------------------------
 * Network error handling routine.
 * Used in following situations:
 *	- Server received an illegal message type on the
 *	  network level communication link. The only legal
 *	  message types on VMS are MSG$_CONNECT & MSG$_INTMSG.
 * The terminate message is added to the action que.
 * -----------------------------------------------------------
 */
#ifndef __linux__
#include <tiuser.h>
#endif
#include <stropts.h>

#include "mdef.h"
#include "cmidef.h"
#include "cmmdef.h"

GBLREF relque action_que;

/*
 * -------------------------------------------------------------
 * Arguments:
 *	clb	- pointer to CLB for this connection
 * -------------------------------------------------------------
 */
void gtcm_neterr(clb)
struct CLB *clb;
{
	unsigned char msg;
	connection_struct *cnx;

	msg = *(unsigned char *) clb->mbf;

#ifndef __linux__
		 /* Get error message code */
	switch(msg)
	{
	case (T_ERROR):
	case (T_DISCONNECT):
	case (T_ORDREL):
/*
 * If element is still on the list the backward link of the forward link
 * will point to this element
 */
		if (clb && ((struct CLB *)RELQUE2PTR(clb->cqe.fl))->cqe.bl
		  + clb->cqe.fl == 0)
		{
/*			if (*mbp != CMMS_S_TERMINATE) */
			if (clb->mbf[0] != CMMS_S_TERMINATE)
			{
/* 				*mbp = CMMS_E_TERMINATE; */
 				clb->mbf[0] = CMMS_E_TERMINATE;
				clb->mbl = 1;
				cnx = (connection_struct *) clb->usr;
				cnx->state = 0;
/*
 * if this connection structure is not already linked in the action que add it
 */
				if (((connection_struct *)
				  RELQUE2PTR(cnx->qent.fl))->qent.bl
				  + cnx->qent.fl != 0)
				{
					insqt(cnx,&action_que);
				}
			}
		}
	default:
		break;
	}
#endif
}

