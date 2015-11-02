/****************************************************************
 *								*
 *	Copyright 2006, 2007 Fidelity Information Services, Inc	*
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
	REPL_INVALID = -1,		/* -1 */
	REPL_START_JNL_SEQNO = 0,	/* 0 */
	REPL_TR_JNL_RECS,		/* 1 */
	REPL_ROLLBACK_FIRST,		/* 2 */
	REPL_WILL_RESTART_OBSOLETE,	/* 3 */ /* Obsoleted effective V4.4-002 since we no longer support dual site config with
					     	 * V4.1 versions. But, DO NOT remove this message type to keep other message types
					     	 * same as in V4.2 and V4.3 versions */
	REPL_XOFF,			/* 4 */
	REPL_XON,			/* 5 */
	REPL_BADTRANS,			/* 6 */
	REPL_HEARTBEAT,			/* 7 */
	REPL_FETCH_RESYNC,		/* 8 */
	REPL_RESYNC_SEQNO,		/* 9 */
	REPL_START_SEQNO_STOPSRCFILTER_OBSOLETE, /* 10 */ /* needed for backward compatibility */
	REPL_XOFF_ACK_ME,		/* 11 */
	REPL_XOFF_ACK,			/* 12 */
	REPL_WILL_RESTART_WITH_INFO,	/* 13 */
	REPL_MULTISITE_MSG_START,	/* 14 */ /* All messages after this are newly introduced for multi-site support only */
	REPL_NEED_INSTANCE_INFO,	/* 15 */
	REPL_INSTANCE_INFO,		/* 16 */
	REPL_NEED_TRIPLE_INFO,		/* 17 */
	REPL_TRIPLE_INFO1,		/* 18 */
	REPL_TRIPLE_INFO2,		/* 19 */
	REPL_NEW_TRIPLE,		/* 20 */
	REPL_INST_NOHIST,		/* 21 */
	REPL_LOSTTNCOMPLETE,		/* 22 */
	REPL_MSGTYPE_LAST=256		/* 256 */
	/* any new message need to be added before REPL_MSGTYPE_LAST */
};

#define	REPL_PROTO_VER_UNINITIALIZED	(char)0xFF	/* -1, the least of the versions to denote an uninitialized version field */
#define	REPL_PROTO_VER_DUALSITE		(char)0x0
#define	REPL_PROTO_VER_MULTISITE	(char)0x1	/* Currently the same as REPL_PROTO_VER_THIS. But the latter can increase
							 * with a new version while this will never change.
							 */
#define	REPL_PROTO_VER_THIS		(char)0x1	/* The current version of the communication protocol between the
							 * primary (source server) and secondary (receiver server or rollback)
							 * Versions GT.M V5.0 and prior that dont support multi site replication
							 * functionality have a protocol version of 0.
							 */
#define START_FLAG_NONE				0x00000000
#define START_FLAG_STOPSRCFILTER		0x00000001
#define START_FLAG_UPDATERESYNC			0x00000002
#define START_FLAG_HASINFO			0x00000004
#define START_FLAG_COLL_M			0x00000008
#define	START_FLAG_VERSION_INFO			0x00000010

#define	MIN_REPL_MSGLEN		32 /* To keep compiler happy with
				    * the definition of repl_msg_t as well
				    * as to accommodate a seq_num */

#define REPL_MSG_HDRLEN	(uint4)(sizeof(int4) + sizeof(int4)) /* For type and
						     * len fields */

typedef struct	/* Used also to send a message of type REPL_INST_NOHIST */
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
	unsigned char	start_seqno[sizeof(seq_num)];
	uint4		start_flags;
	unsigned char	jnl_ver;
	char		proto_ver;	/* Needs to be "signed char" in order to be able to do signed comparisons of this with
					 * the macros REPL_PROTO_VER_DUALSITE (0) and REPL_PROTO_VER_UNINITIALIZED (-1) */
	char		node_endianness;	/* 'L' if this machine is little endian, 'B' if it is big endian */
	char		filler_32[9];
} repl_start_msg_t; /* The first two fields should be as in repl_msg_t */

typedef struct
{
	int4		type;
	int4		len;
	unsigned char	start_seqno[sizeof(seq_num)];
	unsigned char	jnl_ver;
	char		start_flags[4];
	char		proto_ver;	/* Needs to be "signed char" in order to be able to do signed comparisons of this with
					 * the macros REPL_PROTO_VER_DUALSITE (0) and REPL_PROTO_VER_UNINITIALIZED (-1) */
	char		node_endianness;	/* 'L' if this machine is little endian, 'B' if it is big endian */
	char		filler_32[9];
} repl_start_reply_msg_t; /* The first two fields should be as in repl_msg_t */

typedef struct	/* used to send a message of type REPL_FETCH_RESYNC */
{
	int4		type;
	int4		len;
	seq_num		resync_seqno;
	char		proto_ver;	/* Needs to be "signed char" in order to be able to do signed comparisons of this with
					 * the macros REPL_PROTO_VER_DUALSITE (0) and REPL_PROTO_VER_UNINITIALIZED (-1) */
	char		node_endianness;	/* 'L' if this machine is little endian, 'B' if it is big endian */
	char		filler_32[14];
} repl_resync_msg_t; /* The first two fields should be as in repl_msg_t */

