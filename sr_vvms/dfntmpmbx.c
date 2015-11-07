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
#include <lnmdef.h>
#include "vmsdtype.h"

int4 dfntmpmbx (len, addr)
short	len;
char	*addr;
{
	int4			status;
	int4			ret;
	$DESCRIPTOR		(proc_dir, "LNM$PROCESS_DIRECTORY");
	$DESCRIPTOR		(lnm$tmpmbx, "LNM$TEMPORARY_MAILBOX");
	struct
	{
		item_list_3	le[1];
		int4		terminator;
	} item_list;

	item_list.le[0].buffer_length		= len;
	item_list.le[0].item_code		= LNM$_STRING;
	item_list.le[0].buffer_address		= addr;
	item_list.le[0].return_length_address	= &ret;
	item_list.terminator			= 0;

	status = sys$crelnm (0, &proc_dir, &lnm$tmpmbx, 0, &item_list);
	return status;
}
