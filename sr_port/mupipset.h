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

enum rlist_state {
	NONALLOCATED,
	ALLOCATED,
	DEALLOCATED
};

/* ATTN: the first four items in this structure need to be identical to those
 *	 in structure tp_region in tp.h.
 */

typedef struct mu_set_reglist
{
	struct mu_set_reglist	*fPtr;		/* all fields after this are used for mupip_set_journal.c */
	gd_region		*reg;
	char			unique_id[UNIQUE_ID_SIZE];
	enum rlist_state	state;
	sgmnt_data_ptr_t 	sd;
	bool			exclusive;	/* standalone access is required for this region */
	int			fd;
} mu_set_rlist;

int4 mupip_set_file(int db_fn_len, char *db_fn);
int4 mupip_set_jnl_file(char *jnl_fname);
void  mupip_set_jnl_cleanup(void);
uint4 mupip_set_journal(bool journal, bool replication, unsigned short db_fn_len, char *db_fn);
void mupip_set(void);
