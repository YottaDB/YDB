/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef DDPHDR_H_INCLUDED
#define DDPHDR_H_INCLUDED

typedef int4 condition_code;

#define MAX_ETHER_DATA_SIZE 		1500
#define ETHERNET_HEADER_SIZE 		14	/* Is size of Destination address + source address + protocol # */
#define ETHERADDR_LENGTH		6
#define MIN_ETH_RECV_BUFCNT		1	/* from OpenVMS I/O User's Reference Manual, system default */
#define MAX_ETH_RECV_BUFCNT		255	/* from OpenVMS I/O User's Reference Manual */
#define ETH_RCV_BUFCNT			64	/* GT.CM DDP default */
#define DDP_ETH_PROTO_TYPE		((unsigned short)0x3980)	/* will be seen as 8039 on the wire */

#define DDP_MIN_MSG_LEN			64
#define DDP_PROTO_VERSION		0x02 /* MSM, DSM-11 use version 2; DSM 6.0 & 6.1, DDP-DOS V1.00.8+ use version 4 */
#define DDP_MSG_TERMINATOR		0xff

#define DDP_CIRCUIT_NAME_LEN		3
#define DDP_VOLUME_NAME_LEN		3
#define DDP_UCI_NAME_LEN		3

/* Request codes */
#define DDPTR_USEREXIT 		0x00	/* [00]	*/	/* Individual user sends message to agent to clean up for exit. Per DSM doc,
					 	    	 * this code is for an ERROR ON RRB (read request buffer) */
#define DDPTR_LOCK		0x02	/* [02]	*/	/* Not implemented */
#define DDPTR_UNLOCK		0x04	/* [04]	*/	/* Not implemented */
#define DDPTR_ZALLOC 		0x06	/* [06] */
#define DDPTR_ZDEALLOC 		0x08	/* [08] */
#define DDPTR_GET 		0x0a	/* [10] */
#define DDPTR_PUT 		0x0c	/* [12] */
#define DDPTR_KILL 		0x0e	/* [14] */
#define DDPTR_ORDER 		0x10	/* [16] */
#define DDPTR_QUERY 		0x12	/* [18] */
#define DDPTR_DEFINE 		0x14	/* [20] */
#define DDPTR_PREVIOUS 		0x16	/* [22] */
#define DDPTR_JOB		0x18	/* [24] */	/* Not implemented */
#define DDPTR_RESPONSE 		0x1a	/* [26] */
#define DDPTR_ERRESPONSE 	0x1c	/* [28] */
#define DDPTR_RDRQBUF		0x1e	/* [30] */	/* READ REQUEST BUFFER, not implemented */
#define DDPTR_ANNOUNCE 		0x20	/* [32] */

/* Response codes other than DDPTR_RESPONSE and DDPTR_ERRESPONSE */
#define DDPTRX_ANNOUNCE 	0x80	/* [128] */	/* Announcement that we are connecting */
#define DDPTRX_NOCONNECT 	0x81	/* [129] */	/* Named volume set not connected */
#define DDPTRX_CONGESTION 	0x82	/* [130] */	/* Agent congestion */
#define DDPTRX_RUNDOWN 		0x83	/* [131] */	/* Client wishes to disconnect */
#define DDPTRX_SHUTDOWN		0x84	/* [132] */	/* Shutdown the server */
#define DDPTRX_DUMP 		0x85	/* [133] */	/* Start trace */

/* GT.M extended reference form is ^|"globaldirectory"|global, DSM is ^["UCI","VOL"]global */
#define GTM_EXTREF_PREFIX	"^|\""
#define GTM_EXTREF_SUFFIX	"\"|"
#define DSM_EXTREF_PREFIX	"^[\""
#define DSM_UCI_VOL_SEPARATOR	"\",\""
#define DSM_EXTREF_SUFFIX	"\"]"
#define DSM_EXTREF_FORM_LEN	(STR_LIT_LEN(DSM_EXTREF_PREFIX) + \
				 DDP_UCI_NAME_LEN + \
				 STR_LIT_LEN(DSM_UCI_VOL_SEPARATOR) + \
				 DDP_VOLUME_NAME_LEN + \
				 STR_LIT_LEN(DSM_EXTREF_SUFFIX))

#define DDP_VOLSET_CONF_LOGICAL_PREFIX	"GTMDDP_VOLCONF_"
#define DDP_GROUP_LOGICAL_PREFIX	"GTMDDP_GROUPS_"
#define DDP_ETHER_DEV_PREFIX		"GTMDDP_CONTROLLER_"
#define DDP_MAXRECSIZE_PREFIX		"GTMDDP_MAXRECSIZE_"
#define DDP_ETH_RCV_BUFCNT_PREFIX	"GTMDDP_ETHRCVBUFCNT_"
#define DDP_MAXREQCREDITS_PREFIX	"GTMDDP_MAXREQCREDITS_"
#define DDP_CLIENT_CKTNAM_LOGI		"GTMDDP_CIRCUIT_NAME"

#define DDP_MAX_VOLSETS			16
#define DDP_TEXT_ID_LENGTH		3
#define DDP_ANNOUNCE_CODE_LEN		2		/* "WI" or "II" */
#define MAXIMUM_PROCESSES		256
#define DDP_MAX_GROUP			16
#define DDP_DEFAULT_GROUP_MASK		0x0001		/* member of group 0 */
#define DDP_MIN_RECSIZE			1024
#define DDP_LEAST_MAXREQCREDITS		1
#define DDP_LARGEST_MAXREQCREDITS	0xFF

