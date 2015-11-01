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

#ifndef __RECORD_MSG_H__
#define __RECORD_MSG_H__

void	record_msg(int msqid, struct msgbuf *msgp, int msgsz, char *caller);
void	record_dmn_flush(int checkpoint);

#endif
