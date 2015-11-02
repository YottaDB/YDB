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

#define RC_CPT_SPGINV	0x00
#define RC_CPT_SRVLST	0x01
#define RC_CPT_OFLOW	0x02
#define RC_CPT_INVAL	0x03
#define RC_CPT_AGNTQ	0x04
#define RC_CPT_NAMINV	0x0D
#define RC_CPT_LKINV	0x0E
#define RC_CPT_ALLINV	0x0F

#define RC_MAX_CPT_SYNC	65535
#define RC_CPT_OVERFLOW	0xFFFF
#define RC_CPT_ENTRY_SIZE	SIZEOF(int4)
#define RC_CPT_TABSIZE 512

typedef struct {
int4		ring_buff[RC_CPT_TABSIZE];
unsigned short	cpsync;		/* entry count since server start */
unsigned short	cpvfy;		/* number of entries since last delivered to a client */
short		index;		/* next entry to fill */
short		server_count;	/* number of servers using CPT */
}rc_cp_table;

int	rc_cpt_sem;	/* semaphore for CPTable */

#define RC_CPT_PATH "$gtm_dist/gtcm_server"
