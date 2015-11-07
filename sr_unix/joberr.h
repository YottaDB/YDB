/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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

/*
 * The follwoing array is index by values from the enum joberr_t from jobsp.h.
 */
LITDEF joberr_msg joberrs[] = {
	"", 0,
	LIT_AND_LEN("Job error in child process"),
	LIT_AND_LEN("Job error in opening STDIN"),
	LIT_AND_LEN("Job error in directing input to STDIN"),
	LIT_AND_LEN("Job error in creating STDOUT"),
	LIT_AND_LEN("Job error in opening STDOUT"),
	LIT_AND_LEN("Job error in directing output to STDOUT"),
	LIT_AND_LEN("Job error in creating STDERR"),
	LIT_AND_LEN("Job error in opening STDERR"),
	LIT_AND_LEN("Job error in directing output to STDERR"),
	LIT_AND_LEN("Job error in directory specification"),
	LIT_AND_LEN("Job error - CHDIR error"),
	LIT_AND_LEN("Job error in routine specification. Label and offset not found in created process"),
	LIT_AND_LEN("Job error in setting independent session"),
	LIT_AND_LEN("Job error in fork"),
	LIT_AND_LEN("Job error in renaming standard output file"),
	LIT_AND_LEN("Job error in renaming standard error file"),
	LIT_AND_LEN("Job error in middle process to parent process pipe communication"),
	LIT_AND_LEN("Job error in middle process to grandchild process pipe communication"),
	LIT_AND_LEN("Job child was stopped by signal"),
	LIT_AND_LEN("Job child terminated due to signal"),
	LIT_AND_LEN("") 	/* this is used internally to determine try-again situations */
};

