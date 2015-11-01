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


typedef struct {
	opctype opcode;
	bool can_set;
	char os_syst;
} svn_data_type;

typedef struct{
    opctype opcode;
    char os_syst;
} fun_data_type;

