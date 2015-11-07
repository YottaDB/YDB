/****************************************************************
 *								*
 *	Copyright 2006, 2013 Fidelity Information Services, Inc	*
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
	REPL_OBSOLETE_WILL_RESTART,	/* 3 */ /* Obsoleted effective V4.4-002 since we no longer support dual site config with
						 * V4.1 versions. But slot# preserved to not perturb other message #s */
	REPL_XOFF,			/* 4 */
	REPL_XON,			/* 5 */
	REPL_BADTRANS,			/* 6 */
	REPL_HEARTBEAT,			/* 7 */
	REPL_FETCH_RESYNC,		/* 8 */
	REPL_RESYNC_SEQNO,		/* 9 */
	REPL_OBSOLETE_STOPSRCFILTER,	/* 10 */ /* message no longer used but slot# 10 preserved to not perturb other message #s */
	REPL_XOFF_ACK_ME,		/* 11 */
	REPL_XOFF_ACK,			/* 12 */
	REPL_WILL_RESTART_WITH_INFO,	/* 13 */
	REPL_MULTISITE_MSG_START,	/* 14 */ /* All messages after this are newly introduced for multi-site support only */
	REPL_OLD_NEED_INSTANCE_INFO,	/* 15 */
	REPL_OLD_INSTANCE_INFO,		/* 16 */
	REPL_NEED_HISTINFO,		/* 17 */
	REPL_OLD_TRIPLEINFO1,		/* 18 */
	REPL_OLD_TRIPLEINFO2,		/* 19 */
	REPL_OLD_TRIPLE,		/* 20 */
	REPL_INST_NOHIST,		/* 21 */
	REPL_LOSTTNCOMPLETE,		/* 22 */
	REPL_CMP_TEST,			/* 23 */
	REPL_CMP_SOLVE,			/* 24 */
	REPL_CMP2UNCMP,			/* 25 */  /* used to signal a transition to uncompressed messages in case of errors */
	REPL_TR_CMP_JNL_RECS,		/* 26 */  /* used to send compressed messages whose pre-compressed length is <  2**24 */
	REPL_TR_CMP_JNL_RECS2,		/* 27 */  /* used to send compressed messages whose pre-compressed length is >= 2**24 */
	REPL_NEED_INSTINFO,		/* 28 */
	REPL_INSTINFO,			/* 29 */
	REPL_HISTINFO,			/* 30 */  /* used to exchange history information as part of a replication handshake */
	REPL_HISTREC,			/* 31 */  /* sent in the middle of a journal record stream to signal start of new history */
	REPL_NEED_STRMINFO,		/* 32 */  /* sent by a supplementary source server to a supplementary receiver server */
	REPL_STRMINFO,			/* 33 */  /* sent in response to a REPL_NEED_STRMINFO message */
	REPL_LOGFILE_INFO,		/* 34 */  /* sent (at time of handshake) to communicate to one another the $CWD/logfile */
	REPL_MSGTYPE_LAST=256		/* 256 */
	/* any new message need to be added before REPL_MSGTYPE_LAST */
};

#define	REPL_PROTO_VER_UNINITIALIZED	(char)0xFF	/* -1, the least of the versions to denote an uninitialized version field */
#define	REPL_PROTO_VER_DUALSITE		(char)0x0	/* Versions GT.M V5.0 and prior that dont support multi site replication */
#define	REPL_PROTO_VER_MULTISITE	(char)0x1	/* Versions V5.1-000 and above that support multi site replication */
#define	REPL_PROTO_VER_MULTISITE_CMP	(char)0x2	/* Versions V5.3-003 and above that support multisite replication with
							 * the ability to compress the logical records in the replication pipe.
							 */
#define	REPL_PROTO_VER_SUPPLEMENTARY	(char)0x3	/* Versions V5.5-000 and above that support supplementary instances */
#define	REPL_PROTO_VER_REMOTE_LOGPATH	(char)0x4	/* Versions V6.0-003 and above that send remote $CWD as part of handshake */
#define	REPL_PROTO_VER_THIS		REPL_PROTO_VER_REMOTE_LOGPATH
							/* The current/latest version of the communication protocol between the
							 * primary (source server) and secondary (receiver server or rollback)
							 */
/* Below macro defines the maximum size of the replication logfile path information that will be sent across to the other side.
 * While the actual path max is defined by GTM_PATH_MAX, the value of GTM_PATH_MAX differ across different platforms and so we
 * cannot always be sure if this side can store the remote side's logfile path. Since REPL_LOGFILE_INFO message is sent across
 * only during handshake, malloc too is an overkill. So, have a fixed size buffer (see repl_logfile_info_msg_t beow) bounded by
 * the below size.
 */
#define REPL_LOGFILE_PATH_MAX		1023

