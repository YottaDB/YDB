/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef T_ABORT_DEFINED
#define T_ABORT_DEFINED

void	t_abort_cleanup(void);
void	t_abort(gd_region *reg, sgmnt_addrs *csa);

#define CLEAR_CSE(GVNH)							\
{									\
	gv_namehead		*gvnh;					\
	srch_blk_status		*s;					\
									\
	gvnh = GVNH;							\
	if (NULL != gvnh)						\
	{								\
		for (s = &gvnh->hist.h[0]; s->blk_num; s++)		\
			s->cse = NULL;					\
	}								\
}

#define	RESET_BML_SAVE_DOLLAR_TLEVEL				\
{								\
	GBLREF	uint4		bml_save_dollar_tlevel;		\
								\
	if (bml_save_dollar_tlevel)				\
	{							\
		assert(!dollar_tlevel);				\
		dollar_tlevel = bml_save_dollar_tlevel;		\
		bml_save_dollar_tlevel = 0;			\
	}							\
}

#endif
