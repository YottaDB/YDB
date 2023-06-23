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

/* gtm_pipe.h */
typedef enum
{
	input_from_comm = 0,
	output_to_comm
} pipe_type;

int gtm_pipe(char *command, pipe_type pt);
