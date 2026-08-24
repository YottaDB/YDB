/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
/* Pull in error defs */
#include "ydbcmerrors.h"

#define	ZAREQUEST_SENT		16
#define	LREQUEST_SENT		8
#define	REQUEST_PENDING		4
#define	REMOTE_ZALLOCATES	2
#define	REMOTE_LOCKS		1
#define	REMOTE_CLR_MASK		(ZAREQUEST_SENT + LREQUEST_SENT + REMOTE_ZALLOCATES + REMOTE_LOCKS)
#define	CM_BUFFER_OVERHEAD	20
#define	CM_BLKPASS		40

#define	CMM_PROTOCOL_TYPE	"GCM"

#define	S_PROTOCOL		"VAXVMSGTM023GCM010               "
#define	S_HDRSIZE		1
#define	S_PROTSIZE		33
#define	S_REGINSIZE		6
#define	S_LAFLAGSIZE		1
#define	S_SUBLISTSIZE		1
#define	CM_MINBUFSIZE		512 + CM_BUFFER_OVERHEAD

#define	CMLCK_REQUEUE		0
#define	CM_LOCKS		0
#define	CM_ZALLOCATES		0x80
#define	CM_NOLKCANCEL		256

typedef enum
{
	CM_READ = 0,
	CM_WRITE = 1,
	CM_NOOP = 2,
} cm_op_t;

#define CMMS_E_ERROR		1	/* [0x01] */
#define CMMS_L_LKCANALL		2	/* [0x02] */
#define CMMS_L_LKCANCEL		3	/* [0x03] */
#define CMMS_L_LKDELETE		4	/* [0x04] */
#define CMMS_L_LKREQIMMED	5	/* [0x05] */
#define CMMS_L_LKREQNODE	6	/* [0x06] */
#define CMMS_L_LKREQUEST	7	/* [0x07] */
#define CMMS_L_LKRESUME		8	/* [0x08] */
#define CMMS_L_LKACQUIRE	9	/* [0x09] */
#define CMMS_L_LKSUSPEND	10	/* [0x0A] */
#define CMMS_M_LKABORT		11	/* [0x0B] */
#define CMMS_M_LKBLOCKED	12	/* [0x0C] */
#define CMMS_M_LKGRANTED	13	/* [0x0D] */
#define CMMS_M_LKDELETED	14	/* [0x0E] */
#define CMMS_M_LKSUSPENDED	15	/* [0x0F] */
#define CMMS_Q_DATA		16	/* [0x10] */
#define CMMS_Q_GET		17	/* [0x11] */
#define CMMS_Q_KILL		18	/* [0x12] */
#define CMMS_Q_ORDER		19	/* [0x13] */
#define CMMS_Q_PREV		20	/* [0x14] */
#define CMMS_Q_PUT		21	/* [0x15] */
#define CMMS_Q_QUERY		22	/* [0x16] */
#define CMMS_Q_ZWITHDRAW	23	/* [0x17] */
#define CMMS_R_DATA		24	/* [0x18] */
#define CMMS_R_GET		25	/* [0x19] */
#define CMMS_R_KILL		26	/* [0x1A] */
#define CMMS_R_ORDER		27	/* [0x1B] */
#define CMMS_R_PREV		28	/* [0x1C] */
#define CMMS_R_PUT		29	/* [0x1D] */
#define CMMS_R_QUERY		30	/* [0x1E] */
#define CMMS_R_ZWITHDRAW	31	/* [0x1F] */
#define CMMS_R_UNDEF		32	/* [0x20] */
#define CMMS_S_INITPROC		33	/* [0x21] */
#define CMMS_S_INITREG		34	/* [0x22] */
#define CMMS_S_TERMINATE	35	/* [0x23] */
#define CMMS_S_INTERRUPT	36	/* [0x24] */
#define CMMS_T_INITPROC		37	/* [0x25] */
#define CMMS_T_REGNUM		38	/* [0x26] */
#define CMMS_X_INQPROC		39	/* [0x27] */
#define CMMS_X_INQPRRG		40	/* [0x28] */
#define CMMS_X_INQREG		41	/* [0x29] */
#define CMMS_Y_STATPROCREC	42	/* [0x2A] */
#define CMMS_Y_STATPRRGREC	43	/* [0x2B] */
#define CMMS_Y_STATREGREC	44	/* [0x2C] */
#define CMMS_U_LKEDELETE	45	/* [0x2D] */
#define CMMS_U_LKESHOW		46	/* [0x2E] */
#define CMMS_V_LKESHOW		47	/* [0x2F] */
#define CMMS_E_TERMINATE	48	/* [0x30] */
#define CMMS_B_BUFRESIZE	49	/* [0x31] */
#define CMMS_B_BUFFLUSH		50	/* [0x32] */
#define CMMS_C_BUFRESIZE	51	/* [0x33] */
#define CMMS_C_BUFFLUSH		52	/* [0x34] */
#define CMMS_Q_INCREMENT	53	/* [0x35] */	/* Opcode for message type sent from   client (to   server) */
#define CMMS_R_INCREMENT	54	/* [0x36] */	/* Opcode for message type received by client (from server) */
#define CMMS_Q_REVERSEQUERY	55	/* [0x37] */
#define CMMS_R_REVERSEQUERY	56	/* [0x38] */

