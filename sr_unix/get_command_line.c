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

#include "gtm_string.h"
#include "gtm_ctype.h"

#include "stringpool.h"
#include "get_command_line.h"

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
	int		first_item, len, word_cnt;
	unsigned char	*cp;

	result->mvtype = 0; /* so stp_gcol (if invoked below) can free up space currently occupied by this to-be-overwritten mval */
	len = -1;							/* to compensate for no space at the end */
	if (cmd_cnt > 1)
	{
		first_item = 1;
		if (zcmd_line)
		{	/* $ZCMDLINE returns the processed command line. Remove "-direct" and/or "-run <runarg>" from cmd line */
			cp = (unsigned char *)cmd_arg[1];
			if ('-' == *cp++)
			{
				first_item++;
				if ('r' == TOLOWER(*cp))
					first_item++;
			}
		}
		for (word_cnt = first_item; word_cnt < cmd_cnt; word_cnt++)
			len += STRLEN(cmd_arg[word_cnt]) + 1;		/* include space between arguments */
	}
	if (0 >= len)
	{
		result->str.len = 0;
		result->mvtype = MV_STR; /* initialize mvtype now that mval has been otherwise completely set up */
		return;
	}
	ENSURE_STP_FREE_SPACE(len);
	cp = stringpool.free;
	stringpool.free += len;
	result->str.addr = (char *)cp;
	result->str.len = len;
	result->mvtype = MV_STR; /* initialize mvtype now that mval has been otherwise completely set up */
	for (word_cnt = first_item; ; *cp++ = ' ')
	{
		len = STRLEN(cmd_arg[word_cnt]);
		memcpy(cp, cmd_arg[word_cnt], len);
		if (++word_cnt == cmd_cnt)
			break;
		cp += len;
	}
	assert(cp + len == stringpool.free);
	return;
}
