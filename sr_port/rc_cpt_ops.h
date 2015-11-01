/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __RC_CPT_OPS_H__
#define __RC_CPT_OPS_H__

int rc_cpt_entry(int blk);
int rc_cpt_inval(void);
void rc_close_section(void);
int mupip_rundown_cpt(void);
void rc_delete_cpt(void);
int rc_create_cpt(void);
short rc_get_cpsync(void);

#endif
