/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef RC_CPT_OPS_H_INCLUDED
#define RC_CPT_OPS_H_INCLUDED

#ifndef GDSROOT_H
/*This is a duplicate of the definition in gdsroot.h to keep from having to #include it.
 * If that definition changes then so should this.
 */
typedef gtm_int8	block_id;
#endif

int rc_cpt_entry(block_id blk);
int rc_cpt_inval(void);
void rc_close_section(void);
int mupip_rundown_cpt(void);
void rc_delete_cpt(void);
int rc_create_cpt(void);
short rc_get_cpsync(void);

#endif
