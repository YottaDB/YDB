/****************************************************************
 *								*
 * Copyright (c) 2005-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef VERIFY_DB_FORMAT_CHANGE_REQUEST

/* prototypes */

int4	verify_db_format_change_request(gd_region *reg, enum db_ver new_db_format, char *command_name);

#define	WCS_PHASE2_COMMIT_WAIT_LIT	"wcb_phase2_commit_wait"

#define VERIFY_DB_FORMAT_CHANGE_REQUEST
#endif