typedef struct	/* used to send a message of type REPL_NEED_INSTANCE_INFO */
{
	int4		type;
	int4		len;
	unsigned char	instname[MAX_INSTNAME_LEN];
	char		proto_ver;	/* Needs to be "signed char" in order to be able to do signed comparisons of this with
					 * the macros REPL_PROTO_VER_DUALSITE (0) and REPL_PROTO_VER_UNINITIALIZED (-1) */
	char		node_endianness;	/* 'L' if this machine is little endian, 'B' if it is big endian */
	char		is_rootprimary;	/* Whether the source server that is sending this message is a root primary or not. */
	char		filler_32[5];
} repl_needinst_msg_t; /* The first two fields should be as in repl_msg_t */

typedef struct	/* used to send a message of type REPL_NEED_TRIPLE_INFO */
{
	int4		type;
	int4		len;
	seq_num		seqno;	/* The triple requested is that of "seqno-1" */
	char		filler_32[16];
} repl_needtriple_msg_t; /* The first two fields should be as in repl_msg_t */

typedef struct	/* used to send a message of type REPL_INSTANCE_INFO */
{
	int4		type;
	int4		len;
	unsigned char	instname[MAX_INSTNAME_LEN];	/* The name of this replication instance */
	unsigned char	was_rootprimary;
	char		filler_32[7];
} repl_instinfo_msg_t; /* The first two fields should be as in repl_msg_t */

typedef struct	/* used to send a message of type REPL_TRIPLE_INFO1 */
{
	int4		type;
	int4		len;
	seq_num		start_seqno;	/* the starting seqno of the triple range */
	unsigned char	instname[MAX_INSTNAME_LEN];
} repl_tripinfo1_msg_t; /* The first two fields should be as in repl_msg_t */

typedef struct	/* used to send a message of type REPL_TRIPLE_INFO2 */
{
	int4		type;
	int4		len;
	seq_num		start_seqno;	/* the starting seqno of the triple range */
	uint4		cycle;
	uint4		triple_num;
	char		filler_32[8];
} repl_tripinfo2_msg_t; /* The first two fields should be as in repl_msg_t */

/* A REPL_NEW_TRIPLE message is sent in such a form that the receiver server can copy it (after removing the 8-byte header
 * containing the "type" and "len") as such into the receive pool. The update process treats this content as yet another
 * journal record type. Therefore we need to introduce a new journal record type (JRT_TRIPLE) for this purpose and define a
 * structure (repl_triple_jnl_t) that has this type information and the triple related information. This will then be a
 * member of the REPL_NEW_TRIPLE message.
 */
typedef struct	/* sub-structure used to send a message of type REPL_NEW_TRIPLE */
{
	uint4		jrec_type : 8;	/* This definition is copied from that of "jrec_prefix" */
	uint4		forwptr : 24;	/* This definition is copied from that of "jrec_prefix" */
	uint4		cycle;		/* The cycle of the instance at the time of generating this triple */
	seq_num		start_seqno;	/* The starting seqno of the triple range */
	unsigned char	instname[MAX_INSTNAME_LEN];	/* The instance name that generated this triple */
	unsigned char	rcvd_from_instname[MAX_INSTNAME_LEN];	/* instance name this triple was received from (on the secondary) */
} repl_triple_jnl_t;

typedef struct	/* used to send a message of type REPL_NEW_TRIPLE */
{
	int4			type;
	int4			len;
	repl_triple_jnl_t	triplecontent;
} repl_triple_msg_t; /* The first two fields should be as in repl_msg_t */

typedef struct
{
	int4		type;
	int4		len;
	unsigned char	ack_seqno[sizeof(seq_num)];
	unsigned char	ack_time[sizeof(gtm_time4_t)];
	unsigned char	filler_32[12];
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
typedef repl_resync_msg_t	*repl_resync_msg_ptr_t;
typedef repl_needinst_msg_t	*repl_needinst_msg_ptr_t;
typedef repl_needtriple_msg_t	*repl_needtriple_msg_ptr_t;
typedef repl_instinfo_msg_t	*repl_instinfo_msg_ptr_t;
typedef repl_tripinfo1_msg_t	*repl_tripinfo1_msg_ptr_t;
typedef repl_tripinfo2_msg_t	*repl_tripinfo2_msg_ptr_t;
typedef repl_triple_jnl_t	*repl_triple_jnl_ptr_t;
typedef repl_triple_msg_t	*repl_triple_msg_ptr_t;
typedef repl_heartbeat_msg_t	*repl_heartbeat_msg_ptr_t;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(restore)
#endif

void	gtmsource_repl_send(repl_msg_ptr_t msg, char *msgtypestr, seq_num optional_seqno);
void	gtmsource_send_new_triple(void);

void	gtmrecv_repl_send(repl_msg_ptr_t msgp, char *msgtypestr, seq_num optional_seqno);
void	gtmrecv_send_triple_info(repl_triple *triple, int4 triple_num);

#endif
