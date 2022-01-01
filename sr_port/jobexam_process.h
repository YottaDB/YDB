/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef JOBEXAM_PROCESS_INCLUDED
#define JOBEXAM_PROCESS_INCLUDED

#include "io.h"

void jobexam_process(mval *dump_file_name, mval *zshowcodes, mval *dump_file_spec);
void jobexam_dump(mval *dump_file_name, mval *dump_file_spec, char *fatal_file_name_buff, mval *zshowcodes, io_pair *dev_in_use);

#endif
