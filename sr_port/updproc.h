/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef UPDPROC_INCLUDED
#define UPDPROC_INCLUDED

#define SKIP_REC 5

enum upd_bad_trans_type
{
	upd_good_record = 0,
	upd_bad_forwptr,
	upd_bad_backptr,
	upd_rec_not_replicated,
	upd_bad_jnl_seqno,
	upd_bad_mname_size,
	upd_bad_key_size,
	upd_bad_val_size,
	upd_fence_bad_t_num,
	upd_nofence_bad_tupd_num
};

int 		updproc(void);
void 		updproc_actions(gld_dbname_list *gld_db_files);
int 		updproc_init(gld_dbname_list **gld_db_files , seq_num *start_jnl_seqno);
int 		upd_log_init(recvpool_user);
boolean_t 	updproc_open_files(gld_dbname_list **gld_db_files, seq_num *start_jnl_seqno);
void 		updproc_stop(boolean_t exit);
void 		updproc_sigstop(void);
void 		updproc_end(void);
void 		updhelper_init(recvpool_user);
int 		updhelper_reader(void);
boolean_t 	updproc_preread(void);
void 		updhelper_reader_end(void);
void 		updhelper_reader_sigstop(void);
int 		updhelper_writer(void);
void 		updhelper_writer_end(void);
void 		updhelper_writer_sigstop(void);

#endif /* UPDPROC_INCLUDED */
