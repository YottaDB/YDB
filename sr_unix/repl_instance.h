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

#ifndef REPL_INSTANCE_INCLUDED
#define REPL_INSTANCE_INCLUDED

#define GDS_REPL_INST_LABEL 		"GDSRPLUNX02" /* For now this is same as GDS_RPL_LABEL */
#define GDS_REPL_INST_LABEL_SZ 		12
/*
 * Format of the file pointed by gtm_repl_instance
 */
typedef struct repl_inst_fmt_struct {
	unsigned char   label[GDS_REPL_INST_LABEL_SZ];
	int 		jnlpool_semid;
	int 		jnlpool_shmid;
	int 		recvpool_semid;
	int 		recvpool_shmid;
	time_t		jnlpool_semid_ctime;
	time_t 		jnlpool_shmid_ctime;
	time_t 		recvpool_semid_ctime;
	time_t 		recvpool_shmid_ctime;
}  repl_inst_fmt;

#define DEF_INST_FN "mumps.repl"

boolean_t repl_inst_get_name(char *, unsigned int *, unsigned int);
void repl_inst_create(void);
void repl_inst_get(char *, repl_inst_fmt *);
void repl_inst_put(char *, repl_inst_fmt *);
void repl_inst_jnlpool_reset(void);
void repl_inst_recvpool_reset(void);

#endif /* REPL_INSTANCE_INCLUDED */
