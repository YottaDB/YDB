/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef JNL_WRITE_H_INCLUDED
#define JNL_WRITE_H_INCLUDED

/* We do not put this in jnl.h, because it needs all including jnl.h must include gdsblk.h */
void	jnl_write(jnl_private_control *jpc, enum jnl_record_type rectype, jnl_record *jnl_rec, void *parm1);

int jnl_write_extend_if_needed(int4 jrec_len, jnl_buffer_ptr_t jb, uint4 lcl_freeaddr, sgmnt_addrs *csa,
				enum jnl_record_type rectype, gd_region *reg, jnl_private_control *jpc, jnl_record *jnl_rec);

#endif
