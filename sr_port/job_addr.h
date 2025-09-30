/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef JOB_ADDR_H_INCLUDED
#define JOB_ADDR_H_INCLUDED

boolean_t job_addr(mstr *rtn, mstr *label, int4 offset, char **hdr, char **labaddr, boolean_t *need_rtnobj_shm_free);

#endif
