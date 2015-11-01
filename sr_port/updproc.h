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

#ifndef UPDPROC_INCLUDED
#define UPDPROC_INCLUDED

int updproc_init(void);
int updproc_log_init(void);
int updproc(void);
void updproc_actions(void);
void updproc_sigstop(void);
void updproc_end(void);

#endif /* UPDPROC_INCLUDED */