/* A few of these flag bits (e.g. START_FLAG_SRCSRV_IS_VMS) are no longer used but they should not be removed just in case prior
 * versions that used those bitslots communicate with a newer version that assigns a different meaning to those slots. Any new
 * slot additions need to happen at the end of the list. No replacements of unused slots.
 */
#define	START_FLAG_NONE				0x00000000
#define	START_FLAG_STOPSRCFILTER		0x00000001
#define	START_FLAG_UPDATERESYNC			0x00000002
#define	START_FLAG_HASINFO			0x00000004
#define	START_FLAG_COLL_M			0x00000008
#define	START_FLAG_VERSION_INFO			0x00000010
#define	START_FLAG_TRIGGER_SUPPORT		0x00000020
#define	START_FLAG_SRCSRV_IS_VMS		0x00000040	/* Obsolete but preserve slot */
#define	START_FLAG_NORESYNC			0x00000080

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

/* This should ideally match the TCP send/recv buffer size of source/receiver server. However, since the maximum TCP buffer
 * varies by system, and on many systems it is less than 2 MB or even 1 MB, we keep GTMRECV_TCP_RECV_BUFSIZE and
 * GTMSOURCE_TCP_SEND_BUFSIZE at 1 MB to avoid extra attempts to send/receive TCP packets before the limit is lowered enough
 * for the OS to support it.
 */
#define	MAX_REPL_MSGLEN	(2 * 1024 * 1024)
#define MAX_TR_BUFFSIZE	(MAX_REPL_MSGLEN - REPL_MSG_HDRLEN2) /* allow for biggest replication message header */

typedef struct	/* used to send a message of type REPL_START_JNL_SEQNO */
{
	int4		type;
	int4		len;
	unsigned char	start_seqno[SIZEOF(seq_num)];
	uint4		start_flags;
	unsigned char	jnl_ver;
	char		proto_ver;	/* Needs to be "signed char" in order to be able to do signed comparisons of this with
					 * the macros REPL_PROTO_VER_DUALSITE (0) and REPL_PROTO_VER_UNINITIALIZED (-1) */
	char		node_endianness;	/* 'L' if this machine is little endian, 'B' if it is big endian */
	char		is_supplementary;	/* TRUE for a supplementary instance; FALSE otherwise */
	char		filler_32[8];
} repl_start_msg_t; /* The first two fields should be as in repl_msg_t */

typedef struct	/* used to send a message of type REPL_WILL_RESTART_WITH_INFO or REPL_ROLLBACK_FIRST or REPL_RESYNC_SEQNO */
{
	int4		type;
	int4		len;
	unsigned char	start_seqno[SIZEOF(seq_num)];
	unsigned char	jnl_ver;
	char		start_flags[4];
	char		proto_ver;	/* Needs to be "signed char" in order to be able to do signed comparisons of this with
					 * the macros REPL_PROTO_VER_DUALSITE (0) and REPL_PROTO_VER_UNINITIALIZED (-1) */
	char		node_endianness;	/* 'L' if this machine is little endian, 'B' if it is big endian */
	char		is_supplementary;	/* TRUE for a supplementary instance; FALSE otherwise */
	char		filler_32[8];
} repl_start_reply_msg_t; /* The first two fields should be as in repl_msg_t */

typedef struct	/* used to send a message of type REPL_FETCH_RESYNC */
{
	int4		type;
	int4		len;
	seq_num		resync_seqno;
	char		proto_ver;	/* Needs to be "signed char" in order to be able to do signed comparisons of this with
					 * the macros REPL_PROTO_VER_DUALSITE (0) and REPL_PROTO_VER_UNINITIALIZED (-1) */
	char		node_endianness;	/* 'L' if this machine is little endian, 'B' if it is big endian */
	char		is_supplementary;	/* TRUE for a supplementary instance; FALSE otherwise */
	char		filler_32[13];
} repl_resync_msg_t; /* The first two fields should be as in repl_msg_t */

typedef struct	/* used to send a message of type REPL_OLD_NEED_INSTANCE_INFO */
{
	int4		type;
	int4		len;
	unsigned char	instname[MAX_INSTNAME_LEN];
	char		proto_ver;	/* Needs to be "signed char" in order to be able to do signed comparisons of this with
					 * the macros REPL_PROTO_VER_DUALSITE (0) and REPL_PROTO_VER_UNINITIALIZED (-1) */
	char		node_endianness;	/* 'L' if this machine is little endian, 'B' if it is big endian */
	char		is_rootprimary;	/* Whether the source server that is sending this message is a root primary or not. */
	char		filler_32[5];
} repl_old_needinst_msg_t; /* The first two fields should be as in repl_msg_t */

