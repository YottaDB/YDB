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

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "collseq.h"
#include "gdsfhead.h"
#include "do_xform.h"
#include "gvstrsub.h"
#include "zshow.h"

GBLREF	gv_namehead	*gv_target;
GBLREF	bool		transform;

unsigned char *gvstrsub(unsigned char *src, unsigned char *target)
{
	int		length, n, target_len;
	char		buf[MAX_KEY_SZ + 1], buf1[MAX_KEY_SZ + 1], *ptr;
	unsigned char	*str;
	mstr		mstr_x;
	mstr		mstr_tmp;

	ptr = buf;
	for (n = 0, str = src; *str; ++n, ++str)
	{
                if (*str == 1)
                {
                        str++;
                        *ptr++ = *str - 1;
                }
                else
                        *ptr++ = *str;
	}
	if (transform && gv_target && gv_target->collseq)
	{
		mstr_x.len = n;
		mstr_x.addr = buf;
		mstr_tmp.len = sizeof(buf1);
		mstr_tmp.addr = buf1;
		do_xform(gv_target->collseq, XBACK, &mstr_x, &mstr_tmp, &length);
		n = length;
		str = (unsigned char *)mstr_tmp.addr; /* mstr_tmp.addr is used just in case it is
							 reallocated in the XBACK routine */
	} else
		str = (unsigned char *)buf;
	format2zwr((sm_uc_ptr_t)str, n, target, &target_len);
	return target + target_len;
}
