/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iosocket_wtone.c */

#include "mdef.h"
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "io.h"
#include "gt_timer.h"
#include "iotcpdef.h"
#include "iosocketdef.h"
#include "gtm_utf8.h"

GBLREF io_pair	io_curr_device;

void iosocket_wtone(int ch)
{
	mstr	temp;
	char	c, uni_c[4], *endptr;
	io_desc	*iod;

	if (CHSET_M == io_curr_device.out->ochset)
	{
		c = (char)ch;
		temp.len = 1;
		temp.addr = (char *)&c;
	} else
	{
		switch(io_curr_device.out->ochset)
		{
			case CHSET_UTF8:
				endptr = (char *)UTF8_WCTOMB(ch, uni_c);
				break;
			case CHSET_UTF16: /* unspecified endian format implies Big Endian */
			case CHSET_UTF16BE:
				endptr = UTF16BE_WCTOMB(ch, uni_c);
				break;
			case CHSET_UTF16LE:
				endptr = UTF16LE_WCTOMB(ch, uni_c);
				break;
			default:
				GTMASSERT;
		}
		temp.addr = uni_c;
		temp.len = INTCAST(endptr - uni_c);
		assert(0 < temp.len); /* we validated the code point already in op_wtone() */
	}
	UNICODE_ONLY(temp.char_len = 1);
	iosocket_write_real(&temp, TRUE);
	return;
}
