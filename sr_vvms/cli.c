/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <climsgdef.h>
#include <descrip.h>
#include "gtm_limits.h"

#include "gtm_ctype.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"

#include "cli.h"
#include "gtmmsg.h"
#include "error.h"

extern int4 CLI$GET_VALUE();
extern int CLI$PRESENT();

error_def(ERR_STRNOTVALID);

CONDITION_HANDLER(clich)
{
	START_CH;
	CONTINUE;
}

boolean_t cli_get_hex(char *e, uint4 *dst)
{
	uint4		status;
	unsigned short	retlength = 0;
	char		buf[MAX_LINE], *ptr;

	$DESCRIPTOR(d_buf, buf);
	$DESCRIPTOR(d_ent, " ");

	d_ent.dsc$a_pointer = e;
	d_ent.dsc$w_length = strlen(e);
	assert(d_ent.dsc$w_length > 0);
	if (CLI$_PRESENT == CLI$PRESENT(&d_ent))
	{
		if (SS$_NORMAL == (status = CLI$GET_VALUE(&d_ent, &d_buf, &retlength)))
		{
			ptr = &buf[0];
			buf[retlength] = 0;	/* for cli_str_to_hex */
			if (!cli_str_to_hex(ptr, dst))
			{
				gtm_putmsg(VARLSTCNT(4) ERR_STRNOTVALID, 2, retlength, buf);
				return (FALSE);
			}
			return (TRUE);
		} else
			gtm_putmsg(VARLSTCNT(5) ERR_STRNOTVALID, 2, retlength, buf, status);
	}
	return (FALSE);
}

boolean_t cli_get_hex64(char *e, gtm_uint64_t *dst)
{
	uint4		status;
	unsigned short	retlength = 0;
	char		buf[MAX_LINE], *ptr;

	$DESCRIPTOR(d_buf, buf);
	$DESCRIPTOR(d_ent, " ");

	d_ent.dsc$a_pointer = e;
	d_ent.dsc$w_length = strlen(e);
	assert(d_ent.dsc$w_length > 0);
	if (CLI$_PRESENT == CLI$PRESENT(&d_ent))
	{
		if (SS$_NORMAL == (status = CLI$GET_VALUE(&d_ent, &d_buf, &retlength)))
		{
			ptr = &buf[0];
			buf[retlength] = 0;	/* for cli_str_to_hex64 */
			if (!cli_str_to_hex64(ptr, dst))
			{
				gtm_putmsg(VARLSTCNT(4) ERR_STRNOTVALID, 2, retlength, buf);
				return (FALSE);
			}
			return (TRUE);
		} else
			gtm_putmsg(VARLSTCNT(5) ERR_STRNOTVALID, 2, retlength, buf, status);
	}
	return (FALSE);
}

boolean_t cli_get_uint64(char *e, gtm_uint64_t *dst)
{
	uint4		status;
	unsigned short	retlength = 0;
	char		buf[MAX_LINE], *ptr;

	$DESCRIPTOR(d_buf, buf);
	$DESCRIPTOR(d_ent, " ");

	d_ent.dsc$a_pointer = e;
	d_ent.dsc$w_length = strlen(e);
	assert(d_ent.dsc$w_length > 0);
	if (CLI$_PRESENT == CLI$PRESENT(&d_ent))
	{
		if (SS$_NORMAL == (status = CLI$GET_VALUE(&d_ent, &d_buf, &retlength)))
		{
			ptr = &buf[0];
			buf[retlength] = 0;	/* for cli_str_to_uint64 */
			if (!cli_str_to_uint64(ptr, dst))
			{
				gtm_putmsg(VARLSTCNT(4) ERR_STRNOTVALID, 2, retlength, buf);
				return (FALSE);
			}
			return (TRUE);
		} else
			gtm_putmsg(VARLSTCNT(5) ERR_STRNOTVALID, 2, retlength, buf, status);
	}
	return (FALSE);
}

