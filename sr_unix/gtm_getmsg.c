/****************************************************************
 *								*
 * Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries. *
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
#ifdef SYS_ERRLIST_INCLUDE
# include SYS_ERRLIST_INCLUDE
#endif

#include "error.h"
#include "gtmmsg.h"

GBLREF bool	dec_nofac;

#define ERR_TAG		"ENO"

/* #YDB_THREAD_SAFE : The below function (gtm_getmsg) is multi-thread-safe.
 * Note: This is being relied upon by the SET_STAPI_ERRSTR_MULTI_THREAD_SAFE macro which calls this function.
 */
int gtm_getmsg(int4 msgnum, mstr *msgbuf)
{
	short int	m_len, faclen, taglen, j, sever;
	int		retval;
	char		*cp;
	const char 	*top, *msgp, *fac;
	char		outbuf[32];
	char_ptr_t	tag;
	const err_msg	*msg;
	const err_ctl	*ctl;

	ctl = err_check(msgnum);
	retval = 0;
	if (NULL != ctl)
	{
		GET_MSG_INFO(msgnum, ctl, msg);
		msgp = msg->msg;
		tag = (char_ptr_t)msg->tag;
		fac = ctl->facname;
		sever = SEVMASK(msgnum);
	} else
	{
		sever = ERROR;
		tag = (char_ptr_t)outbuf;
		if ((MAX_SYSERR > msgnum) && (msgnum > 0))
		{
			assert(NULL != STRERROR(1));	/* OSF/1 check; can happen with 64-bit pointers and bad declaration */
			cp = (char *)tag;
			MEMCPY_LIT(cp, ERR_TAG);
			cp += strlen(ERR_TAG);
			cp = (char *)i2asc((uchar_ptr_t)cp, msgnum);
			*cp = '\0';
			msgp = STRERROR(msgnum);
		} else
		{
			tag = "UNKNOWN";
			msgp = "Unknown system error !SL";
			retval = ERR_UNKNOWNSYSERR;	/* This is an unknown system error */
		}
		fac = "SYSTEM";
	}
	m_len = strlen(msgp);
	if (!dec_nofac)
	{
		m_len += (faclen = strlen(fac));
		m_len += 4;	/* %-<sev>- */
		m_len += (taglen = strlen((const char *)tag));
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
	return retval;
}
