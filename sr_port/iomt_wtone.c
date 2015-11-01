/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "io.h"

GBLREF boolean_t gtm_utf8_mode;

void iomt_wtone(int ch)
{
	mstr	temp;
	char	c;

	c = (int)ch;
	temp.len = 1;
	temp.addr = &c;
	iomt_write(&temp);
	return;
}
