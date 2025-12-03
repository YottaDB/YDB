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

#define MLK_LOGIN(x)
#define BLOCKING_PROC_DEAD(w,x,y,z) (!is_proc_alive((w)->blocked->owner, (w)->blocked->pstart))
#define PROC_DEAD(w,x,y,z) (!is_proc_alive((w)->owner, (w)->pstart))
