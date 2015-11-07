/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"

#define D2HEX(X) 	(((X) < 10) ? (X) + '0' : (X) - 10 + 'A')
#define BYTES_PER_LINE	32

char *ddp_trace_prefix[] =
{
	"-> ", /* DDP_SEND */
	"<- "  /* DDP_RECV */
};

void ddp_trace_output(unsigned char *cp, int len, int code)
{
	unsigned char	outbuf[BYTES_PER_LINE * 4]; /* space before and after each byte */
	unsigned char	*cin, *cout, *ctop;
	int 		n, m, p, prefix_len;

	prefix_len = strlen(ddp_trace_prefix[code]);
	assert(BYTES_PER_LINE > prefix_len);
	strcpy(outbuf, ddp_trace_prefix[code]);
	for (cin = cp, ctop = cin + len , cout = outbuf + prefix_len, n = 0; cin < ctop; n++)
	{
		if (n >= BYTES_PER_LINE)
		{
			cce_out_write(outbuf, cout - outbuf);
			strcpy(outbuf, ddp_trace_prefix[code]);
			cout = outbuf + prefix_len;
			n = 0;
		}
		m = *cin++;
		p = (m >> 4);
		*cout++ = D2HEX(p);
		p = (m & 0x0F);
		*cout++ = D2HEX(p);
		*cout++ = ' ';
	}
	if (n)
		cce_out_write(outbuf, cout - outbuf);
	return;
}
