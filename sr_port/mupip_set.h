/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

int4	mupip_set_file(int db_fn_len, char *db_fn);
int4	mupip_set_jnlfile(char *jnl_fname, int jnl_fn_len);
void	mupip_set_jnl_cleanup(void);
uint4	mupip_set_journal(unsigned short db_fn_len, char *db_fn);
void	mupip_set(void);
