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
#include <ssdef.h>
#include <jpidef.h>
#include <accdef.h>
#include "vmsdtype.h"

#define	TERM_READ_MIN_BCOUNT	1036
#define	CREMBX_OVRHD		144

bool ojchkbytcnt (cmaxmsg)
int4	cmaxmsg;
{
	int4		preq, creq;
	int4		status;
	int4		bytcnt;
	unsigned short	ret;
	int4		iosb[2];
	struct
	{
		item_list_3	item[1];
		int4		terminator;
	} jpi_list = {4, JPI$_BYTCNT, &bytcnt, &ret, 0};

	preq = CREMBX_OVRHD + ACC$K_TERMLEN;
	creq = CREMBX_OVRHD + cmaxmsg;

	status = sys$getjpi (0, 0, 0, &jpi_list, &iosb[0], 0, 0);
	if (status != SS$_NORMAL) rts_error(VARLSTCNT(1) status);
	sys$synch (0, &iosb[0]);
	if (iosb[0] != SS$_NORMAL) rts_error(VARLSTCNT(1) iosb[0]);

	if (bytcnt < preq + creq) return FALSE;
	if (bytcnt - creq < TERM_READ_MIN_BCOUNT) return FALSE;
	return TRUE;
}

