/****************************************************************
 *								*
 * Copyright (c) 2014-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>
#include "gtm_string.h"

#include "stringpool.h"
#include "error.h"
#include "nametabtyp.h"
#include "namelook.h"
#include "io.h"
#include "iosp.h"
#include "gtm_netdb.h"
#include "gtm_socket.h"
#include "gtm_un.h"
#include "gtm_inet.h"
#include "gtm_ipv6.h"
#include "iosocketdef.h"
#include "op.h"
#include "mmemory.h"
#include "mvalconv.h"
#include "trans_log_name.h"
#include "zsocket.h"
#ifdef GTM_TLS
#include "gtm_caseconv.h"
#include "min_max.h"
#include "gtm_time.h"
#include "gtm_tls.h"
#endif

GBLREF spdesc		stringpool;
GBLREF io_pair		io_curr_device;
GBLREF io_log_name	*io_root_log_name;
GBLREF d_socket_struct	*socket_pool;
GBLREF io_pair		io_std_device;
GBLREF io_log_name	*dollar_principal;
GBLREF mstr		dollar_prin_log;
GBLREF mstr		dollar_zpin;			/* contains "< /" */
GBLREF mstr		dollar_zpout;			/* contains "> /" */

error_def(ERR_ZSOCKETATTR);
error_def(ERR_ZSOCKETNOTSOCK);
error_def(ERR_IONOTOPEN);

LITREF	mval		literal_zero;
LITREF	mval		literal_one;
LITREF	mval		literal_null;
LITREF	mval		skiparg;

#ifdef	GTM_TLS
#define	TLSCLIENTSTR	"CLIENT"
#define	TLSSERVERSTR	"SERVER"
#define TLSOPTIONLIT	"TLS option: "		/* for error message */
LITDEF char *zsocket_tls_options[] = {"CIPHER", "OPTIONS", "SESSION", "INTERNAL", "ALL", NULL};
/* TLS_OPTIONS_ are bits in a bitmask */
#define	TLS_OPTIONS_CIPHER	1
#define	TLS_OPTIONS_OPTIONS	2
#define	TLS_OPTIONS_SESSION	4
#define	TLS_OPTIONS_INTERNAL	8
#define TLS_OPTIONS_ALL_INDEX	4
#define	TLS_OPTIONS_ALL_MASK	(TLS_OPTIONS_CIPHER | TLS_OPTIONS_OPTIONS | TLS_OPTIONS_SESSION)
#define OPTIONPREFIXLEN		3		/* vertbar CPOS colon */
#endif

#define	OPTIONEND		','
#define	OPTIONENDSTR		","
#define OPTIONDISABLE		'!'
#define OPTIONVALUE		'='
#define OPTIONVALUESEP		';'

#define LENGTH_OF_OPTIONEND 	1
#define LENGTH_OF_OPTIONVALUE 	1

#define KEEPALIVE_STR	"KEEPALIVE"
#define KEEPIDLE_STR	"KEEPIDLE"
#define KEEPCNT_STR	"KEEPCNT"
#define KEEPINTVL_STR	"KEEPINTVL"
#define SNDBUF_STR	"SNDBUF"

typedef enum
{
	ZSIR_1_DETERMINE_LENGTH,
	ZSIR_2_RENDER_RESULT
} zsocket_item_round_t;

#define ZSOCKETITEM(A,B,C,D) {(SIZEOF(A) - 1), A}
const nametabent zsocket_names[] =
{
#include "zsockettab.h"		/* BYPASSOK */
};
#undef ZSOCKETITEM
const unsigned char zsocket_indextab[] =
{ /*	A  B  C  D  E  F  G  H  I  J  K  L  M  N */
	0, 0, 1, 2, 4, 4, 4, 4, 5, 7, 7, 11, 13, 14,
  /*	O   P   Q   R   S   T   U   V   W   X   Y   Z  end */
	15, 16, 18, 18, 20, 23, 24, 24, 24, 24, 24, 24, 28
};
#define ZSOCKETITEM(A,B,C,D) C
static const int zsocket_types[] =
{
#include "zsockettab.h"		/* BYPASSOK */
};
#undef ZSOCKETITEM
#define ZSOCKETITEM(A,B,C,D) D
static const int zsocket_level[] =
{
#include "zsockettab.h"
};
#undef ZSOCKETITEM

