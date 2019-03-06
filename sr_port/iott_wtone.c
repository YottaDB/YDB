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

#include "mdef.h"

#include "io.h"
#ifdef UTF8_SUPPORTED
#include "gtm_utf8.h"
#endif

GBLREF	io_pair		io_curr_device;
GBLREF	boolean_t	gtm_utf8_mode;
LITREF	mstr		chset_names[];

void iott_wtone(int v)
{
	mstr	temp;
	char	p[1];
#ifdef UTF8_SUPPORTED
	unsigned char	utf_buf[GTM_MB_LEN_MAX], *up;
#endif
	io_desc	*iod;
	UTF8_ONLY(error_def(ERR_BADCHSET);)

	if (!gtm_utf8_mode UTF8_ONLY(|| CHSET_M == io_curr_device.out->ochset))
	{
		p[0] = (char)v;
		temp.len = 1;
		temp.addr = p;
		UTF8_ONLY(temp.char_len = 1;)
	}
#ifdef UTF8_SUPPORTED
	else if (CHSET_UTF8 == io_curr_device.out->ochset)
	{
		up = UTF8_WCTOMB(v, utf_buf);
		temp.len = INTCAST(up - utf_buf);
		temp.addr = (char *)&utf_buf[0];
	} else
		rts_error(VARLSTCNT(4) ERR_BADCHSET, 2, chset_names[io_curr_device.out->ochset].len,
				chset_names[io_curr_device.out->ochset].addr);
#endif
	iott_write(&temp);
	return;
}
