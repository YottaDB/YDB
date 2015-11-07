/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <jpidef.h>
#include <prvdef.h>
#include <ssdef.h>
#include <efndef.h>

#include "efn.h"
#include "gtmsecshr.h"
#include "vmsdtype.h"
#include "repl_sp.h"

uint4	get_proc_info(uint4 pid, uint4 *time, uint4 *icount)
{
	struct
	{
		item_list_3	item[2];
		int4		terminator;
	} item_list;
	unsigned short	iosb[4], retlen, retlen1;
	uint4		status;
	uint4		prvadr[2], prvprv[2];

	GTMSECSHR_SET_PRIV(PRV$M_WORLD, status);
	if (SS$_NORMAL == status)
	{
		item_list.item[0].buffer_length		= SIZEOF(*icount);
		item_list.item[0].item_code		= JPI$_IMAGECOUNT;
		item_list.item[0].buffer_address	= icount;
		item_list.item[0].return_length_address = &retlen1;
		if (NULL != time)
		{
			item_list.item[1].buffer_length		= SIZEOF(uint4) * 2;
			item_list.item[1].item_code		= JPI$_LOGINTIM;
			item_list.item[1].buffer_address	= time;
			item_list.item[1].return_length_address = &retlen;
		} else	/* from is_proc_alive, no need for LOGINTIM which may require an INSWAP */
		{	/* each of these is a short */
			item_list.item[1].buffer_length		= 0;
			item_list.item[1].item_code		= 0;
		}
		item_list.terminator			= 0;
		status = sys$getjpiw(EFN$C_ENF, &pid, NULL, &item_list, iosb, NULL, 0);
		if (SS$_NORMAL == status)
			status = iosb[0];
		GTMSECSHR_REL_PRIV;
	}
	return status;
}
