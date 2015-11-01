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
#include "gtm_stdlib.h"

#include "rtnhdr.h"
#include "startup.h"
#include "gtm_startup.h"

/* parse an entry reference string into routine, label & offset */
void lref_parse(unsigned char *label_ref, mstr* routine, mstr* label, int* offset)
{
	unsigned char 	ch, *c, *c1;
	int 		i, label_len;
	error_def	(ERR_RUNPARAMERR);

	routine->addr = label->addr = label_ref;
	*offset = 0;
	label_len = strlen(label_ref);
	for (i = 0, c = label_ref;  i < label_len;  i++)
	{
		ch = *c++;
		if (ch == '^'  ||  ch == '+')
		{
			label->len = i;

			if (ch == '+')
			{
				*offset = STRTOL(c, (char**)&c1, 10);
				if (c == c1 ||*c1 != '^')
					rts_error(VARLSTCNT(1) ERR_RUNPARAMERR);
				c = c1 + 1;
			}
			routine->addr = c;
			routine->len = label_ref + label_len - c;
			break;
		}
	}
	if (routine->addr == label_ref)
	{
		routine->len = label_len;
		routine->addr = label_ref;
		label->len = 0;
	}
	if (!is_ident(routine))
		rts_error(VARLSTCNT(1) ERR_RUNPARAMERR);
	if (label->len && !is_ident(label))
		rts_error(VARLSTCNT(1) ERR_RUNPARAMERR);

	routine->len = routine->len > sizeof(mident) ? sizeof(mident) : routine->len;
	label->len = label->len > sizeof(mident) ? sizeof(mident) : label->len;
}
