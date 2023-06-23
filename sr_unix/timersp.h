/****************************************************************
 *								*
 * Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef TIMERSP_included
#define TIMERSP_included

#define TIMER_SCALE		1

/* These values are used during file creation  but may be changed on the fly */
#define TIM_FLU_MOD_BG		((uint8)NANOSECS_IN_SEC * TIMER_SCALE)	/* 1 sec */
#define TIM_FLU_MOD_MM		((uint8)NANOSECS_IN_SEC * TIMER_SCALE)	/* 1 sec */

#endif /* TIMERSP_included */
