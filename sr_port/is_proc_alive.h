/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef IS_PROC_ALIVE_INCLUDED
#define IS_PROC_ALIVE_INCLUDED

uint4 getpstart(int4 pid);
bool is_proc_alive(int4 pid, uint4 pstarttime);

#endif /* IS_PROC_ALIVE_INCLUDED */