#define	CMM_QUERYGET_MIN_LEVEL		"200"	/* $query works as queryget only from version "V200" onwards */
#define	CMM_INCREMENT_MIN_LEVEL		"210"	/* $INCREMENT works only from version "V210" onwards */
#define	CMM_STDNULLCOLL_MIN_LEVEL	"210"	/* Standard null collation works only from version "V210" onwards */
#define	CMM_LONGNAMES_MIN_LEVEL		"210"	/* long name works only from protocol "V210" onwards */
#define	CMM_REVERSEQUERY_MIN_LEVEL	"220"	/* Reverse $query works only from protocol "V220" onwards */

typedef struct cm_region_list_struct
	{
		que_ent				regque;
		struct cm_region_list_struct	*next;
		unsigned char			regnum;
		unsigned char			oper;
		unsigned short			lks_this_cmd;
		bool				reqnode;
		char				filler[3];
		struct cm_region_head_struct	*reghead;
		struct cs_struct		*cs;
		struct mlk_pvtblk_struct	*blkd;
		struct mlk_pvtblk_struct	*lockdata;
		uint4				pini_addr;
	} cm_region_list;

typedef struct cs_struct
	{
		que_ent				qent;
		cm_region_list			*region_root;
		cm_region_list			*current_region;
		struct CLB			*clb_ptr;
		unsigned char			state;
		unsigned char			new_msg;
		unsigned char			maxregnum;
		bool				waiting_in_queue;
		struct timespec			connect;	/* Debugging tool -- time connection was established */
		time_t				lastact;	/* Debugging tool -- time of last server action */
		uint4				stats;
		unsigned short			procnum;
		unsigned short			transnum;
		unsigned short			lk_cancel;
		unsigned short			last_cancelled; /* hold transnum of last cancelled lock request */
		struct           /* hold info from interrupt cancel msg */
		  {              /* laflag can be 0, x40, x80 */
		    unsigned char               laflag;    /* + 1 if valid */
		    unsigned char               transnum;  /* for lk_cancel */
		  } int_cancel;
		struct jnl_process_vector_struct *pvec;
		boolean_t			query_is_queryget; /* based on client/server protocol levels, query == queryget */
		boolean_t			err_compat; /* based on client/server protocol levels (and platform type),
							     * rts_error mechanism b/n client and server might be different */
		boolean_t			cli_supp_allowexisting_stdnullcoll;/* decided based on client's protocol levels */
		boolean_t			client_supports_long_names; /* based on client's levels */
		cm_region_list			*region_array[256];	/* [UCHAR_MAX + 1] speed up gtcm_find_region */
	} connection_struct;

typedef struct cm_region_head_struct
	{
		relque				head;
		struct cm_region_head_struct	*next;
		struct cm_region_head_struct	*last;
		connection_struct		*connect_ptr;
		struct gd_region_struct		*reg;
		gtm_uint64_t			wakeup;
		uint4				refcnt;
		hash_table_mname		*reg_hash;
	} cm_region_head;

typedef struct cm_lk_response_struct
	{
		struct cm_lk_response_struct	*next;
		struct CLB			*response;
	} cm_lk_response;

typedef struct link_info_struct
	{
		unsigned char			neterr;
		unsigned char			lck_info;
		unsigned char			lnk_active;
		char				filler;
		struct mlk_pvtblk_struct	*netlocks;
		unsigned short			procnum;
		unsigned short			buffered_count;
		unsigned short			buffer_size;
		unsigned short			buffer_used;
		unsigned char			*buffer;
		boolean_t			convert_byteorder;
		boolean_t			query_is_queryget; /* based on client/server protocol levels, query == queryget */
		boolean_t			err_compat; /* based on client/server protocol levels (and platform type),
							     * rts_error mechanism b/n client and server might be different */
		cm_lk_response			lk_response;
		boolean_t			server_supports_dollar_incr;	/* decided based on server protocol levels */
		boolean_t			server_supports_std_null_coll;	/* decided based on server protocol levels */
		boolean_t			server_supports_long_names;	/* decided based on server protocol levels */
		boolean_t			server_supports_reverse_query;	/* decided based on server protocol levels */
	} link_info;

