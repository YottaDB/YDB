/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef COLLSEQ_H_INCLUDED
#define COLLSEQ_H_INCLUDED

#include "min_max.h"

#define MAX_COLLTYPE	255
#define MIN_COLLTYPE	0
#define XFORM	0
#define XBACK	1

#ifdef UNIX					/* environment variable syntax is OS dependent */
#	define	CT_PREFIX	"$gtm_collate_"
#	define LCT_PREFIX	"$gtm_local_collate"
#elif defined VMS
#	define	CT_PREFIX	"GTM_COLLATE_"
#	define LCT_PREFIX	"GTM_LOCAL_COLLATE"
#else
#error UNSUPPORTED PLATFORM
#endif

#define ALLOC_XFORM_BUFF(str)							\
if ((str)->len > max_lcl_coll_xform_bufsiz)					\
{										\
	if (0 == max_lcl_coll_xform_bufsiz)					\
	{									\
		assert(NULL == lcl_coll_xform_buff);				\
		max_lcl_coll_xform_bufsiz = MAX_STRBUFF_INIT;			\
	} else									\
	{									\
		assert(NULL != lcl_coll_xform_buff);				\
		free(lcl_coll_xform_buff);					\
	}									\
	while ((str)->len > max_lcl_coll_xform_bufsiz)				\
		max_lcl_coll_xform_bufsiz += max_lcl_coll_xform_bufsiz;		\
	max_lcl_coll_xform_bufsiz = MIN(MAX_STRLEN, max_lcl_coll_xform_bufsiz);	\
	lcl_coll_xform_buff = (char *)malloc(max_lcl_coll_xform_bufsiz);	\
}

typedef struct collseq_struct {
	struct collseq_struct	*flink;
	int			act;
	int4			(*xform)();
	int4			(*xback)();
	int4			(*version)();
	int4			(*verify)();
	int			argtype;
} collseq;

GBLREF char	*lcl_coll_xform_buff;
GBLREF	int	max_lcl_coll_xform_bufsiz;

boolean_t map_collseq(mstr *fspec, collseq *ret_collseq);
collseq *ready_collseq(int act);
int4 do_verify(collseq *csp, unsigned char type, unsigned char ver);
int find_local_colltype(void);
void act_in_gvt(void);

#endif /* COLLSEQ_H_INCLUDED */
