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

typedef struct joberr_msg_struct{
char		*msg;
int		len;
}joberr_msg;

LITDEF joberr_msg joberrs[] = {
	"", 0,
	"Job error in child process", sizeof("Job error in child process")-1,
	"Job error in I/O specification", sizeof("Job error in I/O specification")-1,
	"Job error in directory specification", sizeof("Job error in directory specification")-1,
	"Job error in routine specification", sizeof("Job error in routine specification")-1,
	"Job error in fork", sizeof("Job error in fork")-1,
	"Job error in syscall", sizeof("Job error in syscall")-1,
	"Job child was stopped by signal", sizeof("Job child was stopped by signal")-1,
	"Job child terminated due to signal", sizeof("Job child terminated due to signal")-1,
	"", sizeof("")-1	/* this is used internally to determine try-again situations */
};

