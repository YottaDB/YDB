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
#include <prvdef.h>
#include <psldef.h>
#include <ssdef.h>
#include <efndef.h>

#include "gtmsecshr.h"
#include "secshr_db_clnup.h"
#include "locks.h"
#include "vmsdtype.h"

GBLREF uint4		rundown_process_id;
GBLREF lock_sb		vms_lock_list[MAX_VMS_LOCKS + 1];
GBLREF int		vms_lock_tail;

#define MAX_TRIES 10

void	user_rundown()
{
	struct
	{
		item_list_3	item[1];
		int4		terminator;
	} item_list;
	boolean_t	repeat;
	int		index, lcnt;
	unsigned short	iosb[4];
	uint4		lk_pid, retlen, status;
	uint4		prvadr[2], prvprv[2];

#ifndef TEST_REPL
	secshr_db_clnup(ABNORMAL_TERMINATION);
#endif
	GTMSECSHR_SET_DBG_PRIV(PRV$M_SYSLCK, status);
	if (SS$_NORMAL == status)
	{
		item_list.item[0].buffer_length = SIZEOF(lk_pid);
		item_list.item[0].item_code = LKI$_PID;
		item_list.item[0].buffer_address = &lk_pid;
		item_list.item[0].return_length_address = &retlen;
		item_list.terminator = 0;
		repeat = TRUE;
		for (lcnt = 0;  repeat && lcnt < MAX_TRIES;  lcnt++)
		{
			repeat = FALSE;
			for (index = 0;  index < vms_lock_tail;  index++)
			{
				if (vms_lock_list[index].lockid)
				{
					if ((SS$_NORMAL == (status = sys$getlkiw(EFN$C_ENF, &vms_lock_list[index].lockid,
								&item_list, iosb, NULL, 0, 0)))
							&& (lk_pid == rundown_process_id))
						status = sys$deq(vms_lock_list[index].lockid, NULL, PSL$C_USER, 0);
					if (SS$_SUBLOCKS != status)
						vms_lock_list[index].lockid = 0;
					else
						repeat = TRUE;
				}
			}
		}
		GTMSECSHR_REL_DBG_PRIV;
	}
}