#define RETURN_SOCKOPT(OPTSTATE, OPTVALUE)				\
MBSTART {								\
	numret_set = 0;							\
	if (0 == (SOCKOPTIONS_USER & socketptr->options_state.OPTSTATE))\
	{	/* no user specified value */				\
		numret = socketptr->OPTVALUE = sockopt_value;		\
		numret_set = TRUE;					\
		socketptr->options_state.OPTSTATE |= SOCKOPTIONS_SYSTEM;\
	} else if (sockopt_value == socketptr->OPTVALUE)		\
	{	/* user value same as getsockopt so just return it */	\
		numret = sockopt_value;					\
		numret_set = TRUE;					\
	} else								\
	{	/* return both values */				\
		len = (2 * MAX_DIGITS_IN_INT) + 1;			\
		ENSURE_STP_FREE_SPACE(len);				\
		charptr = (char *)stringpool.free;			\
		charptr = (char *)i2asc((uchar_ptr_t)charptr, socketptr->OPTVALUE);	\
		*charptr++ = OPTIONVALUESEP;				\
		charptr = (char *)i2asc((uchar_ptr_t)charptr, sockopt_value);	\
		len = charptr - (char *)stringpool.free;		\
		dst->str.addr = (char *)stringpool.free;		\
		dst->str.len = len;					\
		UTF8_ONLY(dst->str.char_len = 0);			\
		stringpool.free += len;					\
		zsocket_type = MV_STR;					\
		numret_set = FALSE;					\
	}								\
} MBEND

LITDEF mval literal_local = DEFINE_MVAL_LITERAL(MV_STR | MV_NUM_APPROX, 0, 0, (SIZEOF("LOCAL") - 1), "LOCAL", 0, 0);
LITDEF mval literal_tcp = DEFINE_MVAL_LITERAL(MV_STR | MV_NUM_APPROX, 0, 0, (SIZEOF("TCP") - 1), "TCP", 0, 0);
LITDEF mval literal_tcp6 = DEFINE_MVAL_LITERAL(MV_STR | MV_NUM_APPROX, 0, 0, (SIZEOF("TCP6") - 1), "TCP6", 0, 0);
LITDEF char *zsocket_state_names[] = {"CONNECTED", "LISTENING", "BOUND", "CREATED", "CONNECTINPROGRESS"};
LITDEF char *zsocket_howcreated_names[] = {"LISTEN", "ACCEPTED", "CONNECT", "PRINCIPAL", "PASSED"};
/* Code scanners need this for a bounds check inspite of the above being hard coded names */
#define MAX_ZSOCKET_NAMES (sizeof"CONNECTINPROGRESS" - 1)

/* Macro to set the pointer to the target socket in the SOCKET device */
#define GET_SOCKETPTR_INDEX(DSOCK, INDEX, SOCKETPTR)				\
MBSTART {	/* if not even "," for third arg then force invalid index */	\
	INDEX = (NULL != arg1) ?						\
		(!M_ARG_SKIPPED(arg1) ? mval2i(arg1) : DSOCK->current_socket)	\
		: (DSOCK->n_socket + 1);					\
	if ((0 <= INDEX) && (0 < DSOCK->n_socket) && (INDEX < DSOCK->n_socket))	\
		SOCKETPTR = DSOCK->socket[INDEX];				\
	else	/* Index not in bounds; treat like non-existent socket */	\
		SOCKETPTR = NULL;						\
} MBEND

static void zsocket_options_item(
	zsocket_item_round_t round,
	char** charptr_ptr,
	int* determined_length_ptr,
	bool* need_optionend_flag_ptr,
	const char* caption_string,
	int value,
	int state);

