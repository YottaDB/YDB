/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "patcode.h"
#include "compiler.h"

/* This routine translates system specific wildcarded strings into MUMPS patterns,
	and generates the internal pattern matching literal string into a local buffer
	by calling the PATSTR() hook into the compiler.  The result can be passed
	directly to DO_PATTERN() to match input.

    This routine uses the UNIX pattern matching characters of * and ? for multiple
	and single character wildcarding.

*/

void genpat(mstr *input, mval *patbuf)
{
	uint4 	ecount, status;
	bool		patopen;
	char		source_buffer[MAX_SRCLINE];
	char		*top, *fpat, *pat_str;
	ptstr		pat_ptstr;
	mstr		instr;

	pat_str = source_buffer;
	fpat = input->addr;
	top = fpat + input->len;
	patopen = FALSE;
	while (fpat < top)
	{	if (*fpat == '?')
		{	for (ecount = 0; fpat < top && *fpat == '?'; ecount++)
				fpat++;
			if (patopen)
			{	*pat_str++ = '"';
				patopen = FALSE;
			}
			pat_str = (char *)i2asc((uchar_ptr_t)pat_str, ecount);
			*pat_str++ = 'E';
		}
		else if (*fpat == '*')
		{	while(fpat < top && *fpat == '*')
				fpat++;
			if (patopen)
			{	*pat_str++ = '"';
				patopen = FALSE;
			}
			*pat_str++ = '.';
			*pat_str++ = 'E';
		}
		else
		{	patopen = TRUE;
			*pat_str++ = '1';
			*pat_str++ = '"';
			while(fpat < top && *fpat != '*' && *fpat != '?')
				*pat_str++ = *fpat++;
		}
	}
	if (patopen)
		*pat_str++ = '"';
	instr.addr = source_buffer;
	instr.len = INTCAST(pat_str - source_buffer);
	if (status = patstr(&instr, &pat_ptstr, NULL))
		rts_error(VARLSTCNT(1) status);
	assert(pat_ptstr.len <= MAX_PATOBJ_LENGTH);
	patbuf->str.len = pat_ptstr.len * SIZEOF(uint4);
	memcpy(patbuf->str.addr, &pat_ptstr.buff[0], patbuf->str.len);
}
