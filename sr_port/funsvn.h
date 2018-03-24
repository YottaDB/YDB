/****************************************************************
 *								*
 * Copyright 2001 Sanchez Computer Associates, Inc.		*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef FUNSVN_H
#define FUNSVN_H

typedef struct {
	opctype opcode;
	bool can_set;
	char os_syst;
} svn_data_type;

typedef struct{
    opctype opcode;
    char os_syst;
} fun_data_type;

#endif /*  FUNSVN_H */
