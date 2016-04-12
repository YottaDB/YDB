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

/* iorm_rdone.c */

#include "mdef.h"

#include "gtm_stdio.h"

#include "io.h"
#include "gt_timer.h"
#include "iormdef.h"
#include "gtm_utf8.h"

GBLREF io_pair	io_curr_device;

int	iorm_rdone(mint *v, int4 timeout)
{
	mval		tmp;
	int		ret;
        uint4		codepoint;
	gtm_chset_t	ichset;

	*v = -1;
	ret = iorm_readfl(&tmp, -1, timeout);
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
					UTF8_MBTOWC(tmp.str.addr, tmp.str.addr + tmp.str.len, codepoint);
					break;
				case CHSET_UTF16BE:
					UTF16BE_MBTOWC(tmp.str.addr, tmp.str.addr + tmp.str.len, codepoint);
					break;
				case CHSET_UTF16LE:
					UTF16LE_MBTOWC(tmp.str.addr, tmp.str.addr + tmp.str.len, codepoint);
					break;
				case CHSET_UTF16:
					GTMASSERT;
				default:
#ifdef __MVS
					codepoint = (unsigned char)tmp.str.addr[0];
					break;
#else
					GTMASSERT;
#endif
			}
			UNICODE_ONLY(assert(WEOF != codepoint);)
		} else
			codepoint = (uint4)-1;			/* zero length is end of file */
		*v = (mint)(codepoint);
	}
	return ret;
}
