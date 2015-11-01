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

GBLREF	relque	action_que;

gtcm_write_ast(c)
struct CLB *c;
{	void gtcm_read_ast();

if (c->ios.status != CMI_IO_ACCEPT)
{	/* error */
	*c->mbf = CMMS_E_TERMINATE;
	c->ast = 0;
	insqt(c->usr,&action_que);
	return;
}
if (((connection_struct *)c->usr)->state != CM_NOOP)
{	c->ast = gtcm_read_ast;
/*	cmi_read(c); */
}
return;
}
