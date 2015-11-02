/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef TARG_ALLOC_INCLUDED
#define TARG_ALLOC_INCLUDED

gv_namehead	*targ_alloc(int keysize, mname_entry *gvent, gd_region *reg);
void		targ_free(gv_namehead *gvt);

#endif /* TARG_ALLOC_INCLUDED */
