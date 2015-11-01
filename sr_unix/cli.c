/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_ctype.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"

#include <limits.h>
#include <errno.h>

#include "cli.h"
#include "util.h"
#include "cli_parse.h"

/*
 * --------------------------------------------------
 * Find the qualifier and convert it to hex.
 *
 * Return:
 *	TRUE	- OK
 *	FALSE	- Could not convert to hex
 * --------------------------------------------------
 */
boolean_t cli_get_hex(char *entry, int4 *dst)
{
	char	buf[MAX_LINE];
	char	local_str[MAX_LINE];

	assert(strlen(entry) > 0);
	strncpy(local_str, entry, sizeof(local_str) - 1);

	if ((cli_present(local_str) == CLI_PRESENT) && cli_get_value(local_str, buf))
	{
		if (!cli_str_to_hex(buf, dst))
		{
			FPRINTF(stderr, "Error: cannot convert %s value to hex number.\n", buf);
			return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}

/*
 * --------------------------------------------------
 * Find the qualifier and convert it to decimal number.
 *
 * Return:
 *	TRUE	- OK
 *	FALSE	- Could not convert to number
 * --------------------------------------------------
 */
boolean_t cli_get_int(char *entry, int *dst)
{
	char		buf[25];
	char		local_str[MAX_LINE];

	assert(strlen(entry) > 0);
	strncpy(local_str, entry, sizeof(local_str) - 1);

	if (cli_present(local_str) == CLI_PRESENT
		&& cli_get_value(local_str, buf))
	{
		if (!cli_is_dcm(buf))
		{
			FPRINTF(stderr, "Error: cannot convert %s value to decimal number.\n", buf);
			return FALSE;
		}
		*dst = ATOI(buf);
		return TRUE;
	}
	return FALSE;
}


/*
 * --------------------------------------------------
 * Find the qualifier and convert it to hex.
 * If not possible, convert it to decimal number.
 *
 * Return:
 *	TRUE	- OK
 *	FALSE	- Could not convert to number
 * --------------------------------------------------
 */
boolean_t cli_get_num(char *entry, int4 *dst)
{
	char		buf[25];
	char		local_str[MAX_LINE];
	unsigned long 	local_ulong;
	int		save_errno;

	strncpy(local_str, entry, sizeof(local_str) - 1);
	if ((cli_present(local_str) == CLI_PRESENT) && cli_get_value(local_str, buf))
	{
		if (cli_is_dcm(buf))
		{
			save_errno = errno;
			errno = 0;
			local_ulong = STRTOUL(buf, NULL, 10);
			if ((0 != errno) || UINT_MAX < local_ulong)
			{	/* errno is non-zero implies "str" is outside the range of representable values in an unsigned long.
				 * else if local_ulong > UINT_MAX, it means that on platforms where long is 8-bytes, "str" is >= 4G
				 */
				*dst = 0;
				FPRINTF(stderr, "Error: Cannot convert %s string. Overflow\n",buf);
				errno = save_errno;
				return FALSE;
			} else
			{
				*dst = local_ulong;
				errno = save_errno;
				return TRUE;
			}
		}
		if (cli_str_to_hex(buf, dst))
			return TRUE;
		FPRINTF(stderr, "Error: Cannot convert %s string to number.\n", buf);
	}
	return FALSE;
}

/*
 * --------------------------------------------------
 * Find the qualifier and copy its value to dst buffer.
 *
 * Return:
 *	TRUE	- OK
 *	FALSE	- Could not get string
 * --------------------------------------------------
 */
boolean_t cli_get_str(char *entry, char *dst, unsigned short *max_len)
{
	char		buf[MAX_LINE];
	char		local_str[MAX_LINE];

	assert(*max_len <= sizeof(buf));
	assert(*max_len > 0);

	assert(strlen(entry) > 0);
	strncpy(local_str, entry, sizeof(local_str) - 1);

	if (!(cli_present(local_str) == CLI_PRESENT
	  && cli_get_value(local_str, buf)))
	{
		if (!cli_get_parm(local_str, buf))
			return FALSE;
	}
	memset(dst, 0, *max_len);
	memcpy(dst, buf, strlen(buf));
	*max_len = strlen(buf);
	return TRUE;
}


/*
 * --------------------------------------------------
 * Find the qualifier and convert its value to millisecounds and return it in dst
 *
 * Return:
 *	TRUE	- OK
 *	FALSE	- Could not convert to time
 * --------------------------------------------------
 */
boolean_t cli_get_time(char *entry, uint4 *dst)
{
#define MAXFACTOR (10 * 100 * 60 * 60)	/* (decisec / millisec) * (decisec / sec) * (sec /min) * (min / hour) */
	char		buf[MAX_LINE + 1], *cp, local_str[MAX_LINE];
	unsigned int	factor;

	assert(strlen(entry) > 0);
	strncpy(local_str, entry, sizeof(local_str) - 1);
	buf[0] = ':';
	if ((cli_present(local_str) == CLI_PRESENT
	  && cli_get_value(local_str, buf + 1)))
	{
		for (*dst = 0, factor = 10, cp = buf + strlen(buf) - 1; cp >= buf; cp--)
		{
			if (!ISDIGIT((int)*cp))
			{
				if (':' == *cp)
				{
					if (MAXFACTOR < factor)
						return FALSE;
					*dst = *dst + (STRTOUL(cp + 1, NULL, 10) * factor);
					/* #define MAXFACTOR shows the series for factor */
					factor = ((10 == factor) ? 100 : 60) * factor;
				}
				else
					return FALSE;
			}
		}
		return TRUE;
	}
	return FALSE;
}


/*
 * --------------------------------------------------
 * Find out if cli entry is either T(rue) F(alse), or
 * neither.
 *
 * Return:
 *	1 - TRUE
 *	0 - FALSE
 *	-1 - neither
 * --------------------------------------------------
 */
int4 cli_t_f_n (char *entry)
{
	char		buf[MAX_LINE];
	char		local_str[MAX_LINE];

	assert (strlen(entry) > 0);
	strncpy(local_str, entry, sizeof(local_str) - 1);

	cli_strupper(local_str);
	if (cli_get_value(local_str, buf))
	{
		if ('t' == TOLOWER(buf[0]))
			return (1);
		else if ('f' == TOLOWER(buf[0]))
			return (0);
		else
		{
			util_out_print("Invalid value !AD specified for qualifier !AD", TRUE,
					LEN_AND_STR(buf), LEN_AND_STR(local_str));
			return (-1);
		}
	} else
	{
		FPRINTF(stderr, "Error: cannot get value for %s.\n", entry);
		return (-1);
	}
}
