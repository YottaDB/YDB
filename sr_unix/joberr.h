/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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
	"Job error in child process", SIZEOF("Job error in child process")-1,
	"Job error in I/O specification", SIZEOF("Job error in I/O specification")-1,
	"Job error in directory specification", SIZEOF("Job error in directory specification")-1,
	"Job error in routine specification", SIZEOF("Job error in routine specification")-1,
	"Job error in fork", SIZEOF("Job error in fork")-1,
	"Job error in syscall", SIZEOF("Job error in syscall")-1,
	"Job child was stopped by signal", SIZEOF("Job child was stopped by signal")-1,
	"Job child terminated due to signal", SIZEOF("Job child terminated due to signal")-1,
	"", SIZEOF("")-1	/* this is used internally to determine try-again situations */
};

