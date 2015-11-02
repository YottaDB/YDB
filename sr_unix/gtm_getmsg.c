/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"
#ifdef SYS_ERRLIST_INCLUDE
#include SYS_ERRLIST_INCLUDE
#endif
#include "error.h"
#include "gtmmsg.h"

GBLREF bool	dec_nofac;
#ifndef SYS_ERRLIST_INCLUDE
/* sys_nerr and sys_errlist defined in stdio for linux */
GBLREF int	sys_nerr;
#endif
LITREF char	*sys_errnolist[];
LITREF int	sys_nerrno;


#ifndef SYS_ERRLIST_INCLUDE

#ifdef	__osf__
#pragma pointer_size (save)
#pragma pointer_size (long)
#endif

extern char	*sys_errlist[];

#ifdef	__osf__
#pragma pointer_size (restore)
#endif

#endif

void	gtm_getmsg (int4 msgnum, mstr *msgbuf)
{
	short int	m_len, faclen, taglen, j, sever;
	char 		*cp;
	const char 	*top, *msgp, *fac, *tag;
	const err_msg	*msg;
	const err_ctl	*ctl;

	ctl = err_check(msgnum);
	if (ctl != 0)
	{
		assert((msgnum & FACMASK(ctl->facnum)) && (MSGMASK(msgnum, ctl->facnum) <= ctl->msg_cnt));
		j = MSGMASK(msgnum, ctl->facnum);
		msg = ctl->fst_msg + j - 1;
		msgp = msg->msg;
		tag = msg->tag;
		fac = ctl->facname;
		sever = SEVMASK(msgnum);
	} else
	{
		sever = ERROR;

                if ((msgnum < sys_nerrno) && (msgnum > 0))
			tag = sys_errnolist[msgnum];
		else
		{
#ifdef DEBUG
			/* Below code commented out for now since it was triggered by ZMessage with invalid msg number given */
			/*PRN_ERROR;*/			/* Flush what we have so far prior to the assert failure */
			/*assert(FALSE);*/		/* Will cause an error within the error but we will catch where this
							   happens so we can fix them */
#endif
			tag = "UNKNOWN";
		}

#ifndef __MVS__
		assert (sys_errlist[1] != 0);		/* OSF/1 check; can happen with 64-bit pointers and bad declaration */
                if ((msgnum < sys_nerr) && (msgnum > 0))
#else
                if ((msgnum < MAX_SYSERR) && (msgnum > 0))
#endif
			msgp = STRERROR(msgnum);
		else
			msgp = "Unknown system error !SL";

		fac = "SYSTEM";
	}
	m_len = strlen(msgp);
	if (!dec_nofac)
	{
		m_len += (faclen = strlen(fac));
		m_len += 4;	/* %-<sev>- */
		m_len += (taglen = strlen(tag));
		m_len += 2;	/* ,  */
	}
	m_len = m_len > msgbuf->len - 1 ? msgbuf->len - 1 : m_len;
	cp = msgbuf->addr;
	top = cp + m_len;
	if (!dec_nofac)
	{
		if (cp < top)
			*cp++ = '%';

		j = faclen > top-cp ? top-cp : faclen;
		if (j)
		{
			memcpy(cp, fac, j);
			cp += j;
		}
		if (cp < top)
			*cp++ = '-';
		if (cp < top)
		{
			switch(sever)
			{
			case SUCCESS:	*cp++ = 'S'; break;
			case INFO:	*cp++ = 'I'; break;
			case WARNING:	*cp++ = 'W'; break;
			case ERROR:	*cp++ = 'E'; break;
			case SEVERE:	*cp++ = 'F'; break;
			default:	*cp++ = 'U'; break;
			}
		}
		if (cp < top)
			*cp++ = '-';
		j = taglen > top-cp ? top-cp : taglen;
		if (j)
		{
			memcpy(cp, tag, j);
			cp += j;
		}
		if (cp < top)
			*cp++ = ',';
		if (cp < top)
			*cp++ = ' ';
	}

	memcpy(cp, msgp, top - cp);
	cp += top - cp;
	msgbuf->len = m_len;
	*cp++ = 0;
}