typedef struct
	{
		char	code;
		char	rnum;
		bool	all;
		bool	interactive;
		int4	pid;
		char	nodelength;
		char	node[32];
	} clear_request;

typedef struct
	{
		char	code;
		char	filler[3];
		int4	status;
		int4	locknamelength;
		char	lockname[256];
	} clear_reply;

typedef struct
	{
		char	code;
		bool	clear;
	} clear_confirm;

typedef struct
	{
		char	code;
		char	rnum;
		bool	all;
		bool	wait;
		int4	pid;
		char	nodelength;
		char	node[32];
	} show_request;

typedef struct
	{
		char	code;
		char	line[256];
	} show_reply;

#define CM_CPU_OFFSET			0
#define CM_OS_OFFSET			3
#define CM_IMPLEMENTATION_OFFSET	6
#define CM_VERSION_OFFSET		9
#define CM_TYPE_OFFSET			12
#define CM_LEVEL_OFFSET			15
#define CM_ENDIAN_OFFSET		18

#define CM_FILLER_SIZE			14

typedef struct
	{
		char msg[S_PROTSIZE];
	} protocol_msg;

#define CM_PUT_USHORT(PTR, USVAL, CONVFLAG)				\
	{								\
		if (CONVFLAG)						\
		{							\
			unsigned short val = GTM_BYTESWAP_16(USVAL);	\
			PUT_USHORT(PTR, val);				\
		}							\
		else							\
			PUT_USHORT(PTR, USVAL);				\
	}

#define CM_PUT_SHORT(PTR, SVAL, CONVFLAG)			\
	{							\
		if (CONVFLAG)					\
		{						\
			short val = GTM_BYTESWAP_16(SVAL);	\
			PUT_SHORT(PTR, val);			\
		}						\
		else						\
			PUT_SHORT(PTR, SVAL);			\
	}

#define CM_PUT_ULONG(PTR, ULVAL, CONVFLAG)			\
	{							\
		if (CONVFLAG)					\
		{						\
			uint4  val = GTM_BYTESWAP_32(ULVAL);	\
			PUT_ULONG(PTR, val);			\
		}						\
		else						\
			PUT_ULONG(PTR, ULVAL);			\
	}

#define CM_PUT_LONG(PTR, LVAL, CONVFLAG)			\
	{							\
		if (CONVFLAG)					\
		{						\
			int4  val = GTM_BYTESWAP_32(LVAL);	\
			PUT_LONG(PTR, val);			\
		}						\
		else						\
			PUT_LONG(PTR, LVAL);			\
	}

#define CM_GET_USHORT(USVAR, PTR, CONVFLAG)		\
	{						\
		if (CONVFLAG)				\
		{					\
			unsigned short val;		\
			GET_USHORT(val, (PTR));		\
			USVAR = GTM_BYTESWAP_16(val);	\
		}					\
		else					\
			GET_USHORT((USVAR), (PTR));	\
	}

#define CM_GET_SHORT(SVAR, PTR, CONVFLAG)		\
	{						\
		if (CONVFLAG)				\
		{					\
			short val;			\
			GET_SHORT(val, (PTR));		\
			SVAR = GTM_BYTESWAP_16(val);	\
		}					\
		else					\
			GET_SHORT((SVAR), (PTR));	\
	}

#define CM_GET_ULONG(ULVAR, PTR, CONVFLAG)		\
	{						\
		if (CONVFLAG)				\
		{					\
			uint4 val;			\
			GET_ULONG(val, (PTR));		\
			ULVAR = GTM_BYTESWAP_32(val);	\
		}					\
		else					\
			GET_ULONG((ULVAR), (PTR));	\
	}

#define CM_GET_LONG(LVAR, PTR, CONVFLAG)		\
	{						\
		if (CONVFLAG)				\
		{					\
			int4 val;			\
			GET_LONG(val, (PTR));		\
			LVAR = GTM_BYTESWAP_32(val);	\
		}					\
		else					\
			GET_LONG((LVAR), (PTR));	\
	}

