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
#include "io.h"
#ifdef UNICODE_SUPPORTED
#include "gtm_utf8.h"
#endif

GBLREF	io_pair		io_curr_device;
GBLREF	boolean_t	gtm_utf8_mode;

void iorm_wtone(int ch)
{
	mstr		temp;
	char		c;
#ifdef UNICODE_SUPPORTED
	unsigned char	uni_buf[GTM_MB_LEN_MAX], *endptr;
#endif

	if (!gtm_utf8_mode || !IS_UTF_CHSET(io_curr_device.out->ochset))
	{
		c = (char)ch;
		temp.len = 1;
		temp.addr = &c;
	}
#ifdef UNICODE_SUPPORTED
	else
	{
		switch (io_curr_device.out->ochset)
		{
			case CHSET_UTF8:
				endptr = UTF8_WCTOMB(ch, uni_buf);
				break;
			case CHSET_UTF16:
				/* iorm_write will write BE BOM if first write */
				/* continue as if UTF16BE */
			case CHSET_UTF16BE:
				endptr = UTF16BE_WCTOMB(ch, uni_buf);
				break;
			case CHSET_UTF16LE:
				endptr = UTF16LE_WCTOMB(ch, uni_buf);
				break;
			default:
				GTMASSERT;
		}
		temp.addr = (char *)uni_buf;
		temp.len = INTCAST(endptr - uni_buf);
		assert(0 < temp.len); /* we validated the code point already in op_wtone() */
	}
#endif
	UNICODE_ONLY(temp.char_len = 1;)
	iorm_write(&temp);
	return;
}
