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
	REPL_CMP_TEST,			/* 23 */
	REPL_CMP_SOLVE,			/* 24 */
	REPL_CMP2UNCMP,			/* 25 */  /* used to signal a transition to uncompressed messages in case of errors */
	REPL_TR_CMP_JNL_RECS,		/* 26 */  /* used to send compressed messages whose pre-compressed length is <  2**24 */
	REPL_TR_CMP_JNL_RECS2,		/* 27 */  /* used to send compressed messages whose pre-compressed length is >= 2**24 */
	REPL_MSGTYPE_LAST=256		/* 256 */
	/* any new message need to be added before REPL_MSGTYPE_LAST */
};

#define	REPL_PROTO_VER_UNINITIALIZED	(char)0xFF	/* -1, the least of the versions to denote an uninitialized version field */
#define	REPL_PROTO_VER_DUALSITE		(char)0x0	/* Versions GT.M V5.0 and prior that dont support multi site replication */
#define	REPL_PROTO_VER_MULTISITE	(char)0x1	/* Versions V5.1-000 and above that support multi site replication */
#define	REPL_PROTO_VER_MULTISITE_CMP	(char)0x2	/* Versions V5.3-003 and above that suport multisite replication with
							 * the ability to compress the logical records in the replication pipe.
							 */
#define	REPL_PROTO_VER_THIS		REPL_PROTO_VER_MULTISITE_CMP
							/* The current/latest version of the communication protocol between the
							 * primary (source server) and secondary (receiver server or rollback)
							 */

#define	START_FLAG_NONE				0x00000000
#define	START_FLAG_STOPSRCFILTER		0x00000001
#define	START_FLAG_UPDATERESYNC			0x00000002
#define	START_FLAG_HASINFO			0x00000004
#define	START_FLAG_COLL_M			0x00000008
#define	START_FLAG_VERSION_INFO			0x00000010
#define	START_FLAG_TRIGGER_SUPPORT		0x00000020
#define	START_FLAG_SRCSRV_IS_VMS		0x00000040

#define	MIN_REPL_MSGLEN		32 /* To keep compiler happy with
				    * the definition of repl_msg_t as well
				    * as to accommodate a seq_num */

#define	REPL_MSG_CMPINFOLEN	(SIZEOF(repl_cmpinfo_msg_t))
#define	REPL_MSG_CMPDATALEN	256	/* length of data part of message exchanged between source/receiver to test compression */
#define	REPL_MSG_CMPDATAMASK	0xff
#define	MAX_CMP_EXPAND_FACTOR	2	/* the worst case factor by which compression could actually expand input data */
#define	REPL_MSG_CMPEXPDATALEN	(MAX_CMP_EXPAND_FACTOR * REPL_MSG_CMPDATALEN)

/* The "datalen" field of the REPL_CMP_SOLVE message is set to the following value by the receiver server in case of errors. */
#define	REPL_RCVR_CMP_TEST_FAIL		(-1)

#define	REPL_MSG_ALIGN	8	/* every message sent across the pipe is expected to be at least 8-byte aligned */

#define	REPL_MSG_TYPE	(uint4)(SIZEOF(int4))
#define	REPL_MSG_LEN	(uint4)(SIZEOF(int4))
#define	REPL_MSG_HDRLEN	(uint4)(REPL_MSG_TYPE + REPL_MSG_LEN) /* For type and len fields */

typedef struct	/* Used also to send a message of type REPL_INST_NOHIST */
{
	int4		type;
	int4		len;	/* Is 8-byte aligned for ALL messages except REPL_TR_CMP_JNL_RECS message */
	unsigned char	msg[MIN_REPL_MSGLEN - REPL_MSG_HDRLEN];
	/* All that we need is msg[1], but keep the  compiler happy with
	 * this definition for msg. Also provide space to accommodate a seq_num
	 * so that a static definition would suffice instead of malloc'ing a
	 * small message buffer */
} repl_msg_t;

/* To send a REPL_TR_CMP_JNL_RECS type of message, we include the pre-compressed length as part of the 4-byte type field
 * to keep the replication message header overhead to the current 8-bytes. Define macros to access those fields.
 * If the pre-compressed length cannot fit in 24 bits, then we send a REPL_TR_CMP_JNL_RECS2 message which has 8-more
 * byte overhead as part of the replication message header. It is considered ok to take this extra 8-byte hit for huge messages.
 * The threshold is defined as 2**23 (instead of 2**24) so we ensure the most significant bit is 0 and does not give us
 * problems while doing >> operations (e.g. (((repl_msg_ptr_t)buffp)->type >> REPL_TR_CMP_MSG_TYPE_BITS);
 */
