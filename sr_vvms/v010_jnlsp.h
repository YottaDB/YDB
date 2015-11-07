/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Start jnlsp.h - platform-specific journaling definitions.  */

#ifndef V010_JNLSP_H_INCLUDED
#define V010_JNLSP_H_INCLUDED

#ifndef SS$_NORMAL
#include <ssdef.h>
#endif


#ifdef __ALPHA
# pragma member_alignment save
# pragma nomember_alignment
#endif

typedef	struct
{
	short		low_time;
	uint4	mid_time;
	short		hi_time;
} jnl_proc_time;

#ifdef __ALPHA
# pragma member_alignment restore
#endif

/* in disk blocks but jnl file addresses are kept by byte so limited by uint4 for now */
#define JNL_ALLOC_MAX		 8388608

#define JPV_LEN_NODE		15
#define JPV_LEN_USER		12
#define JPV_LEN_PRCNAM		15
#define JPV_LEN_TERMINAL	8

typedef struct jnl_process_vector_struct
{
	uint4	jpv_pid;			/* Process id */
	jnl_proc_time	jpv_time,			/* Journal record timestamp;  also used for process termination time */
			jpv_login_time;			/* Process login time;  also used for process initialization time */
	int4		jpv_image_count;		/* Image activations [VMS only] */
	unsigned char	jpv_mode;			/* a la JPI$_MODE [VMS only] */
	char		jpv_node[JPV_LEN_NODE],		/* Node name */
			jpv_user[JPV_LEN_USER],		/* User name */
			jpv_prcnam[JPV_LEN_PRCNAM],	/* Process name */
			jpv_terminal[JPV_LEN_TERMINAL];	/* Login terminal */
	/* SIZEOF(jnl_process_vector) must be a multiple of SIZEOF(int4) */
	char		jpv_padding;
} jnl_process_vector;

#define V010_JNL_PROCESS_VECTOR_SIZE	76

typedef	short			fd_type;
typedef vms_file_info		fi_type;

#define NOJNL			0
#define LENGTH_OF_TIME		23
#define SOME_TIME(X)		(X.mid_time != 0)
#define JNL_S_TIME(Y,X)		Y->val.X.process_vector.jpv_time.mid_time
#define JNL_M_TIME(X)		mur_options.X.mid_time
#define EXTTIME(T)		extract_len = exttime(T->mid_time, ref_time, extract_len)
#define EXTTIMEVMS(T)		extract_len = exttime(T.mid_time, &T, extract_len)
#define	EXTINTVMS(I)		EXTINT(I)
#define	EXTTXTVMS(T,L)		EXTTXT(T,L)

#define JNL_FILE_SWITCHED(reg)	(memcmp((&FILE_INFO(reg)->s_addrs)->hdr->jnl_file.jnl_file_id.fid, (&FILE_INFO(reg)->s_addrs)->jnl->fileid.fid, SIZEOF((&FILE_INFO(reg)->s_addrs)->jnl->fileid.fid)) != 0)

/* End of jnlsp.h */

#endif
