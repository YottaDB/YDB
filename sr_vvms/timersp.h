/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef TIMERSP_included
#define TIMERSP_included

#define TIMER_SCALE		-10000		/* 100 nanoseconds / millisecond (delta time representations are negative) */

/* These values are used during file creation  but may be changed on the fly */
#define TIM_FLU_MOD_BG		(1000 * TIMER_SCALE)	/* 1 sec */
#define TIM_FLU_MOD_MM		(30000 * TIMER_SCALE)	/* 30 sec - longer since is a full sync */
#define TIM_AST_WAIT		(5 * TIMER_SCALE)	/* 5 msec */

#endif /*TIMERSP_included */
