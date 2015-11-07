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
#include <ssdef.h>

void gtm_getmsg(msgnum, msgbuf)
uint4 msgnum;
mstr *msgbuf;
{
	int4 status;
	unsigned short m_len;
	$DESCRIPTOR(d_sp,"");

	d_sp.dsc$a_pointer = msgbuf->addr;
	d_sp.dsc$w_length = --msgbuf->len;	/* reserve a byte for a <NUL> terminator */

	status = sys$getmsg(msgnum, &m_len, &d_sp, 0, 0);
	if (status == SS$_NORMAL || status == SS$_BUFFEROVF)
	{
		assert(m_len <= msgbuf->len);
		msgbuf->len = m_len;
	}
	else
		msgbuf->len = 0;
	*(char *)(msgbuf->addr + msgbuf->len) = 0;	/* add a null terminator */
}
