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

#include "mdef.h"
#include "cmidef.h"
#include "cmmdef.h"

GBLREF	relque action_que;

void gtcml_lkstarve(cnx)
connection_struct *cnx;
{
	if (((connection_struct *)RELQUE2PTR(cnx->qent.fl))->qent.bl + cnx->qent.fl != 0)
	{	insqt(cnx,&action_que);
		cnx->new_msg = FALSE;
	}
}
