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

#ifndef JPV_V10TOV12
#define JPV_V10TOV12

#define V10_JPV_LEN_NODE            15
#define V10_JPV_LEN_USER            12
#define V10_JPV_LEN_PRCNAM          15
#define V10_JPV_LEN_TERMINAL        8

typedef struct v10_jnl_process_vector_struct
{
        uint4   	jpv_pid;                        	/* Process id */
        jnl_proc_time   jpv_time,                       /* Journal record timestamp;  also used for process termination time */
                        jpv_login_time;                 /* Process login time;  also used for process initialization time */
        int4            jpv_image_count;                /* Image activations [VMS only] */
        unsigned char   jpv_mode;                       /* a la JPI$_MODE [VMS only] */
        char            jpv_node[V10_JPV_LEN_NODE],         /* Node name */
                        jpv_user[V10_JPV_LEN_USER],         /* User name */
                        jpv_prcnam[V10_JPV_LEN_PRCNAM],     /* Process name */
                        jpv_terminal[V10_JPV_LEN_TERMINAL]; /* Login terminal */
        /* SIZEOF(jnl_process_vector) must be a multiple of SIZEOF(int4) */
        char            jpv_padding;
} v10_jnl_process_vector;

void jpv_v10to12(char *old_jpv_ptr, jnl_process_vector *new_jpv);

#endif /* JPV_V10TOV12 */
