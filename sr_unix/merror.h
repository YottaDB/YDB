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

typedef struct err_msg_struct
{
	char		*tag;
	char		*msg;
	int		parm_count;
} err_msg;

typedef struct err_ctl_struct
{
	int		facnum;
	char		*facname;
	err_msg		*fst_msg;
	int		msg_cnt;
} err_ctl;

#define FCNTL		1
#define MSGCNTL		27
#define MSGFAC		16
#define MSGNBIT		15
#define MSGSEVERITY	3
#define MSGNUM		3

#define FACMASK(fac)		(FCNTL << MSGCNTL  |  1 << MSGNBIT  |  (fac) << MSGFAC)
#define MSGMASK(msg,fac)	(((msg) & ~FACMASK(fac)) >> MSGSEVERITY)
#define SEVMASK(msg)		((msg) & 7)

err_ctl *err_check(int err);

