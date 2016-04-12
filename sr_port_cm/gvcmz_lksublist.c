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

#include "gtm_string.h"

#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "mlkdef.h"
#include "gvcmz.h"
#include "cmi.h"
#include "iosp.h"
#include "copy.h"

GBLREF unsigned char cmlk_num;

void gvcmz_lksublist(struct CLB *lnk)
{
	mlk_pvtblk	*lk_walk;
	unsigned char	*ptr, *list_len, *hdr, count, save_hdr;
	unsigned short	len, msg_len;
	uint4		status;

	hdr = ptr = lnk->mbf;
	save_hdr = *hdr;
	ptr += S_HDRSIZE + S_LAFLAGSIZE + 1;
	list_len = ptr++;
	count = 0;
	msg_len = S_HDRSIZE + S_LAFLAGSIZE + 1 + 1;
	lk_walk = ((link_info *)(lnk->usr))->netlocks;
	while (lk_walk)
	{
		len = 1 + 1 + 1 + lk_walk->total_length; /* regnum + translev + subsc count + key */
		if (msg_len + len + SIZEOF(len) >= lnk->mbl)
		{
			*hdr = CMMS_L_LKREQNODE;
			*list_len = count;
			lnk->cbl = msg_len;
			status = cmi_write(lnk);
			if (CMI_ERROR(status))
			{
				((link_info *)(lnk->usr))->neterr = TRUE;
				gvcmz_error(CMMS_L_LKREQNODE, status);
				return;
			}
			msg_len = S_HDRSIZE + S_LAFLAGSIZE + 1 + 1;
			count = 0;
			ptr = list_len + 1;
		}
		len = 1 + 1 + 1 + lk_walk->total_length; /* regnum + translev + subsc count + key */
		CM_PUT_USHORT(ptr, len, ((link_info *)(lnk->usr))->convert_byteorder);
		ptr += SIZEOF(unsigned short);
		*ptr++ = lk_walk->region->cmx_regnum;
		*ptr++ = lk_walk->translev;
		assert(256 > lk_walk->subscript_cnt); /* else the assignment "*ptr++ = lk_walk->subscript_cnt" could be lossy */
		*ptr++ = lk_walk->subscript_cnt;
		memcpy(ptr, lk_walk->value, lk_walk->total_length);
		ptr += lk_walk->total_length;
		count++;
		msg_len += len + SIZEOF(len);
		lk_walk = lk_walk->next;
	}
	if (count)
	{
		*list_len = count;
		lnk->cbl = msg_len;
	}
	*hdr = save_hdr;
}
