/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

#include "gtm_limits.h"
#include <errno.h>

#include "cli.h"
#include "util.h"
#include "cli_parse.h"
#include "min_max.h"
#include "gtmimagename.h"
#include "gtmmsg.h"

error_def(ERR_CLISTRTOOLONG);

/*
 * --------------------------------------------------
 * Find the qualifier and convert it to hex.
 *
 * Return:
 *	TRUE	- OK
 *	FALSE	- Could not convert to hex
 * --------------------------------------------------
 */
boolean_t cli_get_hex(char *entry, uint4 *dst)
{
	char	buf[MAX_LINE];
	char	local_str[MAX_LINE];

	assert(strlen(entry) > 0);
	strncpy(local_str, entry, SIZEOF(local_str) - 1);

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
 * Find the qualifier and convert it to 64 bit hex.
 *
 * Return:
 *	TRUE	- OK
 *	FALSE	- Could not convert to hex
 * --------------------------------------------------
 */
boolean_t cli_get_hex64(char *entry, gtm_uint64_t *dst)
{
	char	buf[MAX_LINE];
	char	local_str[MAX_LINE];

	assert(strlen(entry) > 0);
	strncpy(local_str, entry, SIZEOF(local_str) - 1);

	if ((cli_present(local_str) == CLI_PRESENT) && cli_get_value(local_str, buf))
	{
		if (!cli_str_to_hex64(buf, dst))
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
 * Find the qualifier and convert it to 64 bit unsigned decimal number.
 *
 * Return:
 *	TRUE	- OK
 *	FALSE	- Could not convert to hex
 * --------------------------------------------------
 */
boolean_t cli_get_uint64(char *entry, gtm_uint64_t *dst)
{
	char	buf[MAX_LINE];
	char	local_str[MAX_LINE];

	assert(strlen(entry) > 0);
	strncpy(local_str, entry, SIZEOF(local_str) - 1);

	if ((cli_present(local_str) == CLI_PRESENT) && cli_get_value(local_str, buf))
	{
		if (!cli_str_to_uint64(buf, dst))
		{
			FPRINTF(stderr, "Error: cannot convert %s value to 64-bit unsigned decimal number.\n", buf);
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
boolean_t cli_get_int(char *entry, int4 *dst)
{
	char		buf[MAX_LINE];
	char		local_str[MAX_LINE];

	assert(strlen(entry) > 0);
	strncpy(local_str, entry, SIZEOF(local_str) - 1);

	if (cli_present(local_str) == CLI_PRESENT
		&& cli_get_value(local_str, buf))
	{
		if (!cli_is_dcm(buf) || !cli_str_to_int(buf, dst))
		{
			FPRINTF(stderr, "Error: cannot convert %s value to decimal number.\n", buf);
			return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}

/*
 * --------------------------------------------------
 * Find the qualifier and convert it to 64 bit decimal number.
 *
 * Return:
 *	TRUE	- OK
 *	FALSE	- Could not convert to number
 * --------------------------------------------------
 */
boolean_t cli_get_int64(char *entry, gtm_int64_t *dst)
{
	char		buf[MAX_LINE];
	char		local_str[MAX_LINE];

	assert(strlen(entry) > 0);
	strncpy(local_str, entry, SIZEOF(local_str) - 1);

	if (cli_present(local_str) == CLI_PRESENT
		&& cli_get_value(local_str, buf))
	{
		if (!cli_is_dcm(buf) || !cli_str_to_int64(buf, dst))
		{
			FPRINTF(stderr, "Error: cannot convert %s value to decimal number.\n", buf);
			return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}
/*
 * --------------------------------------------------
 * Find the qualifier and convert it to decimal number
 * unless 0x prefix.
 *
 * Return:
 *	TRUE	- OK
 *	FALSE	- Could not convert to number
 * --------------------------------------------------
 */
boolean_t cli_get_num(char *entry, int4 *dst)
{
	char		buf[MAX_LINE];
	char		local_str[MAX_LINE];

	assert(strlen(entry) > 0);
	strncpy(local_str, entry, SIZEOF(local_str) - 1);

	if (cli_present(local_str) == CLI_PRESENT
		&& cli_get_value(local_str, buf))
	{
		if (!cli_str_to_num(buf, dst))
		{
			FPRINTF(stderr, "Error: cannot convert %s string to number.\n", buf);
			return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}

/*
 * --------------------------------------------------
 * Find the qualifier and convert it to 64 bit decimal number
 * unless 0x prefix.
 *
 * Return:
 *	TRUE	- OK
 *	FALSE	- Could not convert to number
 * --------------------------------------------------
 */
boolean_t cli_get_num64(char *entry, gtm_int64_t *dst)
{
	char		buf[MAX_LINE];
	char		local_str[MAX_LINE];

	assert(strlen(entry) > 0);
	strncpy(local_str, entry, SIZEOF(local_str) - 1);

	if (cli_present(local_str) == CLI_PRESENT
		&& cli_get_value(local_str, buf))
	{
		if (!cli_str_to_num64(buf, dst))
		{
			FPRINTF(stderr, "Error: cannot convert %s string to number.\n", buf);
			return FALSE;
		}
		return TRUE;
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
	size_t		maxdstlen, maxbuflen, copylen;
#	ifdef DEBUG
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	endif
	maxbuflen = SIZEOF(buf);
	maxdstlen = *max_len;

	assert(maxdstlen <= maxbuflen);
	assert(maxdstlen > 0);

	assert(strlen(entry) > 0);
	strncpy(local_str, entry, SIZEOF(local_str) - 1);

	DEBUG_ONLY(TREF(cli_get_str_max_len) = maxdstlen;)	/* for use inside cli_get_value -> get_parm_entry ... */
	if (!((CLI_PRESENT == cli_present(local_str)) && cli_get_value(local_str, buf)))
	{
		if (!cli_get_parm(local_str, buf))
		{
			DEBUG_ONLY(TREF(cli_get_str_max_len) = 0;)	/* for use inside cli_get_value -> get_parm_entry ... */
			return FALSE;
		}
	}
	DEBUG_ONLY(TREF(cli_get_str_max_len) = 0;)	/* for use inside cli_get_value -> get_parm_entry ... */
	copylen = strlen(buf);
	if (maxdstlen < copylen)
	{
		if (!IS_GTM_IMAGE)
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_CLISTRTOOLONG, 3, entry, copylen, maxdstlen);
		else
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_CLISTRTOOLONG, 3, entry, copylen, maxdstlen);
		return FALSE;
	}
	memset(dst, 0, maxdstlen);
	memcpy(dst, buf, copylen);
	*max_len = (unsigned short) copylen;
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
	strncpy(local_str, entry, SIZEOF(local_str) - 1);
	buf[0] = ':';
	if ((cli_present(local_str) == CLI_PRESENT
	  && cli_get_value(local_str, buf + 1)))
	{
		for (*dst = 0, factor = 10, cp = buf + strlen(buf) - 1; cp >= buf; cp--)
		{
			if (!ISDIGIT_ASCII((int)*cp))
			{
				if (':' == *cp)
				{
					if (MAXFACTOR < factor)
						return FALSE;
					*dst = *dst + (uint4)(STRTOUL(cp + 1, NULL, 10) * factor);
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
	strncpy(local_str, entry, SIZEOF(local_str) - 1);
	local_str[SIZEOF(local_str) - 1] = '\0';
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

/*
 * --------------------------------------------------
 * Find out if cli entry is either T(rue), A(lways), F(alse), N(ever), or
 * neither.
 *
 * Return:
 *	0 - FALSE/NEVER
 *	1 - TRUE/ALWAYS
 *      2 - ALLOWEXISTING
 *     -1 - None of them
 * --------------------------------------------------
 */
int4 cli_n_a_e (char *entry)
{
	char		buf[MAX_LINE];
	char		local_str[MAX_LINE];

	assert (strlen(entry) > 0);
	strncpy(local_str, entry, SIZEOF(local_str) - 1);
	local_str[SIZEOF(local_str) - 1] = '\0';
	cli_strupper(local_str);
	if (cli_get_value(local_str, buf))
	{
		if ('f' == TOLOWER(buf[0]) || 'n' == TOLOWER(buf[0]))
			return (0);
		else if ('t' == TOLOWER(buf[0]) || 'a' == TOLOWER(buf[0]))
			return (1);
		else if ('e' == TOLOWER(buf[0]))
			return (2);
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

boolean_t cli_get_defertime(char *entry, int4 *dst)
{
	char		buf[MAX_LINE];
	char		local_str[MAX_LINE];
	int		ch_index = 0;
	int4		prev_value = 0, num = 0;
	boolean_t	neg_num = FALSE;

	assert(strlen(entry) > 0);
	strncpy(local_str, entry, SIZEOF(local_str) - 1);

	if (cli_present(local_str) == CLI_PRESENT
		&& cli_get_value(local_str, buf))
	{
		prev_value = 0;
		if (buf[ch_index] == '-')
		{
			neg_num = TRUE;
			ch_index++;
		}
		for (; ((buf[ch_index] >= '0' && buf[ch_index] <= '9') && (buf[ch_index] != '\0')); ch_index++)
		{
			num = num * 10 + (buf[ch_index] - '0');
			if (num < prev_value)
				break;
			prev_value = num;
		}
		if (neg_num && buf[ch_index] == '\0')
		{
			num = num * -1;
		}
		if((-1 > num || num > INT_MAX) || buf[ch_index] != '\0' || (neg_num && !num))
		{
			FPRINTF(stderr, "Error: cannot convert %s value to decimal number.\n", buf);
			return FALSE;
		}
		*dst = num;
		return TRUE;
	}
	return FALSE;
}

