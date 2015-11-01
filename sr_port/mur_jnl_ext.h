/****************************************************************
 *								*
 *	Copyright 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MUR_EXT_SET_H
#define MUR_EXT_SET_H

void	mur_extract_set   (fi_type *fi, jnl_record *rec, pini_list_struct *plst);
void	mur_extract_blk  (fi_type *fi, jnl_record *rec, pini_list_struct *plst);
void	mur_extract_epoch (fi_type *fi, jnl_record *rec, pini_list_struct *plst);
void    mur_extract_inctn (fi_type *fi, jnl_record *rec, pini_list_struct *plst);
void	mur_extract_tcom  (fi_type *fi, jnl_record *rec, pini_list_struct *plst);
void	mur_extract_align (fi_type *fi, jnl_record *rec, pini_list_struct *plst);
void	mur_extract_pfin  (fi_type *fi, jnl_record *rec, pini_list_struct *plst);
void	mur_extract_null  (fi_type *fi, jnl_record *rec, pini_list_struct *plst);
void	mur_extract_pini  (fi_type *fi, jnl_record *rec, pini_list_struct *plst);
void	mur_extract_eof   (fi_type *fi, jnl_record *rec, pini_list_struct *plst);
int 	extract_process_vector(jnl_process_vector *pv, int extract_len);

#endif
