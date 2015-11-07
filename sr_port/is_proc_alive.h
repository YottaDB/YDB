/****************************************************************
 *								*
 * Copyright (c) 2001, 2015 Fidelity National Information	*
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

#define	IMAGECNT(imagecnt)	VMS_ONLY(imagecnt) UNIX_ONLY(0)

#ifdef VMS
bool is_proc_alive(uint4 pid, uint4 imagecnt);
#elif defined(UNIX)
bool is_proc_alive(int4 pid, int4 imagecnt);
#endif

#endif /* IS_PROC_ALIVE_INCLUDED */