typedef struct	/* used to send a message of type REPL_NEED_INSTINFO */
{
	int4		type;
	int4		len;
	unsigned char	instname[MAX_INSTNAME_LEN];
	repl_inst_uuid	lms_group_info;
	char		proto_ver;	/* Needs to be "signed char" in order to be able to do signed comparisons of this with
					 * the macros REPL_PROTO_VER_DUALSITE (0) and REPL_PROTO_VER_UNINITIALIZED (-1) */
	char		is_rootprimary;	/* Whether the source server that is sending this message is a root primary or not. */
	char		is_supplementary;	/* TRUE for a supplementary instance; FALSE otherwise */
	char		filler_32[5];
} repl_needinst_msg_t; /* The first two fields should be as in repl_msg_t */

typedef struct	/* used to send a message of type REPL_OLD_INSTANCE_INFO */
{
	int4		type;
	int4		len;
	unsigned char	instname[MAX_INSTNAME_LEN];	/* The name of this replication instance */
	unsigned char	was_rootprimary;
	char		filler_32[7];
} repl_old_instinfo_msg_t; /* The first two fields should be as in repl_msg_t */

typedef struct	/* used to send a message of type REPL_INSTINFO */
{
	int4		type;
	int4		len;
	unsigned char	instname[MAX_INSTNAME_LEN];	/* The name of this replication instance */
	seq_num		strm_jnl_seqno;		/* Whenever a supplementary receiver handshakes with a non-supplementary source,
						 * the current jnl seqno (which is across all streams) is sent in the initial
						 * REPL_START_JNL_SEQNO message. Only after the source server sends the
						 * REPL_NEED_INSTINFO message does the receiver know which non-supplementary stream
						 * the source corresponds to and can therefore determine the stream specific
						 * jnl seqno. Once this is known, instead of re-sending the REPL_START_JNL_SEQNO
						 * message, the receiver sends this new seqno in the "strm_jnl_seqno" field as
						 * part of the REPL_INSTINFO message. In cases where the receiver is not
						 * supplementary or if the soruce is supplementary, this field is set to 0.
						 */
	repl_inst_uuid	lms_group_info;
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

typedef struct	/* used to send a message of type REPL_NEED_HISTINFO */
{
	int4		type;
	int4		len;
	seq_num		seqno;		/* The histinfo requested is that of "seqno-1" */
	int4		strm_num;	/* used only in case of supplementary instances */
	int4		histinfo_num;	/* if not INVALID_HISTINFO_NUM, then we want the histinfo_num'th history record
					 * on the remote side. in this case ignore seqno & strm_num.
					 */
	char		filler_32[8];
} repl_needhistinfo_msg_t; /* The first two fields should be as in repl_msg_t */

typedef struct	/* used to send a message of type REPL_OLD_TRIPLEINFO1 */
{
	int4		type;
	int4		len;
	seq_num		start_seqno;	/* the starting seqno of the histinfo range */
	unsigned char	instname[MAX_INSTNAME_LEN];
} repl_histinfo1_msg_t; /* The first two fields should be as in repl_msg_t */

typedef struct	/* used to send a message of type REPL_OLD_TRIPLEINFO2 */
{
	int4		type;
	int4		len;
	seq_num		start_seqno;	/* the starting seqno of the histinfo range */
	uint4		cycle;
	int4		histinfo_num;
	char		filler_32[8];
} repl_histinfo2_msg_t; /* The first two fields should be as in repl_msg_t */

typedef struct	/* used to send a message of type REPL_HISTINFO */
{
	int4		type;
	int4		len;
	repl_histinfo	history;
} repl_histinfo_msg_t; /* The first two fields should be as in repl_msg_t */

/* A REPL_OLD_TRIPLE message is sent in such a form that the receiver server can copy it (after removing the 8-byte header
 * containing the "type" and "len") as such into the receive pool. The update process treats this content as yet another
 * journal record type. Therefore we need to introduce a new journal record type (JRT_TRIPLE) for this purpose and define a
 * structure (repl_old_triple_jnl_t) that has this type information and the triple related information. This will then be a
 * member of the REPL_OLD_TRIPLE message.
 * Note: GT.M versions that support supplementary instances send a REPL_HISTREC message instead of REPL_OLD_TRIPLE.
 */
typedef struct	/* sub-structure used to send a message of type REPL_OLD_TRIPLE */
{
	uint4		jrec_type : 8;	/* This definition is copied from that of "jrec_prefix" */
	uint4		forwptr : 24;	/* This definition is copied from that of "jrec_prefix" */
	uint4		cycle;		/* The cycle of the instance at the time of generating this triple */
	seq_num		start_seqno;	/* The starting seqno of the triple range */
	unsigned char	instname[MAX_INSTNAME_LEN];	/* The instance name that generated this triple */
	unsigned char	rcvd_from_instname[MAX_INSTNAME_LEN];	/* instance name this triple was received from (on the secondary) */
} repl_old_triple_jnl_t;

typedef struct	/* used to send a message of type REPL_OLD_TRIPLE */
{
	int4			type;
	int4			len;
	repl_old_triple_jnl_t	triplecontent;
} repl_old_triple_msg_t; /* The first two fields should be as in repl_msg_t */

typedef struct	/* sub-structure used to send a message of type REPL_HISTREC */
{
	uint4		jrec_type : 8;	/* This definition is copied from that of "jrec_prefix" */
	uint4		forwptr : 24;	/* This definition is copied from that of "jrec_prefix" */
	uint4		filler_8byte_align;
	repl_histinfo	histcontent;	/* the structure containing the history information */
} repl_histrec_jnl_t;

typedef struct	/* used to send a message of type REPL_HISTREC */
{
	int4			type;
	int4			len;
	repl_histrec_jnl_t	histjrec;	/* the journal record containing the history information */
} repl_histrec_msg_t; /* The first two fields should be as in repl_msg_t */

typedef struct	/* used to send a message of type REPL_NEED_STRMINFO */
{
	int4		type;
	int4		len;
	seq_num		seqno;	/* The strminfo requested is that of "seqno-1" */
	char		filler_32[16];
} repl_needstrminfo_msg_t; /* The first two fields should be as in repl_msg_t */

typedef struct	/* used to send a message of type REPL_STRMINFO */
{
	int4		type;
	int4		len;
	int4		last_histinfo_num[MAX_SUPPL_STRMS];
} repl_strminfo_msg_t; /* The first two fields should be as in repl_msg_t */

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

typedef struct
{
	int4		type;
	int4		len;
	int4		fullpath_len;
	uint4		pid;
	char		proto_ver;
	char		filler_32[15]; /* to ensure that at least 32 bytes are sent across (gtmrecv_fetchresync relies on this) */
	char		fullpath[REPL_LOGFILE_PATH_MAX + 1]; /* + 1 for null-terminator */
} repl_logfile_info_msg_t;

#define REPL_LOGFILE_INFO_MSGHDR_SZ	OFFSETOF(repl_logfile_info_msg_t, fullpath[0])

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(save)
# pragma pointer_size(long)
#endif

typedef repl_msg_t		*repl_msg_ptr_t;
typedef repl_cmpmsg_t		*repl_cmpmsg_ptr_t;
typedef repl_start_msg_t	*repl_start_msg_ptr_t;
typedef repl_start_reply_msg_t	*repl_start_reply_msg_ptr_t;
typedef repl_resync_msg_t	*repl_resync_msg_ptr_t;
typedef repl_old_needinst_msg_t	*repl_old_needinst_msg_ptr_t;
typedef repl_needinst_msg_t	*repl_needinst_msg_ptr_t;
typedef repl_old_instinfo_msg_t	*repl_old_instinfo_msg_ptr_t;
typedef repl_instinfo_msg_t	*repl_instinfo_msg_ptr_t;
typedef repl_needhistinfo_msg_t	*repl_needhistinfo_msg_ptr_t;
typedef repl_histinfo1_msg_t	*repl_histinfo1_msg_ptr_t;
typedef repl_histinfo2_msg_t	*repl_histinfo2_msg_ptr_t;
typedef repl_old_triple_jnl_t	*repl_old_triple_jnl_ptr_t;
typedef repl_histrec_jnl_t	*repl_histrec_jnl_ptr_t;
typedef repl_histinfo_msg_t	*repl_histinfo_msg_ptr_t;
typedef repl_needstrminfo_msg_t	*repl_needstrminfo_msg_ptr_t;
typedef repl_strminfo_msg_t	*repl_strminfo_msg_ptr_t;
typedef repl_heartbeat_msg_t	*repl_heartbeat_msg_ptr_t;
typedef repl_cmpinfo_msg_t	*repl_cmpinfo_msg_ptr_t;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(restore)
#endif

void	gtmsource_repl_send(repl_msg_ptr_t msg, char *msgtypestr, seq_num optional_seqno, int4 optional_strm_num);
void	gtmsource_send_new_histrec(void);

void	gtmrecv_repl_send(repl_msg_ptr_t msgp, int4 type, int4 len, char *msgtypestr, seq_num optional_seqno);
void	gtmrecv_send_histinfo(repl_histinfo *histinfo);
void	gtmrecv_check_and_send_instinfo(repl_needinst_msg_ptr_t need_instinfo_msg, boolean_t is_rcvr_srvr);
uint4   repl_logfileinfo_get(char *logfile, repl_logfile_info_msg_t *msgp, boolean_t cross_endian, FILE *logfp);

#endif
