/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef UPDPROC_INCLUDED
#define UPDPROC_INCLUDED

enum upd_bad_trans_type
{
	upd_good_record = 0,
	upd_bad_backptr,
	upd_rec_not_replicated,
	upd_bad_jnl_seqno,
	upd_bad_key_size,
	upd_bad_val_size,
	upd_fence_bad_t_num,
	upd_nofence_bad_tupd_num
};

int updproc_init(void);
int updproc_log_init(void);
int updproc(void);
void updproc_actions(void);
void updproc_sigstop(void);
void updproc_end(void);

#endif /* UPDPROC_INCLUDED */
