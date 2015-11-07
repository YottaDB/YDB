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
#include <descrip.h>
#include <ssdef.h>
#include <climsgdef.h>
#include "util_spawn.h"

void util_spawn(void)
{

	char buf[256];
	$DESCRIPTOR(d_buf,buf);
	$DESCRIPTOR(d_ent," ");

	d_ent.dsc$a_pointer = "COMMAND";
	d_ent.dsc$w_length = 7;

	if (CLI$PRESENT(&d_ent) == CLI$_PRESENT)
	{
		if (CLI$GET_VALUE(&d_ent,&d_buf) == SS$_NORMAL)
			lib$spawn(&d_buf);
		return;
	}
	d_buf.dsc$w_length = 0;
	lib$spawn(&d_buf);
	return;

}