void	op_fnzsocket(UNIX_ONLY_COMMA(int numarg) mval *dst, ...)
{
	int		zsocket_item, zsocket_type, tmpnum, numret, index, index2;
	int		numret_set;
	int4		stat, len, len2;
	mval		*arg1, *arg2, tmpmval;
	mval		*keyword;
	mval		*devicename;
	mstr		tn;			/* translated name */
	io_desc		*iod;
	io_log_name	*nl, *tl;
	char		buf1[MAX_TRANS_NAME_LEN];	/* buffer to hold translated name */
	char		*c1;		/* used to compare $P name */
	int		nlen;		/* len of $P name */
	io_log_name	*tlp;		/* logical record for translated name for $principal */
	int		nldone;		/* 0 if not $ZPIN or $ZPOUT, 1 if $ZPIN and 2 if $ZPOUT */
	int			sockopt_value;	/* for getsockopt calls */
	GTM_SOCKLEN_TYPE	sockopt_len;	/* for getsockopt calls */
	d_socket_struct		*dsocketptr;
	socket_struct		*socketptr = NULL;
#ifdef	GTM_TLS
	int			tls_options_mask, optionoffset, optionlen;
	gtm_tls_socket_t	*tls_sock;
	gtm_tls_ctx_t		*tls_ctx;
	gtm_tls_conn_info	conn_info;
	char			*charptr, *optionend;
	struct tm		*localtm;
#endif
	va_list		var;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assertpro(2 <= numarg);
	VAR_START(var, dst);
	assertpro(NULL != dst);
	devicename = va_arg(var, mval *);
	if ((NULL != devicename) && !M_ARG_SKIPPED(devicename))
		MV_FORCE_STR(devicename);
	else
		devicename = NULL;		/* use stringpool */
	keyword = va_arg(var, mval *);
	MV_FORCE_STR(keyword);
	numarg -= 3;	/* remove destination, device, and keyword from count */
	if (numarg > 0)
	{
		arg1 = va_arg(var, mval *);
		if (--numarg > 0)
		{
			arg2 = va_arg(var, mval *);
			DEBUG_ONLY(--numarg;)
		} else
			arg2 = (mval *)NULL;
	} else
	{
		arg1 = (mval *)NULL;
		arg2 = (mval *)NULL;
	}
	assert(!numarg);
	va_end(var);
	nldone = 0;
	if (NULL == devicename)
	{
		if ((NULL == socket_pool) || (NULL == socket_pool->iod))
		{
			*dst = literal_null;	/* no socketpool device yet */
			return;
		}
		iod = socket_pool->iod;
	} else if (0 == devicename->str.len)
		iod = io_curr_device.in;
	else
	{
		if ((io_std_device.in != io_std_device.out))
		{
			tlp = dollar_principal ? dollar_principal : io_root_log_name->iod->trans_name;
			nlen = tlp->len;
			assert(dollar_zpout.len == dollar_zpin.len);
			if ((nlen + dollar_zpout.len) == devicename->str.len)
			{	/* passed the length test now compare the 2 pieces, the first one the length of
				   $P and the second $ZPIN or $ZPOUT
				*/
				c1 = (char *)tlp->dollar_io;
				if (!memvcmp(c1, nlen, &(devicename->str.addr[0]), nlen))
				{
					if (!memvcmp(dollar_zpin.addr, dollar_zpin.len,
						     &(devicename->str.addr[nlen]), dollar_zpin.len))
						nldone = 1;
					else if (!memvcmp(dollar_zpout.addr, dollar_zpout.len,
							  &(devicename->str.addr[nlen]), dollar_zpout.len))
						nldone = 2;
				}
			}
		}
		if (0 == nldone)
			nl = get_log_name(&devicename->str, NO_INSERT);
		else
			nl = get_log_name(&dollar_prin_log, NO_INSERT);
		if (NULL == nl)
		{
			stat = trans_log_name(&devicename->str, &tn, buf1, SIZEOF(buf1), dont_sendmsg_on_log2long);
			if (SS_NORMAL != stat)
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_IONOTOPEN);
			else
			{
				if (0 == (tl = get_log_name(&tn, NO_INSERT)))
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_IONOTOPEN);
				nl = tl;
			}
		}
		if (!nl->iod || (dev_open != nl->iod->state))
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_IONOTOPEN);
		iod = nl->iod;
	}
	/* if iod is standard in device and it is a split device and it is $ZPOUT set iod to output device */
	if ((2 == nldone) && (io_std_device.in == iod))
		iod = io_std_device.out;
	if (gtmsocket != iod->type)
	{
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_ZSOCKETNOTSOCK);
	}
	dsocketptr = (d_socket_struct *)iod->dev_sp;
	if ((zsocket_item = namelook(zsocket_indextab, zsocket_names, keyword->str.addr, keyword->str.len)) < 0)
	{
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_ZSOCKETATTR, 2, keyword->str.len, keyword->str.addr);
		/* This line is unreachable but gcc 12.2 on AARCH64 does not realize it and issues a [-Warray-bounds]
		 * warning in the following "zsocket_types[zsocket_item]" line assuming "zsocket_item" is -1.
		 * To avoid that, just set "zsocket_item" to 0 here even though we don't expect to ever reach here.
		 */
		zsocket_item = 0;	/* avoid false [-Warray-bounds] warning in gcc 12.2 on AARCH64 */
	}
	zsocket_type = zsocket_types[zsocket_item];
	if ((level_socket == zsocket_level[zsocket_item]) && (zsocket_index != zsocket_item))
	{
		GET_SOCKETPTR_INDEX(dsocketptr, index, socketptr);
		if (NULL == socketptr)
		{	/* index out of bounds */
			*dst = literal_null;
			return;
		}
	}
	switch (zsocket_item)
	{
		case zsocket_blocking:
			assert(socketptr);
			if ((socket_connected == socketptr->state) && socketptr->nonblocked_output)
				*dst = literal_zero;
			else
				*dst = literal_one;
			break;
		case zsocket_currindex:
			numret = (int)dsocketptr->current_socket;
			break;
		case zsocket_delimiter:
			assert(socketptr);
			if (NULL == arg2)
			{	/* want how many delimiters */
				numret = socketptr->n_delimiter;
				zsocket_type = MV_NM;
			} else
			{
				index2 = mval2i(arg2);
				if ((0 > index2) || (index2 > socketptr->n_delimiter - 1))
				{	/* not in range */
					*dst = literal_null;
					zsocket_type = MV_STR;
				} else
				{	/* return UTF-8 or M */
					dst->str = socketptr->delimiter[index2];
					assert((0 <= dst->str.len) && (MAX_DELIM_LEN >= dst->str.len));
					s2pool(&dst->str);
					zsocket_type = MV_STR;
				}
			}
			break;
		case zsocket_descriptor:
			assert(socketptr);
			numret = socketptr->sd;
			break;
		case zsocket_howcreated:
			assert(socketptr);
			assert(creator_passed >= socketptr->howcreated);
			dst->str.addr = (char *)zsocket_howcreated_names[socketptr->howcreated];
			dst->str.len = STRLEN(dst->str.addr);
			UTF8_ONLY(dst->str.char_len = 0);
			assert((0 <= dst->str.len) && (MAX_ZSOCKET_NAMES >= dst->str.len));
			s2pool(&dst->str);
			break;
		case zsocket_index:
			if (M_ARG_SKIPPED(arg1))
			{
				numret = (int)dsocketptr->current_socket;
				if (0 > numret)
				{
					*dst = literal_null;
					zsocket_type = MV_STR;
				}
			} else if ((NULL == arg1) || (0 == arg1->str.len))
			{
				*dst = literal_null;
				zsocket_type = MV_STR;
			} else
			{
				MV_FORCE_STR(arg1);
				numret = iosocket_handle(arg1->str.addr, &arg1->str.len, FALSE, dsocketptr);
				if (0 > numret)
				{
					*dst = literal_null;
					zsocket_type = MV_STR;
				}
			}
			break;
		case zsocket_ioerror:
			assert(socketptr);
			if (socketptr->ioerror)
				*dst = literal_one;
			else
				*dst = literal_zero;
			break;
		case zsocket_keepalive:
			/* return [uservalue;]getsockoptvalue */
			/* return null string if error */
			assert(socketptr);
			sockopt_value = 0;
			sockopt_len = sizeof(sockopt_value);
			if (-1 != iosocket_getsockopt(socketptr, "SO_KEEPALIVE", SO_KEEPALIVE, SOL_SOCKET, &sockopt_value,
						&sockopt_len, FALSE))
			{
				RETURN_SOCKOPT(alive, keepalive);
			} else
			{
				*dst = literal_null;
				zsocket_type = MV_STR;
			}
			break;
		case zsocket_keepcnt:
			/* return [uservalue;]getsockoptvalue */
			/* return null string if error */
			assert(socketptr);
			sockopt_value = 0;
			sockopt_len = sizeof(sockopt_value);
			if (-1 != iosocket_getsockopt(socketptr, "TCP_KEEPCNT", TCP_KEEPCNT, IPPROTO_TCP, &sockopt_value,
						&sockopt_len, FALSE))
			{
				RETURN_SOCKOPT(cnt, keepcnt);
			} else
			{
				*dst = literal_null;
				zsocket_type = MV_STR;
			}
			break;
		case zsocket_keepidle:
			/* return [uservalue;]getsockoptvalue */
			/* return null string if error */
			assert(socketptr);
			sockopt_value = 0;
			sockopt_len = sizeof(sockopt_value);
			if (-1 != iosocket_getsockopt(socketptr, "TCP_KEEPIDLE", TCP_KEEPIDLE, IPPROTO_TCP, &sockopt_value,
						&sockopt_len, FALSE))
			{
				RETURN_SOCKOPT(idle, keepidle);
			} else
			{
				*dst = literal_null;
				zsocket_type = MV_STR;
			}
			break;
		case zsocket_keepintvl:
			/* return [uservalue;]getsockoptvalue */
			/* return null string if error */
			assert(socketptr);
			sockopt_value = 0;
			sockopt_len = sizeof(sockopt_value);
			if (-1 != iosocket_getsockopt(socketptr, "TCP_KEEPINTVL", TCP_KEEPINTVL, IPPROTO_TCP, &sockopt_value,
						&sockopt_len, FALSE))
			{
				RETURN_SOCKOPT(intvl, keepintvl);
			} else
			{
				*dst = literal_null;
				zsocket_type = MV_STR;
			}
			break;
		case zsocket_localaddress:
			assert(socketptr);
			if (NULL != socketptr->local.saddr_ip)
			{
				dst->str.addr = socketptr->local.saddr_ip;
				dst->str.len = STRLEN(socketptr->local.saddr_ip);
#			ifndef VMS
			} else if (socket_local == socketptr->protocol)
			{
				if (NULL != socketptr->local.sa)
					dst->str.addr = ((struct sockaddr_un *)(socketptr->local.sa))->sun_path;
				else if (NULL != socketptr->remote.sa)	/* CONNECT */
					dst->str.addr = ((struct sockaddr_un *)(socketptr->remote.sa))->sun_path;
				else
				{
					*dst = literal_null;
					break;
				}
				dst->str.len = STRLEN(dst->str.addr);
#			endif
			} else
				*dst = literal_null;
			UTF8_ONLY(dst->str.char_len = 0);
			assert((0 <= dst->str.len) && (SA_MAXLEN >= dst->str.len));
			s2pool(&dst->str);
			break;
		case zsocket_localport:
			assert(socketptr);
			if ((NULL != socketptr->local.sa) || socketptr->passive)
				numret = (int)socketptr->local.port;
			else
			{
				*dst = literal_null;
				zsocket_type = MV_STR;
			}
			break;
		case zsocket_morereadtime:
			assert(socketptr);
			if (socketptr->def_moreread_timeout)	/* user specified */
				numret = (int)socketptr->moreread_timeout;
			else
			{
				*dst = literal_null;
				zsocket_type = MV_STR;
			}
			break;
		case zsocket_number:
			numret = (int)dsocketptr->n_socket;
			break;
<<<<<<< HEAD
		case zsocket_options:;
			int determined_length = 0;
			for (zsocket_item_round_t round = ZSIR_1_DETERMINE_LENGTH; round <= ZSIR_2_RENDER_RESULT; round++)
=======
		case zsocket_options:
			/* build string from socket struct - note this may not be exactly what was specified */
			assert(socketptr);
			len = 0;
			if (SOCKOPTIONS_SYSTEM < socketptr->options_state.alive)
			{	/* user specified */
				len += STRLEN(KEEPALIVE_STR);
				if (0 == socketptr->keepalive)
					len++;		/* disabled */
				else if (SOCKOPTIONS_SYSTEM < socketptr->options_state.idle)
				{	/* if keepalive disabled skip keepidle */
					len += STRLEN(KEEPIDLE_STR) + 1;	/* count = */
					len += MAX_DIGITS_IN_INT;
				}
			}
			if (SOCKOPTIONS_SYSTEM < socketptr->options_state.sndbuf)
>>>>>>> 3c1c09f2 (GT.M V7.1-001)
			{
				bool need_optionend_flag = false;

				if (ZSIR_2_RENDER_RESULT == round)
				{
					ENSURE_STP_FREE_SPACE(determined_length);
					charptr = (char *)stringpool.free;
				}

				zsocket_options_item(
					round,
					&charptr,
					&determined_length,
					&need_optionend_flag,
					KEEPALIVE_STR,
					socketptr->keepalive,
					socketptr->options_state.alive);

				if (socketptr->keepalive > 0)
				{
					zsocket_options_item(
						round,
						&charptr,
						&determined_length,
						&need_optionend_flag,
						KEEPIDLE_STR,
						socketptr->keepidle,
						socketptr->options_state.idle);
				}

				zsocket_options_item(
					round,
					&charptr,
					&determined_length,
					&need_optionend_flag,
					KEEPCNT_STR,
					socketptr->keepcnt,
					socketptr->options_state.cnt);

				zsocket_options_item(
					round,
					&charptr,
					&determined_length,
					&need_optionend_flag,
					KEEPINTVL_STR,
					socketptr->keepintvl,
					socketptr->options_state.intvl);

				zsocket_options_item(
					round,
					&charptr,
					&determined_length,
					&need_optionend_flag,
					SNDBUF_STR,
					socketptr->iobfsize,
					socketptr->options_state.sndbuf);
			} // for round
			len = charptr - (char *)stringpool.free;
			dst->str.addr = (char *)stringpool.free;
			dst->str.len = len;
			UTF8_ONLY(dst->str.char_len = 0);
			stringpool.free += len;
			break;
		case zsocket_parent:
			assert(socketptr);
			if (NULL != socketptr->parenthandle)
			{
				dst->str.addr = socketptr->parenthandle;
				dst->str.len = STRLEN(socketptr->parenthandle);
				UTF8_ONLY(dst->str.char_len = 0);
				/* The source buffer is socket_struct->handle[MAX_HANDLE_LEN] */
				assert((0 <= dst->str.len) && (MAX_HANDLE_LEN >= dst->str.len));
				s2pool(&dst->str);
			} else
				*dst = literal_null;
			break;
		case zsocket_protocol:
			assert(socketptr);
#			ifndef VMS
			if (socket_local == socketptr->protocol)
				*dst = literal_local;
			else
#			endif
			{
				if (NULL != socketptr->remote.sa)
					tmpnum= socketptr->remote.ai.ai_family;
				else if (NULL != socketptr->local.sa)
					tmpnum= socketptr->local.ai.ai_family;
				else
					tmpnum = AF_UNSPEC;
				switch (tmpnum)
				{
					case AF_INET:
						*dst = literal_tcp;
						break;
					case AF_INET6:
						*dst = literal_tcp6;
						break;
					default:
						*dst = literal_null;
				}
			}
			break;
		case zsocket_remoteaddress:
			assert(socketptr);
			if (NULL != socketptr->remote.saddr_ip)
			{
				dst->str.addr = socketptr->remote.saddr_ip;
				dst->str.len = STRLEN(socketptr->remote.saddr_ip);
			} else if (socket_local == socketptr->protocol)
			{
				if (NULL != socketptr->remote.sa)
					dst->str.addr = ((struct sockaddr_un *)(socketptr->remote.sa))->sun_path;
				else if (NULL != socketptr->remote.sa)	/* CONNECT */
					dst->str.addr = ((struct sockaddr_un *)(socketptr->local.sa))->sun_path;
				else
				{
					*dst = literal_null;
					break;
				}
				dst->str.len = STRLEN(dst->str.addr);
			} else
				*dst = literal_null;
			UTF8_ONLY(dst->str.char_len = 0);
			assert((0 <= dst->str.len) && (SA_MAXLEN >= dst->str.len));
			s2pool(&dst->str);
			break;
		case zsocket_remoteport:
			assert(socketptr);
			if (NULL != socketptr->remote.sa)
				numret = (int)socketptr->remote.port;
			else
			{
				*dst = literal_null;
				zsocket_type = MV_STR;
			}
			break;
		case zsocket_sndbuf:
			/* return [uservalue;]getsockoptvalue */
			/* return null string if error */
			assert(socketptr);
			sockopt_value = 0;
			sockopt_len = sizeof(sockopt_value);
			if (-1 != iosocket_getsockopt(socketptr, "SO_SNDBUF", SO_SNDBUF, SOL_SOCKET, &sockopt_value,
						&sockopt_len, FALSE))
			{
				RETURN_SOCKOPT(sndbuf, iobfsize);
			} else
			{
				*dst = literal_null;
				zsocket_type = MV_STR;
			}
			break;
		case zsocket_sockethandle:
			assert(socketptr);
			dst->str.addr = socketptr->handle;
			dst->str.len = socketptr->handle_len;
			UTF8_ONLY(dst->str.char_len = 0);
			assert((0 <= dst->str.len) && (MAX_HANDLE_LEN >= dst->str.len));
			s2pool(&dst->str);
			break;
		case zsocket_state:
			assert(socketptr);
			assert(socket_connect_inprogress >= socketptr->state);
			dst->str.addr = (char *)zsocket_state_names[socketptr->state];
			dst->str.len = STRLEN(dst->str.addr);
			UTF8_ONLY(dst->str.char_len = 0);
			assert((0 <= dst->str.len) && (MAX_ZSOCKET_NAMES >= dst->str.len));
			s2pool(&dst->str);
			break;
		case zsocket_tls:
#			ifdef	GTM_TLS
			assert(socketptr);
			if (socketptr->tlsenabled)
			{
				tls_options_mask = 0;
				tls_sock = (gtm_tls_socket_t *)socketptr->tlssocket;
				if (NULL == tls_sock)
				{
					*dst = literal_null;	/* something is wrong */
					break;
				}
				len = SIZEOF(ONE_COMMA) - 1 + SIZEOF(TLSCLIENTSTR) - 1 + 1; /* remove nulls, add trailing comma */
				len += STRLEN(tls_sock->tlsid);		/* trailing comma above not needed if no tlsid but OK */
				tls_options_mask = 0;
				if ((NULL != arg2) && (0 < arg2->str.len))
				{
					len2 = MIN((MAX_TRANS_NAME_LEN - 1), arg2->str.len);
					lower_to_upper((uchar_ptr_t)buf1, (uchar_ptr_t)arg2->str.addr, len2);
					buf1[len2] = '\0';
					for (charptr = buf1; (&buf1[len2] > charptr); charptr = optionend)
					{
						if (buf1 < charptr)
							if ('\0' == *++charptr)
								break;
						optionend = strstr((const char *)charptr, OPTIONENDSTR);
						if (NULL == optionend)
							optionend = charptr + STRLEN(charptr);
						*optionend = '\0';
						for (index2 = 0; NULL != zsocket_tls_options[index2]; index2++)
							if (0 == STRCMP(charptr, zsocket_tls_options[index2]))
							{
								if (TLS_OPTIONS_ALL_INDEX == index2)
									tls_options_mask |= TLS_OPTIONS_ALL_MASK;
								else
									tls_options_mask |= 1 << index2;
								break;
							}
						if (NULL == zsocket_tls_options[index2])
						{	/* not found */
							len2 = SIZEOF(TLSOPTIONLIT) - 1;
							optionoffset = charptr - buf1;
							optionlen = MIN((MAX_TRANS_NAME_LEN - 1 - len2), (optionend - charptr));
							charptr = arg2->str.addr;
							memcpy(buf1, TLSOPTIONLIT, len2);
							memcpy(&buf1[len2], &charptr[optionoffset], optionlen);
							buf1[len2 + optionlen] = '\0';
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZSOCKETATTR, 2,
								(len2 + optionlen), buf1);
							return;		/* make compiler happy */
						}
					}
					memset(&conn_info, 0, SIZEOF(conn_info));
					if (0 == gtm_tls_get_conn_info(tls_sock, &conn_info))
					{
						if (TLS_OPTIONS_CIPHER & tls_options_mask)
						{
							len += STRLEN(conn_info.protocol) + OPTIONPREFIXLEN;	/* |P: */
							len += STRLEN(conn_info.session_algo) + OPTIONPREFIXLEN;	/* |C: */
						}
						if (TLS_OPTIONS_OPTIONS & tls_options_mask)
						{
							len += (OPTIONPREFIXLEN + MAX_HEX_DIGITS_IN_INT8);	/* |O: long */
							len += 3;		/* ,xx for verify_mode */
						}
						if (TLS_OPTIONS_SESSION & tls_options_mask)
						{
							len += OPTIONPREFIXLEN;	/* |S: */
							len += 7 + 1 + 1;	/* RENSEC: 0 or 1 - is secure comma */
							len += 7 + MAX_DIGITS_IN_INT + 1;	/* RENTOT: value comma */
							len += 7 + MAX_SESSION_ID_LEN + 1;	/* SESSID: hex session_id comma */
							len += 7 + CTIME_BEFORE_NL + 2;		/* SESEXP: room for NL + null */
						}
					} else
						len2 = 0;	/* no conn info available - ignore errors here */
					if (TLS_OPTIONS_INTERNAL & tls_options_mask)
					{	/* |I: ctxflags,sockflags */
						len += (OPTIONPREFIXLEN + (2 * MAX_HEX_DIGITS_IN_INT) + 1);
					}
				} else
				{
					len2 = 0;	/* flag no extras */
				}
				ENSURE_STP_FREE_SPACE(len);
				charptr = (char *)stringpool.free;
				len = SIZEOF(ONE_COMMA) - 1;
				memcpy(charptr, ONE_COMMA, len);
				charptr += len;
				len = SIZEOF(TLSCLIENTSTR) - 1;
				if (GTMTLS_OP_CLIENT_MODE & tls_sock->flags)
					MEMCPY_LIT(charptr, TLSCLIENTSTR);
				else
					MEMCPY_LIT(charptr, TLSSERVERSTR);
				charptr += len;
				len = STRLEN(tls_sock->tlsid);
				if (0 < len)
				{
					*charptr++ = ',';
					memcpy(charptr, tls_sock->tlsid, len);
					charptr += len;
				}
				if (0 < len2)
				{
					if (TLS_OPTIONS_CIPHER & tls_options_mask)
					{
						STRCPY(charptr, "|P:");
						charptr += OPTIONPREFIXLEN;
						len2 = STRLEN(conn_info.protocol);
						memcpy(charptr, conn_info.protocol, len2);
						charptr += len2;
						STRCPY(charptr, "|C:");
						charptr += OPTIONPREFIXLEN;
						len2 = STRLEN(conn_info.session_algo);
						memcpy(charptr, conn_info.session_algo, len2);
						charptr += len2;
					}
					if (TLS_OPTIONS_OPTIONS & tls_options_mask)
					{
						STRCPY(charptr, "|O:");
						charptr += OPTIONPREFIXLEN;
						i2hexl((qw_num)conn_info.options, (uchar_ptr_t)charptr, MAX_HEX_DIGITS_IN_INT8);
						charptr += MAX_HEX_DIGITS_IN_INT8;
						*charptr++ = ',';
						i2hex(conn_info.verify_mode, (uchar_ptr_t)charptr, 2);
						charptr += 2;
					}
					if (TLS_OPTIONS_SESSION & tls_options_mask)
					{
						STRCPY(charptr, "|S:");
						charptr += OPTIONPREFIXLEN;
						STRCPY(charptr, "RENSEC:");
						charptr[7] = conn_info.secure_renegotiation ? '1' : '0';
						charptr[8] = ',';
						charptr += 9;
						STRCPY(charptr, "RENTOT:");
						charptr += 7;
						charptr = (char *)i2asc((uchar_ptr_t)charptr, conn_info.total_renegotiations);
						len2 = STRLEN(conn_info.session_id);
						if (0 < len2)
						{
							*charptr++ = ',';
							STRCPY(charptr, "SESSID:");
							charptr += 7;
							memcpy(charptr, conn_info.session_id, len2);
							charptr += len2;
						}
						if (-1 != conn_info.session_expiry_timeout)
						{
							*charptr++ = ',';
							STRCPY(charptr, "SESEXP:");
							charptr += 7;
							GTM_LOCALTIME(localtm, (time_t *)&conn_info.session_expiry_timeout);
							STRFTIME(charptr, CTIME_BEFORE_NL + 2, CTIME_STRFMT, localtm, len2);
							assert(CTIME_BEFORE_NL == (len2 - 1));
							charptr += (len2 - 1);		/* ignore NL */
						}
					}
				}
				if (TLS_OPTIONS_INTERNAL & tls_options_mask)
				{
					tls_ctx = tls_sock->gtm_ctx;
					STRCPY(charptr, "|I:");
					charptr += OPTIONPREFIXLEN;
					i2hex(tls_ctx->flags, (uchar_ptr_t)charptr, MAX_HEX_DIGITS_IN_INT);
					charptr += MAX_HEX_DIGITS_IN_INT;
					*charptr++ = ',';
					i2hex(tls_sock->flags, (uchar_ptr_t)charptr, MAX_HEX_DIGITS_IN_INT);
					charptr += MAX_HEX_DIGITS_IN_INT;
				}
				len = charptr - (char *)stringpool.free;
				dst->str.addr = (char *)stringpool.free;
				dst->str.len = len;
				stringpool.free += len;
			} else
