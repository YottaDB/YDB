/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iosocket_rdone.c */

#include "mdef.h"
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "io.h"
#include "gt_timer.h"
#include "iosocketdef.h"
#include "gtm_utf8.h"

GBLREF io_pair	io_curr_device;

int	iosocket_rdone(mint *v, int4 msec_timeout)
{
	gtm_chset_t	ichset;
	int		ret;
	io_desc		*iod;
	mval		tmp;
	uint4		codepoint;

	ret = iosocket_readfl(&tmp, 1, msec_timeout);
	if (ret)
	{
		if (0 < tmp.str.len)
		{
			ichset = io_curr_device.in->ichset;
			switch(ichset)
			{
				case CHSET_M:
					codepoint = (unsigned char)tmp.str.addr[0];
					break;
				case CHSET_UTF8:
					UTF8_MBTOWC(tmp.str.addr, tmp.str.addr + tmp.str.len, codepoint);
					break;
				case CHSET_UTF16BE:
					UTF16BE_MBTOWC(tmp.str.addr, tmp.str.addr + tmp.str.len, codepoint);
					break;
				case CHSET_UTF16LE:
					UTF16LE_MBTOWC(tmp.str.addr, tmp.str.addr + tmp.str.len, codepoint);
					break;
				default:
					assertpro(ichset != ichset);
			}
			UNICODE_ONLY(assert(WEOF != codepoint));
		} else
			/* Null length string returns 0 */
			codepoint = 0;
		*v = (mint)(codepoint);
	} else
		*v = -1;
	return ret;
}
