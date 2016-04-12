/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __GOLEVEL_H__
#define __GOLEVEL_H__
/* golevel() will unwind up to the counted frame corresponding to level specified by the parm. The first flag indicates
 * whether or not it is ok to land on a $ZINTERRUPT frame or not (which it is not ok when unwinding a $ZINTERRUPT). Also,
 * In a trigger environment, an additional flag that determines if landing on a trigger frame means the unwind should
 * continue or not is supplied. This flag is passed to goframes() which does the actual unwind of a specific number of
 * rames (counted and uncounted). Note goframes is used by both golevel and goerrorframe.
 */
#ifdef GTM_TRIGGER
#define GOLEVEL(level, unwtrigrframe)			golevel(level, unwtrigrframe)
#define GOFRAMES(frames, unwtrigrframe, fromzgoto)	goframes(frames, unwtrigrframe, fromzgoto)
void	golevel(int4 level, boolean_t unwtrigrframe);
void	goframes(int4 frames, boolean_t unwtrigrframe, boolean_t fromzgoto);
#else
#define GOLEVEL(level, unwtrigrframe)			golevel(level)
#define GOFRAMES(frames, unwtrigrframe, fromzgoto)	goframes(frames)
void	golevel(int4 level);				/* unwind upto the counted frame corresponding to frame level "level" */
void	goframes(int4 frames);				/* unwind "frames" number of frames */
#endif
void	goerrorframe(void);				/* unwind upto (but not including) the frame pointed to by the "error_frame"
							 * global
							 */
#endif