#			endif
				*dst = literal_null;
			break;
		case zsocket_zbfsize:
			assert(socketptr);
			numret = socketptr->buffer_size;
			break;
		case zsocket_zdelay:
			assert(socketptr);
			if (socketptr->nodelay)
				*dst = literal_zero;
			else
				*dst = literal_one;
			break;
		case zsocket_zff:
			assert(socketptr);
			if (0 < socketptr->zff.len)
			{
				dst->str = socketptr->zff;
				assert((0 <= dst->str.len) && (MAX_ZFF_LEN >= dst->str.len));
				s2pool(&dst->str);
			} else
				*dst = literal_null;
			break;
		case zsocket_zibfsize:
			assert(socketptr);
			sockopt_value = 0;
			sockopt_len = sizeof(sockopt_value);
			if (-1 != iosocket_getsockopt(socketptr, "SO_RCVBUF", SO_RCVBUF, SOL_SOCKET, &sockopt_value,
						&sockopt_len, FALSE))
			{
				RETURN_SOCKOPT(rcvbuf, bufsiz);
			} else
				numret = socketptr->bufsiz;
			break;
		default:
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_ZSOCKETATTR, 2, keyword->str.len, keyword->str.addr);
			GTM_UNREACHABLE();
	}
	dst->mvtype = zsocket_type;
	if (MV_NM == dst->mvtype)
		MV_FORCE_MVAL(dst, numret);
	return;
}

