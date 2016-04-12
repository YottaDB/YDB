/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef JOBINTR_EVENT_INCLUDED
#define JOBINTR_EVENT_INCLUDED

UNIX_ONLY(void jobinterrupt_event(int sig, siginfo_t *info, void *context);)
VMS_ONLY(void jobinterrupt_event(void);)
void jobinterrupt_set(int4 dummy);

#endif

