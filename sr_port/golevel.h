/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __GOLEVEL_H__
#define __GOLEVEL_H__

void	golevel(int4 level);		/* unwind upto the counted frame corresponding to frame level "level" */
void	goerrorframe(stack_frame *fp);	/* unwind upto (but not including) the frame pointed to by "fp" */

/* both golevel() and goerror() use goframes() */
void	goframes(int4 frames);		/* unwind "frames" number of frames */

#endif
