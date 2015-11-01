/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __MUR_EXT_SET_H__
#define __MUR_EXT_SET_H__

void	mur_extract_set(jnl_record *rec, uint4 pid);
void	detailed_extract_set(jnl_record	*rec, uint4 pid);
void	mur_extract_pblk(jnl_record *rec, uint4 pid);
void	mur_extract_epoch(jnl_record *rec, uint4 pid);
void    mur_extract_inctn(jnl_record *rec, uint4 pid);
void	mur_extract_aimg(jnl_record *rec, uint4 pid);
void	mur_extract_tcom(jnl_record *rec, uint4 pid);

void	mur_extract_align(jnl_record *rec);
void	mur_extract_pfin(jnl_record *rec);
void	mur_extract_null(jnl_record *rec);
void	mur_extract_pini(jnl_record *rec);
void	mur_extract_eof(jnl_record *rec);
int 	extract_process_vector(jnl_process_vector *pv, int extract_len);

#endif

