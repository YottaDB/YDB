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
#include <descrip.h>
#include <iodef.h>
#include <ssdef.h>
#include <efndef.h>

#include "comline.h"
#include "gtm_caseconv.h"
#include "m_recall.h"

#define DEF_BUF_SZ		1024	/* from IOTT_OPEN.C */
#define	CCPROMPT 		0x00010000
#define ERR_RECBADNUM		"Recall error: bad numeric argument format"
#define ERR_RECNOTFND		"Recall error: command not found"
#define ERR_RECOUTOFRANGE	"Recall error: numeric argument out of range"

GBLREF mstr		*comline_base;
GBLREF int		comline_index;

int m_recall (short len, char *addr, int4 *index, short tt_channel)
{
	unsigned char	*cp, *cp_top;
	unsigned char	recbuf[8];
	unsigned char	faobuf[DEF_BUF_SZ];
	unsigned short	reclen, arglen, iosb[4];
	unsigned short	faolen;
 	unsigned int	n, cl;
	$DESCRIPTOR	(faodsc, faobuf);
	static readonly $DESCRIPTOR	(ctrstr, "!2SL !AD");

	cp = addr; cp_top = cp + len;
	while (cp < cp_top && *cp != SP && *cp != '\t')
		cp++;
	reclen = (char *) cp - addr;
	if (reclen != 3 && reclen != 6)
		return FALSE;

	lower_to_upper (recbuf, addr, reclen);
	if (memcmp (recbuf, "RECALL", reclen))
		return FALSE;

	while (cp < cp_top && (*cp == SP || *cp == '\t'))
		cp++;
	if (cp == cp_top)
	{
		flush_pio ();
		cl = comline_index;
		n = 1;
		do
		{
			cl = clmod (cl - 1);
			if (!comline_base[cl].len)
				break;
			sys$fao(&ctrstr, &faolen, &faodsc, n++, comline_base[cl].len, comline_base[cl].addr);

			/* don't do status on terminal qiow'w in this module,
			as errors should be unlikely and would cause messy exit from dm_read
			if you don't like it revise the interface between the routines */

 			sys$qiow(EFN$C_ENF, tt_channel, IO$_WRITEVBLK, &iosb, 0, 0, faobuf, faolen, 0, CCPROMPT, 0, 0);
		} while (cl != comline_index);
		assert (!comline_base[cl].len || (n == MAX_RECALL + 1 && cl == comline_index));
		*index = 0;
		return TRUE;
	}
	/* cp now points to beginning of arg */

	/* throw away trailing whitespace */
	while (*--cp_top == SP || *cp_top == '\t')
		assert(cp < cp_top);
	cp_top++;
	arglen = cp_top - cp;

	if ('0' <= *cp && *cp <= '9')	/* numeric argument */
	{
		n = 0;
		while (cp < cp_top && *cp != SP && *cp != '\t')
		{
			n *= 10;
			if ('0' <= *cp && *cp <= '9')
				n += (*cp++ - '0');
			else
			{
				sys$qiow(EFN$C_ENF, tt_channel, IO$_WRITEVBLK, &iosb, 0, 0,
					ERR_RECBADNUM, sizeof ERR_RECBADNUM - 1, 0, CCPROMPT, 0, 0);
				*index = -1;
				return TRUE;
			}
		}
		if (n <= 0 || n > MAX_RECALL)
		{
			sys$qiow(EFN$C_ENF, tt_channel, IO$_WRITEVBLK, &iosb, 0, 0,
				ERR_RECOUTOFRANGE, sizeof ERR_RECOUTOFRANGE - 1, 0, CCPROMPT, 0, 0);
			*index = -1;
			return TRUE;
		}
		assert (arglen == 1 || arglen == 2);
	}
	else				/* string argument */
	{
		cl = comline_index;
		n = 1;
		do
		{
			cl = clmod (cl - 1);
			if (!comline_base[cl].len) break;
			if (!memcmp (comline_base[cl].addr, cp, arglen))
				break;
			n++;
		} while (cl != comline_index);
		if (!comline_base[cl].len || cl == comline_index)
		{
			sys$qiow(EFN$C_ENF, tt_channel, IO$_WRITEVBLK, &iosb, 0, 0,
				ERR_RECNOTFND, sizeof ERR_RECNOTFND - 1, 0, CCPROMPT, 0, 0);
			*index = -1;
			return TRUE;
		}
	}
	assert (0 < n && n <= MAX_RECALL);
	*index = n;
	return TRUE;
}