boolean_t cli_get_int(char *e, int *dst)		/* entity, destination */
{
	uint4		status;
	unsigned short	retlength = 0;
	unsigned char	buf[MAX_LINE], *bufpt;

	$DESCRIPTOR(d_buf, buf);
	$DESCRIPTOR(d_ent, " ");

	d_ent.dsc$a_pointer = e;
	d_ent.dsc$w_length = strlen(e);
	assert(d_ent.dsc$w_length > 0);

	if (CLI$_PRESENT == CLI$PRESENT(&d_ent))
	{
		if (SS$_NORMAL == (status = CLI$GET_VALUE(&d_ent, &d_buf, &retlength)))
		{
			bufpt = &buf[0];
			buf[retlength] = 0;	/* for cli_str_to_int */
			if (!cli_str_to_int(bufpt, dst))
			{
				gtm_putmsg(VARLSTCNT(4) ERR_STRNOTVALID, 2, retlength, buf);
				return FALSE;
			} else
				return TRUE;
		} else
			gtm_putmsg(VARLSTCNT(5) ERR_STRNOTVALID, 2, retlength, buf, status);
	}
	return FALSE;
}

boolean_t cli_get_int64(char *e, gtm_int64_t *dst)		/* entity, destination */
{
	uint4		status;
	unsigned short	retlength = 0;
	unsigned char	buf[MAX_LINE], *bufpt;

	$DESCRIPTOR(d_buf, buf);
	$DESCRIPTOR(d_ent, " ");

	d_ent.dsc$a_pointer = e;
	d_ent.dsc$w_length = strlen(e);
	assert(d_ent.dsc$w_length > 0);

	if (CLI$_PRESENT == CLI$PRESENT(&d_ent))
	{
		if (SS$_NORMAL == (status = CLI$GET_VALUE(&d_ent, &d_buf, &retlength)))
		{
			bufpt = &buf[0];
			buf[retlength] = 0;	/* for cli_str_to_int64 */
			if (!cli_str_to_int64(bufpt, dst))
			{
				gtm_putmsg(VARLSTCNT(4) ERR_STRNOTVALID, 2, retlength, buf);
				return FALSE;
			} else
				return TRUE;
		} else
			gtm_putmsg(VARLSTCNT(5) ERR_STRNOTVALID, 2, retlength, buf, status);
	}
	return FALSE;
}

boolean_t cli_get_num(char *e, int *dst)		/* entity, destination */
{
	uint4		status;
	unsigned short	retlength = 0;
	unsigned char	buf[MAX_LINE], *bufpt;

	$DESCRIPTOR(d_buf, buf);
	$DESCRIPTOR(d_ent, " ");

	d_ent.dsc$a_pointer = e;
	d_ent.dsc$w_length = strlen(e);
	assert(d_ent.dsc$w_length > 0);

	if (CLI$_PRESENT == CLI$PRESENT(&d_ent))
	{
		if (SS$_NORMAL == (status = CLI$GET_VALUE(&d_ent, &d_buf, &retlength)))
		{
			bufpt = &buf[0];
			buf[retlength] = 0;	/* for cli_str_to_num */
			if (!cli_str_to_num(bufpt, dst))
			{
				gtm_putmsg(VARLSTCNT(4) ERR_STRNOTVALID, 2, retlength, buf);
				return FALSE;
			} else
				return TRUE;
		} else
			gtm_putmsg(VARLSTCNT(5) ERR_STRNOTVALID, 2, retlength, buf, status);
	}
	return FALSE;
}

boolean_t cli_get_num64(char *e, gtm_int64_t *dst)		/* entity, destination */
{
	uint4		status;
	unsigned short	retlength = 0;
	unsigned char	buf[MAX_LINE], *bufpt;

	$DESCRIPTOR(d_buf, buf);
	$DESCRIPTOR(d_ent, " ");

	d_ent.dsc$a_pointer = e;
	d_ent.dsc$w_length = strlen(e);
	assert(d_ent.dsc$w_length > 0);

	if (CLI$_PRESENT == CLI$PRESENT(&d_ent))
	{
		if (SS$_NORMAL == (status = CLI$GET_VALUE(&d_ent, &d_buf, &retlength)))
		{
			bufpt = &buf[0];
			buf[retlength] = 0;	/* for cli_str_to_num64 */
			if (!cli_str_to_num64(bufpt, dst))
			{
				gtm_putmsg(VARLSTCNT(4) ERR_STRNOTVALID, 2, retlength, buf);
				return FALSE;
			} else
				return TRUE;
		} else
			gtm_putmsg(VARLSTCNT(5) ERR_STRNOTVALID, 2, retlength, buf, status);
	}
	return FALSE;
}

