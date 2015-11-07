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

#include <lkidef.h>
#include <lckdef.h>
#include <prvdef.h>
#include <ssdef.h>
#include <efndef.h>

#include "gtmsecshr.h"
#include "locks.h"
#include "vmsdtype.h"

GBLREF uint4	rundown_process_id;
GBLREF lock_sb	vms_lock_list[MAX_VMS_LOCKS + 1];
GBLREF int	vms_lock_tail;

uint4	gtm_deq(unsigned int lkid, void *valblk, unsigned int acmode, unsigned int flags)
{
	struct
	{
		item_list_3	item[1];
		int4		terminator;
	} item_list;
	unsigned short	iosb[4];
	int		index;
	uint4		lk_pid = 0, retlen, status;
	uint4		prvadr[2], prvprv[2];

	item_list.item[0].buffer_length = SIZEOF(lk_pid);
	item_list.item[0].item_code = LKI$_PID;
	item_list.item[0].buffer_address = &lk_pid;
	item_list.item[0].return_length_address = &retlen;
	item_list.terminator = 0;
	if ((SS$_NORMAL == (status = sys$getlkiw(EFN$C_ENF, &lkid, &item_list, iosb, NULL, 0, 0)))
		&& (lk_pid == rundown_process_id))
	{
		GTMSECSHR_SET_DBG_PRIV(PRV$M_SYSLCK, status);
		if (SS$_NORMAL == status)
		{
			status = sys$deq(lkid, valblk, acmode, flags);
			if ((status & 1) && !(flags & LCK$M_CANCEL))
			{
				for (index = 0;  index < vms_lock_tail;  index++)
				{
					if (vms_lock_list[index].lockid == lkid)
					{
						vms_lock_list[index].lockid = 0;
						break;
					}
				}
			}
			GTMSECSHR_REL_DBG_PRIV;
		}
	}
	return status;
}
