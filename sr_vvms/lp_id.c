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

#include <ssdef.h>
#include <descrip.h>
#include <psldef.h>
#include <syidef.h>
#include <lckdef.h>
#include <efndef.h>

#include "gdsroot.h"
#include "ladef.h"
#include "vmsdtype.h"
#include "locks.h"

uint4	lp_id(uint4 *lkid)
{
	struct
	{
		item_list_3	ilist[1];
		int4		terminator;
	}			syi_list_1;
	struct dsc$descriptor_s node_dsc;
	vms_lock_sb		lksb;
	uint4			node, status;
	unsigned short		iosb[4], retlen, local_nodename_len;
	char			node_buff[SIZEOF(LMLK) + 2 * SIZEOF(long)];

	node = 0;
	syi_list_1.ilist[0].item_code = SYI$_NODE_CSID;
	syi_list_1.ilist[0].buffer_address = &node;
	syi_list_1.ilist[0].buffer_length = SIZEOF(node);
	syi_list_1.ilist[0].return_length_address = &retlen;
	syi_list_1.terminator = 0;
	status = sys$getsyiw(EFN$C_ENF, NULL, NULL, &syi_list_1, iosb, NULL, 0);
	if (SS$_NORMAL == status)
		status = iosb[0];
	if (SS$_NORMAL == status)
	{
		memcpy(node_buff, LMLK, SIZEOF(LMLK) - 1);
		node_buff[SIZEOF(LMLK) - 1] = '_';
		i2hex(node, &node_buff[SIZEOF(LMLK)], 2 * SIZEOF(long));
		node_dsc.dsc$w_length = SIZEOF(LMLK) + 2 * SIZEOF(long);
		node_dsc.dsc$a_pointer = node_buff;
		node_dsc.dsc$b_dtype = DSC$K_DTYPE_T;
		node_dsc.dsc$b_class = DSC$K_CLASS_S;
		status = gtm_enqw(EFN$C_ENF, LCK$K_NLMODE, &lksb, LCK$M_SYSTEM, &node_dsc, *lkid, NULL, 0, NULL, PSL$C_USER, 0);
		if (SS$_NORMAL == status)
			status = lksb.cond;
		if (SS$_NORMAL == status)
			*lkid = lksb.lockid;
	}
	return status;
}
