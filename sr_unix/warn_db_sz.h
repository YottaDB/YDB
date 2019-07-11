/****************************************************************
 *								*
 * Copyright (c) 2018-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef WARN_DB_SZ_H_INCLUDED
#define WARN_DB_SZ_H_INCLUDED

#include "gdsroot.h"

void warn_db_sz(char *db_fname, block_id prev_blocks, block_id curr_blocks, block_id tot_blocks);
#endif /* WARN_DB_SZ_H_INCLUDED */
