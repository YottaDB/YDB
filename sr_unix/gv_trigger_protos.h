/****************************************************************
 *								*
 *	Copyright 2010, 2014 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GV_TRIGGER_PROTOS_H_INCLUDED
#define GV_TRIGGER_PROTOS_H_INCLUDED

boolean_t	gvtr_get_hasht_gblsubs(mval *subs_mval, mval *ret_mval);
void		gvtr_db_read_hasht(sgmnt_addrs *csa);
void		gvtr_free(gv_namehead *gvt);
void		gvtr_init(gv_namehead *gvt, uint4 cycle, boolean_t tp_is_implicit, int err_code);
int		gvtr_match_n_invoke(gtm_trigger_parms *trigparms, gvtr_invoke_parms_t *gvtr_parms);
#endif
