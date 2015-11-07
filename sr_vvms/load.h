/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef LOAD_INCLUDED
#define LOAD_INCLUDED

void bin_load(uint4 begin, uint4 end, struct RAB *inrab, struct FAB *infab);
void go_load(uint4 begin, uint4 end, struct RAB *inrab, struct FAB *infab);
void goq_load(uint4 begin, uint4 end, struct FAB *infab);
void goq_m11_load(struct FAB *infab, char *in_buff, uint4 rec_count, uint4 end);
void goq_mvx_load(struct FAB *infab, char *in_buff, uint4 rec_count, uint4 end);

#endif /* LOAD_INCLUDED */