#define	REPL_TR_CMP_MSG_TYPE_BITS	8
#define	REPL_TR_CMP_MSG_UNCMPLEN_BITS	24
#define	REPL_TR_CMP_THRESHOLD		(1 << (REPL_TR_CMP_MSG_UNCMPLEN_BITS - 1))
#define	REPL_TR_CMP_MSG_TYPE_MASK	((1 << REPL_TR_CMP_MSG_TYPE_BITS) - 1)

/* Defines for sending REPL_TR_CMP_JNL_RECS2 message */
#define	REPL_MSG_UNCMPLEN	SIZEOF(int4)
#define	REPL_MSG_CMPLEN		SIZEOF(int4)
#define	REPL_MSG_HDRLEN2	(REPL_MSG_HDRLEN + REPL_MSG_UNCMPLEN + REPL_MSG_CMPLEN)	/* 16-byte header */
typedef struct	/* Used also to send a message of type REPL_TR_CMP_JNL_RECS2 */
{
	int4		type;
	int4		len;
	int4		uncmplen;
	int4		cmplen;
	unsigned char	msg[MIN_REPL_MSGLEN - REPL_MSG_HDRLEN2];
	/* All that we need is msg[1], but keep the  compiler happy with
	 * this definition for msg. Also provide space to accommodate a seq_num
	 * so that a static definition would suffice instead of malloc'ing a
	 * small message buffer */
} repl_cmpmsg_t;

#define	MAX_REPL_MSGLEN	(1 * 1024 * 1024) /* should ideally match the TCP send (recv) bufsiz of source (receiver) server */
#define MAX_TR_BUFFSIZE	(MAX_REPL_MSGLEN - REPL_MSG_HDRLEN2) /* allow for biggest replication message header */

typedef struct
{
	int4		type;
	int4		len;
	unsigned char	start_seqno[SIZEOF(seq_num)];
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
	unsigned char	start_seqno[SIZEOF(seq_num)];
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

typedef struct	/* used to send messages of type REPL_CMP_TEST or REPL_CMP_SOLVE */
{
	int4		type;
	int4		len;
	int4		datalen;		   /* length of compressed or uncompressed data */
	char		proto_ver;
	char		filler_16[3];
	char		data[REPL_MSG_CMPDATALEN]; /* compressed (if REPL_CMP_TEST) or uncompressed (if REPL_CMP_SOLVE) data */
	char		overflowdata[(MAX_CMP_EXPAND_FACTOR - 1) * REPL_MSG_CMPDATALEN];
					/* buffer to hold overflow in case compression expands data */
} repl_cmpinfo_msg_t; /* The first two fields should be as in repl_msg_t */

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
	unsigned char	ack_seqno[SIZEOF(seq_num)];
	unsigned char	ack_time[SIZEOF(gtm_time4_t)];
	unsigned char	filler_32[12];
} repl_heartbeat_msg_t; /* The first two fields should be as in repl_msg_t */

typedef struct
{
	struct
	{
		sm_off_t	fl;
		sm_off_t	bl;
	} que;
	repl_heartbeat_msg_t	heartbeat;
} repl_heartbeat_que_entry_t;

typedef struct		/* Used to send a message of type REPL_BADTRANS or REPL_CMP2UNCMP */
{
	int4		type;
	int4		len;
	seq_num		start_seqno;	/* The seqno that source server should restart sending from */
	char		filler_32[16];
} repl_badtrans_msg_t;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(save)
# pragma pointer_size(long)
#endif

typedef repl_msg_t		*repl_msg_ptr_t;
typedef repl_cmpmsg_t		*repl_cmpmsg_ptr_t;
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
typedef repl_cmpinfo_msg_t	*repl_cmpinfo_msg_ptr_t;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(restore)
#endif

void	gtmsource_repl_send(repl_msg_ptr_t msg, char *msgtypestr, seq_num optional_seqno);
void	gtmsource_send_new_triple(boolean_t rcvr_same_endianness);

void	gtmrecv_repl_send(repl_msg_ptr_t msgp, int4 type, int4 len, char *msgtypestr, seq_num optional_seqno);
void	gtmrecv_send_triple_info(repl_triple *triple, int4 triple_num);

#endif
