/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iorm_rdone.c */

#include "mdef.h"

#include "gtm_stdio.h"

#include "io.h"
#include "gt_timer.h"
#include "iormdef.h"
#include "gtm_utf8.h"

GBLREF io_pair	io_curr_device;

int	iorm_rdone(mint *v, int4 msec_timeout)
{
	mval		tmp;
	int		ret;
        uint4		codepoint;
	gtm_chset_t	ichset;

	*v = -1;
	ret = iorm_readfl(&tmp, -1, msec_timeout);
	if (ret)
	{
		if (0 != tmp.str.len)
		{
			ichset = io_curr_device.in->ichset;
			switch(ichset)
			{
				case CHSET_M:
					codepoint = (unsigned char)tmp.str.addr[0];
					break;
				case CHSET_UTF8:
				case CHSET_UTF16BE:
				case CHSET_UTF16LE:
					UTF8_MBTOWC(tmp.str.addr, tmp.str.addr + tmp.str.len, codepoint);
					break;
				case CHSET_UTF16:
					assertpro(ichset && FALSE);
				default:
#ifdef __MVS
					codepoint = (unsigned char)tmp.str.addr[0];
					break;
#else
					assertpro(ichset && FALSE);
#endif
			}
			UNICODE_ONLY(assert(WEOF != codepoint);)
		} else
			codepoint = (uint4)-1;			/* zero length is end of file */
		*v = (mint)(codepoint);
	}
	return ret;
}