/* some constants that were derived from DSM packets */
#define DDP_GROUP_MASK			0x0101
#define DDP_ADVERTISE_INTERVAL		0x00
#define DDP_MAX_REQUEST_CREDITS		0x04
#define DDP_CPU_TYPE			0x01
#define DDP_SOFTWARE_VERSION		0x00
#define DDP_CPU_LOAD_RATING		0x00
#define DDP_AUTOCONFIGURE_VERSION	0x02
#define DDP_ANNOUNCE_FILLER3_LITERAL	0x01ff00ff
#define DDP_GLOBAL_TYPE			0x02

/* DDP node status bit masks */
#define DDP_NODE_STATUS_READ		0x01 /* bit is 1 if read locked, 0 if read enabled */
#define DDP_NODE_STATUS_WRITE		0x02 /* bit is 1 if write locked, 0 if write enabled */
#define DDP_NODE_STATUS_STATE		0x04 /* bit is 1 if unreachable, 0 if reachable */
#define DDP_NODE_STATUS_CHANGE		0x10 /* bit is 1 if state change occurred on circuit */
#define DDP_NODE_STATUS_DISABLED	0x20 /* bit is 1 if disabled, 0 if enabled */

#define DDP_NODE_STATUS_ALL_CLEAR	0x00 /* read enabled + write enabled + reachable + no state change + circuit enabled */

#define DDP_LOG_ERROR(err_len, err_string)											\
{																\
	now_t	now;	/* for GET_CUR_TIME macro */										\
	char	time_str[CTIME_BEFORE_NL + 2]; /* for GET_CUR_TIME macro*/							\
	char	*time_ptr; /* for GET_CUR_TIME macro*/										\
	bool	save_dec_nofac;													\
																\
	GET_CUR_TIME;														\
	save_dec_nofac = dec_nofac; /* save for later restore */								\
	dec_nofac = TRUE; /* don't need error mnemonic prefix, just print the message contents */				\
	dec_err(VARLSTCNT(6) ERR_DDPLOGERR, 4, CTIME_BEFORE_NL, time_ptr, (err_len), (err_string));				\
	dec_nofac = save_dec_nofac; /* back to what it was */									\
}

/* All structures that are DDP messages should be packed; do not let the compiler pad for structure member alignment */
#if defined(__alpha)
# pragma member_alignment save
# pragma nomember_alignment
#endif

typedef struct
{
	unsigned short uci;
	unsigned short volset;
} ddp_info;

typedef struct
{
	unsigned char	trancode;
	unsigned char	proto;
	unsigned short	source_circuit_name;	/* 5-bit format */
	unsigned short	source_job_number;
	unsigned short	remote_circuit_name;	/* 5-bit format */
	unsigned short	remote_job_number;	/* 0000 if this is a request */
	unsigned char	message_number;
	unsigned char	filler1;		/* literal 00 */
	unsigned short	message_length;
	unsigned char	hdrlen;
	unsigned char	txt[1];
} ddp_hdr_t;

#define DDP_MSG_HDRLEN	0x0f /* excluding txt field of ddp_hdr_t */

struct frame_hdr
{
	unsigned short	frame_length;
	unsigned char	destination_address[ETHERADDR_LENGTH];
	unsigned char	source_address[ETHERADDR_LENGTH];
	unsigned char	protocol_type[2];
};

struct in_buffer_struct
{
	struct frame_hdr	fh;
	ddp_hdr_t		dh;
};

typedef struct
{ /* byte position in announce packet - 15 is the first position past the header, position starts from 0, header size is 15 bytes */
	unsigned short	filler0;			/* position 15: literal 0x0000 */
	unsigned char	code[DDP_ANNOUNCE_CODE_LEN];	/* position 17: "WI", or "II" */
	unsigned char	ether_addr[ETHERADDR_LENGTH];	/* position 19: Ethernet physical address for this node */
	unsigned short	circuit_name;			/* position 25: DDP Node (circuit) name */
	unsigned short	filler1;			/* position 27: reserved for possible name extension */
	unsigned short	filler2;			/* position 29: reserved for possible name extension */
	unsigned short	max_job_no;			/* position 31: max job # */
	unsigned short	group_mask;			/* position 33: DDP group number mask */
	unsigned char	advertise_interval;		/* position 35: Advertise interval in seconds */
	unsigned char	max_request_credits;		/* position 36: Maximum request credits */
	unsigned char	cpu_type;			/* position 37: Remote CPU type */
	unsigned char	version;			/* position 38: Version of software */
	unsigned char	cpu_load_rating;		/* position 39: CPU load rating */
	unsigned char	proto_version;			/* position 40: DDP protocol version in use */
	unsigned char	node_status;			/* position 41: see comments re: DDP_NODE_STATUS_* */
	unsigned char	autoconfigure_version;		/* position 42: DDP autoconfigure version */
	unsigned short	volset[DDP_MAX_VOLSETS];	/* position 43: 5 bit formal volset names */
	/******************************************* begin unknown **************************************************/
	unsigned char	filler3[32 + 4 + 8];		/* position 75: at position 107 we write literal 0x01ff00ff */
	/******************************************* end   unknown **************************************************/
	unsigned char	terminator;			/* position 119: DDP_MSG_TERMINATOR */
} ddp_announce_msg_t;

#define DDP_ANNOUNCE_MSG_LEN	105 /* from ddp_announce_msg_t */

typedef struct
{
	unsigned char	naked_size;
	unsigned short	uci;
	unsigned short	vol;
	unsigned char	global_type;
	unsigned char	global_len;
	unsigned char	global[1]; /* actually, global_len bytes of formatted global reference */
} ddp_global_request_t; /* immediately follows the message header (ddp_hdr) for global request. For SET, ASCII value follows */

#if defined(__alpha)
# pragma member_alignment restore
#endif

#endif /* DDPHDR_H_INCLUDED */
