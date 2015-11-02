/****************************************************************
 *								*
 *	Copyright 2010, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GV_TRIGGER_PROTOS_H_INCLUDED
#define GV_TRIGGER_PROTOS_H_INCLUDED

STATICFNDCL	void		gvtr_db_tpwrap_helper(sgmnt_addrs *csa, int err_code, boolean_t root_srch_needed);
STATICFNDCL	boolean_t	gvtr_get_hasht_gblsubs(mval *subs_mval, mval *ret_mval);
STATICFNDCL	boolean_t	gvtr_get_hasht_gblsubs_and_index(mval *subs_mval, mval *index, mval *ret_mval);
STATICFNDCL	uint4		gvtr_process_range(gv_namehead *gvt, gvtr_subs_t *subsdsc, int type, char *start, char *end);
STATICFNDCL	uint4		gvtr_process_pattern(char *ptr, uint4 len, gvtr_subs_t *subsdsc, gvt_trigger_t *gvt_trigger);
STATICFNDCL	uint4		gvtr_process_gvsubs(char *start, char *end, gvtr_subs_t *subsdsc,
						    boolean_t colon_imbalance, gv_namehead *gvt);
STATICFNDCL	boolean_t	gvtr_is_key_a_match(char *keysub_start[], gv_trigger_t *trigdsc, mval *lvvalarray[]);
STATICFNDCL	boolean_t	gvtr_is_value_a_match(mval *val, gv_trigger_t *trigdsc);

void	gvtr_db_read_hasht(sgmnt_addrs *csa);
void	gvtr_free(gv_namehead *gvt);
void	gvtr_init(gv_namehead *gvt, uint4 cycle, boolean_t tp_is_implicit, int err_code);
int	gvtr_match_n_invoke(gtm_trigger_parms *trigparms, gvtr_invoke_parms_t *gvtr_parms);
#endif
