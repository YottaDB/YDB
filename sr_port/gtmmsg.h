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

#ifndef __GTMMSG_H__
#define __GTMMSG_H__


void gtm_putmsg();
#ifdef VMS
void gtm_getmsg(uint4 msgnum, mstr *msgbuf);
#elif defined(UNIX)
void gtm_getmsg(int4 msgnum, mstr *msgbuf);
#else
#error Unsupported platform
#endif

#endif
