/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

#define ERR_TAG		"ENO"


void	gtm_getmsg(int4 msgnum, mstr *msgbuf)
{
	short int	severity;
	char		*cp;
	const char 	*top, *msgp, *fac, *base;
	char		outbuf[32];
	char_ptr_t	tag;
	const err_msg	*msg;
	const err_ctl	*ctl;
	size_t		cp_len, faclen, m_len, taglen;

	assert(0 < msgbuf->len);	/* All callers pass in a large buffer */
	ctl = err_check(msgnum);
	if (NULL != ctl)
	{
		GET_MSG_INFO(msgnum, ctl, msg);
		msgp = msg->msg;
		tag = (char_ptr_t)msg->tag;
		fac = (const char *)ctl->facname;
		severity = (short int)SEVMASK(msgnum);
	} else
	{
		severity = ERROR;
		tag = (char_ptr_t)outbuf;
		if ((MAX_SYSERR > msgnum) && (msgnum > 0))
		{
			assert(NULL != STRERROR(1));	/* OSF/1 check; can happen with 64-bit pointers and bad declaration */
			cp = (char *)tag;
			MEMCPY_LIT(cp, ERR_TAG);
			cp += STR_LIT_LEN(ERR_TAG);
			cp = (char *)i2asc((uchar_ptr_t)cp, msgnum);
			*cp = '\0';
			msgp = STRERROR(msgnum);
		} else
		{
			tag = "UNKNOWN";
			msgp = "Unknown system error !SL";
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
	/* Subtract the max length by 1 to leave space for terminating null */
	m_len = (m_len > (msgbuf->len - 1)) ? (msgbuf->len - 1) : m_len;
	base = (const char *)msgbuf->addr;
	cp = (char *)base;
	top = (const char *)(cp + m_len);
	if (!dec_nofac)
	{
		/* Use a loop construct to fill the message buffer with as much
		 * information as possible. At any point in the loop, if the
		 * buffer becomes full, a break statement will exit the loop,
		 * terminating the the buffer filling.
		 */
		do {
			if (cp >= top)
				break;
			*cp++ = '%';
			cp_len = (size_t)((faclen > (top - cp)) ? (top - cp) : faclen);
			if (cp_len)
			{
				memcpy(cp, fac, cp_len);
				cp += cp_len;
			}
			if (cp >= top)
				break;
			*cp++ = '-';
			if (cp >= top)
				break;
			switch(severity)
			{
				case SUCCESS:
					*cp++ = 'S';
					break;
				case INFO:
					*cp++ = 'I';
					break;
				case WARNING:
					*cp++ = 'W';
					break;
				case ERROR:
					*cp++ = 'E';
					break;
				case SEVERE:
					*cp++ = 'F';
					break;
				default:
					*cp++ = 'U';
					break;
			}
			if (cp >= top)
				break;
			*cp++ = '-';
			cp_len = (size_t)((taglen > (top - cp)) ? (top - cp) : taglen);
			if (cp_len)
			{
				memcpy(cp, tag, cp_len);
				cp += cp_len;
			}
			if (cp >= top)
				break;
			*cp++ = ',';
			if (cp >= top)
				break;
			*cp++ = ' ';
			break;
		} while (0);
	}
	assert((top - cp) <= (m_len - (cp - base)));
	assert(0 <= (top - cp));
	cp_len = (size_t)(top - cp);
	memcpy(cp, msgp, cp_len);
	cp += cp_len;
	/* The buffer size calculation always leaves space for a null character */
	assert((int)(cp - base) < msgbuf->len);
	*cp = 0;
	msgbuf->len = (int)m_len;
}