static void zsocket_options_item(
	zsocket_item_round_t round,
	char** charptr_ptr,
	int* determined_length_ptr,
	bool* need_optionend_flag_ptr,
	const char* caption_string,
	int value,
	int state)
{
	// if not user defined option, don't render
	if (SOCKOPTIONS_SYSTEM >= state)
		return;

	if (ZSIR_1_DETERMINE_LENGTH == round)
	{
		if (*need_optionend_flag_ptr)
			*determined_length_ptr += LENGTH_OF_OPTIONEND;
		*determined_length_ptr += STRLEN(caption_string);
		*determined_length_ptr += LENGTH_OF_OPTIONVALUE;
		*determined_length_ptr += MAX_DIGITS_IN_INT;
	}

	if (ZSIR_2_RENDER_RESULT == round)
	{
		if (*need_optionend_flag_ptr)
		{
			**charptr_ptr = OPTIONEND;
			*charptr_ptr += LENGTH_OF_OPTIONEND;
		}
		strcpy(*charptr_ptr, caption_string);
		*charptr_ptr += strlen(caption_string);
		**charptr_ptr = OPTIONVALUE;
		*charptr_ptr += LENGTH_OF_OPTIONVALUE;
		*charptr_ptr = (char *)i2asc((uchar_ptr_t)*charptr_ptr, value);
	}

	*need_optionend_flag_ptr = true;
}
