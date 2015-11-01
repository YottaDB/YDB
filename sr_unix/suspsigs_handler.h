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

#ifndef __SUSPSIGS_HANDLER_H__
#define __SUSPSIGS_HANDLER_H__

#ifdef __sparc
void suspsigs_handler(int sig);
#else
void suspsigs_handler(int sig, siginfo_t *info, void *context);
#endif

#endif
