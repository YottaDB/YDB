/****************************************************************
 *								*
 *	Copyright 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GVNH_SPANREG_INCLUDED
#define GVNH_SPANREG_INCLUDED

#include "view.h"	/* needed for "viewparm" */

#define	ADD_GVT_TO_VIEW_NOISOLATION_LIST(GVT, PARMBLK)					\
{											\
	noisolation_element	*gvnh_entry;						\
											\
	GBLREF	buddy_list	*noisolation_buddy_list;				\
											\
	gvnh_entry = (noisolation_element *)get_new_element(noisolation_buddy_list, 1);	\
	gvnh_entry->gvnh = GVT;								\
	gvnh_entry->next = PARMBLK->ni_list.gvnh_list;					\
	PARMBLK->ni_list.gvnh_list = gvnh_entry;					\
}

void gvnh_spanreg_init(gvnh_reg_t *gvnh_reg, gd_addr *addr, gd_binding *gvmap_start);
void gvnh_spanreg_subs_gvt_init(gvnh_reg_t *gvnh_reg, gd_addr *addr, viewparm *parmblk);
boolean_t gvnh_spanreg_ismapped(gvnh_reg_t *gvnh_reg, gd_addr *addr, gd_region *reg);

#endif /* GVNH_SPANREG_INCLUDED */
