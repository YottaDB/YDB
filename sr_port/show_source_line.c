/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"
#include "compiler.h"
#include "show_source_line.h"

#define ARROW	"^-----"

GBLREF short		last_source_column;
GBLREF unsigned char	*source_buffer;
GBLREF bool		dec_nofac;

void show_source_line(void)
{
	char buf[MAX_SRCLINE + sizeof(ARROW) - 1];
	char *b, *c, *c_top;
	error_def(ERR_SRCLIN);

	for (c = (char *)source_buffer, b = buf, c_top = c + last_source_column - 1; c < c_top; c++)
	{	if (*c == '\t')
			*b++ = '\t';
		else
			*b++ = ' ';
	}
	memcpy(b, ARROW, sizeof(ARROW) - 1);
	b += sizeof(ARROW) - 1;
	dec_nofac = TRUE;
	dec_err(VARLSTCNT (6) ERR_SRCLIN, 4, LEN_AND_STR((char *)source_buffer), b - buf, buf);
	dec_nofac = FALSE;
}
