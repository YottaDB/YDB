/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2021-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_ctype.h"

#include "stringpool.h"
#include "get_command_line.h"
#include "restrict.h"

GBLREF spdesc		stringpool;
GBLREF int 		cmd_cnt;

#ifdef __osf__
#pragma pointer_size (save)
#pragma pointer_size (long)
#endif

GBLREF char **cmd_arg;

#ifdef __osf__
#pragma pointer_size (restore)
#endif

void get_command_line(mval *result, boolean_t zcmd_line)
{
	int		first_item, len, nlen, word_cnt;
	size_t		ulen;
	unsigned char	*cp, *cp_top;

	if (RESTRICTED(zcmdline))
	{
		result->mvtype = MV_STR;
		result->str.len = result->str.char_len = 0;
		return;
	}
	result->mvtype = 0; /* so stp_gcol, if invoked below, can free up space currently occupied by this to-be-overwritten mval */
	len = -1;							/* to compensate for no space at the end */
	if (cmd_cnt > 1)
	{
		first_item = 1;
		if (zcmd_line)
		{	/* $ZCMDLINE returns the processed command line. Remove "-direct" and/or "-run <runarg>" from cmd line */
			if (!STRCMP(cmd_arg[1], "-"))
			{
				first_item += 2;
				cp = (unsigned char *)cmd_arg[2];
			} else
			{
				cp = (unsigned char *)cmd_arg[1];
				assert(NULL != cp);
				if ('-' == *cp++)
					first_item++;
			}
			if ((1 < first_item) && (NULL != cp) && ('r' == TOLOWER(*cp)))
				first_item++;
		}
		for (word_cnt = first_item; word_cnt < cmd_cnt; word_cnt++)
		{
			nlen = len + (int)STRLEN(cmd_arg[word_cnt]) + 1;		/* include space between arguments */
			assert(len < nlen);
			len = nlen;
			assert(0 <= len);
		}
	}
	if (0 >= len)
	{
		result->str.len = 0;
		result->mvtype = MV_STR; /* initialize mvtype now that mval has been otherwise completely set up */
		return;
	}
	ENSURE_STP_FREE_SPACE(len);
	cp_top = cp = stringpool.free;
	cp_top += len;
	stringpool.free += len;
	result->str.addr = (char *)cp;
	result->str.len = len;
	result->mvtype = MV_STR; /* initialize mvtype now that mval has been otherwise completely set up */
	for (word_cnt = first_item; cp <= cp_top; *cp++ = ' ')
	{
		ulen = strlen(cmd_arg[word_cnt]);
		DEBUG_ONLY(len = (int)ulen);	/* For IS_AT_END_OF_STRINGPOOL below */
		assert(cp_top >= (cp + ulen));
		memcpy(cp, cmd_arg[word_cnt], ulen);
		if (++word_cnt == cmd_cnt)
			break;
		cp += ulen;			/* Do not advance cp for IS_AT_END_OF_STRINGPOOL below */
	}
	assert(IS_AT_END_OF_STRINGPOOL(cp, len));
	return;
}
