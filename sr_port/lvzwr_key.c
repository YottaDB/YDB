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
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "zwrite.h"

GBLREF lvzwrite_struct lvzwrite_block;

unsigned char *lvzwr_key(buff, size)
unsigned char *buff;
unsigned short size;
{
	int n;
	unsigned char *cp, *cq;

	cp = (unsigned char *)lvzwrite_block.curr_name;
	for (cq = cp + sizeof(mident) ; cp < cq && *cp && size; cp++)
	{
		*buff++ = *cp;
		size--;
	}
	if (lvzwrite_block.subsc_count)
	{
		if (size)
			*buff++ = '('; size--;
		for (n = 0 ; ; )
		{
			MV_FORCE_STR(((zwr_sub_lst *)lvzwrite_block.sub)->subsc_list[n].actual);
			if (size > ((zwr_sub_lst *)lvzwrite_block.sub)->subsc_list[n].actual->str.len)
			{
				memcpy(buff, ((zwr_sub_lst *)lvzwrite_block.sub)->subsc_list[n].actual->str.addr,
					((zwr_sub_lst *)lvzwrite_block.sub)->subsc_list[n].actual->str.len);
				buff += ((zwr_sub_lst *)lvzwrite_block.sub)->subsc_list[n].actual->str.len;
				size -= ((zwr_sub_lst *)lvzwrite_block.sub)->subsc_list[n].actual->str.len;
			}
			else
				break;

			if (++n < lvzwrite_block.curr_subsc && size)
			{	*buff++ = ',';
				size--;
			}else
			{
				if (size)
				{	*buff++ = ')';
					size--;
				}
				break;
			}
		}
	}
	return buff;
}