/* ------------------------------------------------------------------------------------------------------------------
 * Bounds checking of an inbound GT.CM GNP message.
 *
 * The GT.CM GNP server hands the raw client message buffer ("clb_ptr->mbf") to a gtcmtr_*() handler, which walks it
 * with "*ptr++" and GET_USHORT() and feeds the lengths it finds there to memcpy()/malloc(). Every one of those
 * lengths is chosen by the client, so each has to be checked against (a) how many bytes were actually received and
 * (b) how big the destination is. And the check has to happen BEFORE the length is adjusted: a "len--" on an
 * "unsigned short" 0 wraps around to 65535.
 *
 * "clb_ptr->cbl" holds the number of bytes cmj_fini() actually read for the message currently being processed, so it
 * is the authoritative end of the message. Note that "clb_ptr->mbl" is NOT: it is the size of the allocated buffer,
 * which is almost always larger than the message sitting in it.
 * ------------------------------------------------------------------------------------------------------------------
 */
#define	CM_CLB_MSG_END(CLB)		((CLB)->mbf + (CLB)->cbl)
#define	CM_CLB_BYTES_LEFT(CLB, PTR)	((CM_CLB_MSG_END(CLB) > (unsigned char *)(PTR))				\
						? (size_t)(CM_CLB_MSG_END(CLB) - (unsigned char *)(PTR)) : (size_t)0)
#define	CM_MSG_END(CNX)			CM_CLB_MSG_END((CNX)->clb_ptr)
#define	CM_MSG_BYTES_LEFT(CNX, PTR)	CM_CLB_BYTES_LEFT((CNX)->clb_ptr, PTR)

/* Reject the message currently being processed. Names the message type so the operator log identifies the sender's
 * intent. Does not return.
 */
#define	CM_BADMSG(CNX)												\
	RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_BADGTMNETMSG, 1, (int4)*(CNX)->clb_ptr->mbf)

/* Verify NBYTES more bytes of the current message can be consumed starting at PTR */
#define	CM_CHECK_AVAIL(CNX, PTR, NBYTES)									\
MBSTART {													\
	if ((size_t)(NBYTES) > CM_MSG_BYTES_LEFT(CNX, PTR))							\
		CM_BADMSG(CNX);											\
} MBEND

/* The client side of the same problem. GT.CM GNP clients and their servers are designed to run within the same
 * security zone, so this is not about a hostile server: the lengths in a reply need checking against what was
 * received for the same reason the ones in a request do, so that a malformed message cannot walk a pointer off the
 * end of the buffer. BADSRVRNETMSG is what the client code already raises for a reply it cannot parse. Does not
 * return on failure.
 */
#define	CM_CHECK_AVAIL_REPLY(CLB, PTR, NBYTES)									\
MBSTART {													\
	if ((size_t)(NBYTES) > CM_CLB_BYTES_LEFT(CLB, PTR))							\
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_BADSRVRNETMSG);					\
} MBEND

/* TRUE for a (BASE, END, PREV) triple that does not describe a well formed gv_key. A key assembled out of a message
 * describes itself with "end" and "prev", and the runtime indexes "base" with both, writing as well as reading: "end"
 * is the offset of the second of the two <NUL> bytes that terminate a key, and "prev" the offset of the last
 * subscript. LIMIT is one past the last byte of BASE holding message data, so a key has to end inside it. A caller
 * with no "prev" worth checking passes 0, which turns that term off.
 */
#define	CM_BAD_KEY_SHAPE(BASE, END, PREV, LIMIT)							\
	((2 > (END)) || ((END) >= (LIMIT))								\
		|| (KEY_DELIMITER != (BASE)[(END)]) || (KEY_DELIMITER != (BASE)[(END) - 1])		\
		|| ((0 != (PREV)) && ((PREV) >= (END))))

/* Read the "<length> <region number>" prefix that the CMMS_Q_* messages carry, where <length> counts the region
 * number itself plus whatever follows. Verifies the prefix was actually received and that <length> accounts for at
 * least the region number, BEFORE the region number is subtracted from it: subtracting first would turn a
 * client-supplied 0 into 65535.
 */
#define	CM_GET_REGNUM(CNX, PTR, REGNUM, LEN)									\
MBSTART {													\
	CM_CHECK_AVAIL(CNX, PTR, SIZEOF(unsigned short) + SIZEOF(unsigned char));				\
	GET_USHORT(LEN, PTR);											\
	(PTR) += SIZEOF(unsigned short);									\
	if (0 == (LEN))												\
		CM_BADMSG(CNX);											\
	REGNUM = *(PTR)++;											\
	(LEN)--;	/* subtract size of regnum */								\
} MBEND

#define CM_GET_GVCURRKEY(PTR, LEN)	PTR = gtcmtr_get_key(gv_currkey, PTR, LEN);
