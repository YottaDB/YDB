/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef SOURCE_FILE_INCLUDED
#define SOURCE_FILE_INCLUDED

void		compile_source_file(unsigned short flen, char *faddr, boolean_t mExtReqd);
boolean_t	open_source_file (void);
int4		read_source_file (void);
void		close_source_file (void);

#define REV_TIME_BUFF_LEN	20

#endif /* SOURCE_FILE_INCLUDED */
