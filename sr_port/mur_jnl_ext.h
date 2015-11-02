/****************************************************************
 *								*
 *	Copyright 2003, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MUR_EXT_SET_H
#define MUR_EXT_SET_H

void	mur_extract_set(jnl_ctl_list *jctl, fi_type *fi, jnl_record *rec, pini_list_struct *plst);
void	mur_extract_null(jnl_ctl_list *jctl, fi_type *fi, jnl_record *rec, pini_list_struct *plst);
void	mur_extract_align(jnl_ctl_list *jctl, fi_type *fi, jnl_record *rec, pini_list_struct *plst);
void	mur_extract_blk(jnl_ctl_list *jctl, fi_type *fi, jnl_record *rec, pini_list_struct *plst);
void	mur_extract_epoch(jnl_ctl_list *jctl, fi_type *fi, jnl_record *rec, pini_list_struct *plst);
void    mur_extract_inctn(jnl_ctl_list *jctl, fi_type *fi, jnl_record *rec, pini_list_struct *plst);
void	mur_extract_eof(jnl_ctl_list *jctl, fi_type *fi, jnl_record *rec, pini_list_struct *plst);
void	mur_extract_trunc(jnl_ctl_list *jctl, fi_type *fi, jnl_record *rec, pini_list_struct *plst);
void	mur_extract_pfin(jnl_ctl_list *jctl, fi_type *fi, jnl_record *rec, pini_list_struct *plst);
void	mur_extract_pini(jnl_ctl_list *jctl, fi_type *fi, jnl_record *rec, pini_list_struct *plst);
void	mur_extract_tcom(jnl_ctl_list *jctl, fi_type *fi, jnl_record *rec, pini_list_struct *plst);
int 	extract_process_vector(jnl_process_vector *pv, int extract_len);
#endif
