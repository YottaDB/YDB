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
#include <ssdef.h>
#include <psldef.h>
#include <secdef.h>
#include <descrip.h>
#include <prvdef.h>
#include "del_sec.h"

int del_sec(uint4 flags,struct dsc$descriptor *gdsnam,char *ident)
{
int4 status;
uint4	prvadr[2], prvprv[2];

prvadr[1] = 0;
prvadr[0] = PRV$M_SYSGBL | PRV$M_PRMGBL;
status = sys$setprv(TRUE, &prvadr[0], FALSE, &prvprv[0]);
if (status == SS$_NORMAL)
{	status = sys$dgblsc(flags,gdsnam,ident);
}
if (~prvprv[0] & (PRV$M_SYSGBL | PRV$M_PRMGBL))
{	prvprv[0] = ~prvprv[0];
	prvprv[1] = ~prvprv[1];
	sys$setprv(FALSE, &prvprv[0], FALSE, 0);
}
return status;
}