boolean_t cli_get_str(char *e, char *dst, unsigned short *ml)	/* entity, destination, max length */
{
	char	buf[MAX_LINE];
	$DESCRIPTOR(d_buf, buf);
	$DESCRIPTOR(d_ent, " ");
	unsigned short	l=0;
	int4	status;

	assert(*ml > 0);
	d_ent.dsc$a_pointer = e;
	d_ent.dsc$w_length = strlen(e);
	assert(d_ent.dsc$w_length > 0);

	if (((CLI$_PRESENT == (status = CLI$PRESENT(&d_ent))) || (CLI$_LOCPRES == status))
		&& (CLI$_ABSENT != CLI$GET_VALUE(&d_ent, &d_buf, &l)))
	{
		if (l <= *ml)
		{
			memset(dst, 0, *ml);
			memcpy(dst, buf, l);
			*ml = l;
			return TRUE;
		}
	}
	return FALSE;
}

boolean_t cli_negated(char *e)	/* entity */
{
	$DESCRIPTOR(d_ent, " ");
	int4	status;

	d_ent.dsc$a_pointer = e;
	d_ent.dsc$w_length = strlen(e);
	assert(d_ent.dsc$w_length > 0);
	status =CLI$PRESENT(&d_ent);
	if ((CLI$_NEGATED == status) || (CLI$_LOCNEG == status))
		return TRUE;
	else
		return FALSE;
}

int cli_present(char *e)	/* entity */
{
	int4	status;
	$DESCRIPTOR(d_ent, " ");

	d_ent.dsc$a_pointer = e;
	d_ent.dsc$w_length = strlen(e);
	assert(d_ent.dsc$w_length > 0);
	status = CLI$PRESENT(&d_ent);
	if ((CLI$_PRESENT == status) || (CLI$_LOCPRES == status))
		return CLI_PRESENT;
	else if ((CLI$_NEGATED == status) || (CLI$_LOCNEG == status))
		return CLI_NEGATED;
	else if (CLI$_ABSENT == status)
		return CLI_ABSENT;
	else
		return status;
}

int4 cli_t_f_n(char *e)	/* entity */
{	/* this function retrieves a CLI value of type TRUE_FALSE_NEITHER (for example, as seen in PATCH_CMD.CLD).
	 * It assumes the presence of the qualifier in question.
	 */
	uint4 		status;
	char		buf[MAX_LINE];
	unsigned short		retlength = 0;
	$DESCRIPTOR(d_buf, buf);
	$DESCRIPTOR(d_ent, " ");

	d_ent.dsc$a_pointer = e;
	d_ent.dsc$w_length = strlen(e);
	assert(d_ent.dsc$w_length > 0);

	if (SS$_NORMAL == (status = CLI$GET_VALUE(&d_ent, &d_buf, &retlength)))
	{
		if ('T' == buf[0])
			return (1);
		else  if ('F' == buf[0])
			return (0);
		else
			return (-1);
	} else
	{
		gtm_putmsg(VARLSTCNT(5) ERR_STRNOTVALID, 2, retlength, buf, status);
		return (-1);
	}
}

int4 cli_n_a_e(char *e)	/* entity */
{	/* this function retrieves a CLI value of type TRUE_ALWAYS_FALSE_NEVER_EXISTING .
	 * It assumes the presence of the qualifier in question.
	 */
	uint4 		status;
	char		buf[MAX_LINE];
	unsigned short		retlength = 0;
	$DESCRIPTOR(d_buf, buf);
	$DESCRIPTOR(d_ent, " ");

	d_ent.dsc$a_pointer = e;
	d_ent.dsc$w_length = strlen(e);
	assert(d_ent.dsc$w_length > 0);

	if (SS$_NORMAL == (status = CLI$GET_VALUE(&d_ent, &d_buf, &retlength)))
	{
		if ('F' == buf[0] || 'N' == buf[0])
			return (0);
		else if ('T' == buf[0] || 'A' == buf[0])
			return (1);
		else  if ('E' == buf[0])
			return (2);
		else
			return (-1);
	} else
	{
		gtm_putmsg(VARLSTCNT(5) ERR_STRNOTVALID, 2, retlength, buf, status);
		return (-1);
	}
}
