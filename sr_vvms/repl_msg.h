/****************************************************************
 *								*
 *	Copyright 2006, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef _REPL_MSG_H
#define _REPL_MSG_H

enum
{
	REPL_START_JNL_SEQNO = 0,
	REPL_TR_JNL_RECS, /* 1 */
	REPL_ROLLBACK_FIRST, /* 2 */
	REPL_WILL_RESTART_OBSOLETE, /* 3 */ /* Obsoleted effective V4.4-002 since we no longer support dual site config with
					     * V4.1 versions. But, DO NOT remove this message type to keep other message types
					     * same as in V4.2 and V4.3 versions */
	REPL_XOFF, /* 4 */
	REPL_XON, /* 5 */
	REPL_BADTRANS, /* 6 */
	REPL_HEARTBEAT, /* 7 */
	REPL_FETCH_RESYNC, /* 8 */
	REPL_RESYNC_SEQNO, /* 9 */
	REPL_START_SEQNO_STOPSRCFILTER, /* 10 */ /* needed for backward compatibility with 4.1-000 */
	REPL_XOFF_ACK_ME, /* 11 */
	REPL_XOFF_ACK, /* 12 */
	REPL_WILL_RESTART_WITH_INFO /* 13 */
};

#define START_FLAG_NONE				0x00000000
#define START_FLAG_STOPSRCFILTER		0x00000001
#define START_FLAG_UPDATERESYNC			0x00000002
#define START_FLAG_HASINFO			0x00000004
#define START_FLAG_COLL_M			0x00000008
#define	START_FLAG_VERSION_INFO			0x00000010
#define	START_FLAG_TRIGGER_SUPPORT		0x00000020
#define	START_FLAG_SRCSRV_IS_VMS		0x00000040

#define	MIN_REPL_MSGLEN		32 /* To keep compiler happy with
				    * the definition of repl_msg_t as well
				    * as to accommodate a seq_num */

#define REPL_MSG_HDRLEN	(SIZEOF(int4) + SIZEOF(int4)) /* For type and
						     * len fields */

typedef struct
{
	int4		type;
	int4		len;
	unsigned char 	msg[MIN_REPL_MSGLEN - REPL_MSG_HDRLEN];
	/* All that we need is msg[1], but keep the  compiler happy with
	 * this definition for msg. Also provide space to accommodate a seq_num
	 * so that a static definition would suffice instead of malloc'ing a
	 * small message buffer */
} repl_msg_t;

#define	MAX_REPL_MSGLEN	(1 * 1024 * 1024) /* should ideally match the TCP send (recv) bufsiz of source (receiver) server */
#define MAX_TR_BUFFSIZE	(MAX_REPL_MSGLEN - REPL_MSG_HDRLEN) /* allow for replication message header */

typedef struct
{
	int4		type;
	int4		len;
	unsigned char	start_seqno[SIZEOF(seq_num)];
	uint4		start_flags;
	unsigned char	jnl_ver;
	char		filler[MIN_REPL_MSGLEN - REPL_MSG_HDRLEN - SIZEOF(seq_num) -
			       SIZEOF(uint4) - SIZEOF(unsigned char)];
} repl_start_msg_t; /* The first two fields should be as in repl_msg_t */

typedef struct
{
	int4		type;
	int4		len;
	unsigned char	start_seqno[SIZEOF(seq_num)];
	unsigned char	jnl_ver;
	char		start_flags[4];
	char		filler[MIN_REPL_MSGLEN - REPL_MSG_HDRLEN - SIZEOF(seq_num) -
			       - SIZEOF(unsigned char) - 4 * SIZEOF(char)];
} repl_start_reply_msg_t; /* The first two fields should be as in repl_msg_t */

typedef struct
{
	int4		type;
	int4		len;
	unsigned char	ack_seqno[SIZEOF(seq_num)];
	unsigned char	ack_time[SIZEOF(time_t)];
	unsigned char	filler[MIN_REPL_MSGLEN - REPL_MSG_HDRLEN - SIZEOF(seq_num) - SIZEOF(time_t)];
} repl_heartbeat_msg_t; /* The first two fields should be as in repl_msg_t */

typedef struct
{
	struct
	{
		int4		fl;
		int4		bl;
	} que;
	repl_heartbeat_msg_t	heartbeat;
} repl_heartbeat_que_entry_t;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(save)
# pragma pointer_size(long)
#endif

typedef repl_msg_t 		*repl_msg_ptr_t;
typedef repl_start_msg_t	*repl_start_msg_ptr_t;
typedef repl_start_reply_msg_t	*repl_start_reply_msg_ptr_t;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(restore)
#endif

#endif
