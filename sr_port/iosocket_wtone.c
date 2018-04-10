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

/* iosocket_wtone.c */

#include "mdef.h"
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "io.h"
#include "gt_timer.h"
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
			case CHSET_UTF16:
			case CHSET_UTF16BE:
			case CHSET_UTF16LE:
				endptr = (char *)UTF8_WCTOMB(ch, uni_c);
				break;
			default:
				assertpro(io_curr_device.out->ochset != io_curr_device.out->ochset);
		}
		temp.addr = uni_c;
		temp.len = INTCAST(endptr - uni_c);
		assert(0 < temp.len); /* we validated the code point already in op_wtone() */
	}
	UNICODE_ONLY(temp.char_len = 1);
	iosocket_write_real(&temp, TRUE);
	return;
}
