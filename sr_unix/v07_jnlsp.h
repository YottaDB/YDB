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

/* Start jnlsp.h - platform-specific journaling definitions.  */

#ifndef sys_nerr
#include <errno.h>
#endif


typedef int4			jnl_proc_time;

/* in disk blocks but jnl file addresses are kept by byte so limited by uint4 for now */
#ifndef OFF_T_LONG
#define JNL_ALLOC_MAX		4194304  /* 2GB */
#else
#define JNL_ALLOC_MAX		8388608  /* 4GB */
#endif

#define JPV_LEN_PRCNAM		15
#define JPV_LEN_NODE		15
#define JPV_LEN_TERMINAL	15
#define JPV_LEN_USER		15

typedef struct
{
	uint4	jpv_pid;			/* Process id */
	jnl_proc_time	jpv_time,			/* Journal record timestamp;  also used for process termination time */
			jpv_login_time;			/* Used for process initialization time */
	char		jpv_node[JPV_LEN_NODE],		/* Node name */
			jpv_user[JPV_LEN_USER],		/* User name */
			jpv_prcnam[JPV_LEN_PRCNAM],	/* Process name */
			jpv_terminal[JPV_LEN_TERMINAL];	/* Login terminal */
	/* sizeof(jnl_process_vector) must be a multiple of sizeof(int4) */
} jnl_process_vector;


typedef	int			fd_type;
typedef unix_file_info		fi_type;

#define NOJNL			-1
#define LENGTH_OF_TIME		11
#define SOME_TIME(X)		(X != 0)
#define JNL_S_TIME(Y,X)		Y->val.X.process_vector.jpv_time
#define JNL_M_TIME(X)		mur_options.X
#define EXTTIME(T)		extract_len = exttime(*T, ref_time, extract_len)
#define EXTTIMEVMS(T)
#define EXTINTVMS(I)
#define EXTTXTVMS(T,L)

#define JNL_FILE_SWITCHED(reg) (!is_gdid_gdid_identical(&(&FILE_INFO(reg)->s_addrs)->hdr->jnl_file.u, &(&FILE_INFO(reg)->s_addrs)->jnl->fileid))

/* End of jnlsp.h */
