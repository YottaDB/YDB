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

#ifndef GTMMSG_H_INCLUDED
#define GTMMSG_H_INCLUDED


#ifdef VMS
void gtm_getmsg(uint4 msgnum, mstr *msgbuf);
void gtm_putmsg(int4 msgid, ...);
#elif defined(UNIX)
void gtm_getmsg(int4 msgnum, mstr *msgbuf);
void gtm_putmsg(int argcnt, ...);
void gtm_putmsg_noflush(int argcnt, ...);
#else
#error Unsupported platform
#endif

#endif /* GTMMSG_H_INCLUDED */
