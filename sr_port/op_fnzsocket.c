/****************************************************************
 *								*
 * Copyright (c) 2014-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
GBLREF io_pair		*io_std_device;
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
#define	OPTIONEND		','
#define	OPTIONENDSTR		","
#define OPTIONPREFIXLEN		3		/* vertbar CPOS colon */
#endif

#define ZSOCKETITEM(A,B,C,D) {(SIZEOF(A) - 1), A}
const nametabent zsocket_names[] =
{
#include "zsockettab.h"		/* BYPASSOK */
};
#undef ZSOCKETITEM
const unsigned char zsocket_indextab[] =
{ /*	A  B  C  D  E  F  G  H  I  J  K  L  M  N */
	0, 0, 0, 1, 3, 3, 3, 3, 4, 6, 6, 6, 8, 9,
  /*	O   P   Q   R   S   T   U   V   W   X   Y   Z  end */
	10, 10, 12, 12, 14, 16, 17, 17, 17, 17, 17, 17, 21
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
LITDEF mval literal_local = DEFINE_MVAL_LITERAL(MV_STR | MV_NUM_APPROX, 0, 0, (SIZEOF("LOCAL") - 1), "LOCAL", 0, 0);
LITDEF mval literal_tcp = DEFINE_MVAL_LITERAL(MV_STR | MV_NUM_APPROX, 0, 0, (SIZEOF("TCP") - 1), "TCP", 0, 0);
LITDEF mval literal_tcp6 = DEFINE_MVAL_LITERAL(MV_STR | MV_NUM_APPROX, 0, 0, (SIZEOF("TCP6") - 1), "TCP6", 0, 0);
LITDEF char *zsocket_state_names[] = {"CONNECTED", "LISTENING", "BOUND", "CREATED", "CONNECTINPROGRESS"};
LITDEF char *zsocket_howcreated_names[] = {"LISTEN", "ACCEPTED", "CONNECT", "PRINCIPAL", "PASSED"};

#define GET_SOCKETPTR_INDEX(DSOCK, INDEX, SOCKETPTR)			\
{									\
	INDEX = (NULL != arg1) ? (!M_ARG_SKIPPED(arg1) ? mval2i(arg1) : DSOCK->current_socket) : (DSOCK->n_socket + 1);	\
	if ((0 < DSOCK->n_socket) && (INDEX <= DSOCK->n_socket))	\
		SOCKETPTR = DSOCK->socket[INDEX];			\
	else								\
		SOCKETPTR = NULL;					\
}

void	op_fnzsocket(UNIX_ONLY_COMMA(int numarg) mval *dst, ...)
{
	VMS_ONLY(int	numarg;)
	int		zsocket_item, zsocket_type, tmpnum, numret, index, index2;
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
	d_socket_struct		*dsocketptr;
	socket_struct		*socketptr;
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
	VMS_ONLY(va_count(numarg));
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
	}
	else if (0 == devicename->str.len)
		iod = io_curr_device.in;
	else
	{
		if ((io_std_device->in != io_std_device->out))
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
			stat = TRANS_LOG_NAME(&devicename->str, &tn, buf1, SIZEOF(buf1), dont_sendmsg_on_log2long);
			if (SS_NORMAL != stat)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_IONOTOPEN);
			else
			{
				if (0 == (tl = get_log_name(&tn, NO_INSERT)))
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_IONOTOPEN);
				nl = tl;
			}
		}
		if (!nl->iod || (dev_open != nl->iod->state))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_IONOTOPEN);
		iod = nl->iod;
	}
	/* if iod is standard in device and it is a split device and it is $ZPOUT set iod to output device */
	if ((2 == nldone) && (io_std_device->in == iod))
		iod = io_std_device->out;
	if (gtmsocket != iod->type)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZSOCKETNOTSOCK);
	}
	dsocketptr = (d_socket_struct *)iod->dev_sp;
	if ((zsocket_item = namelook(zsocket_indextab, zsocket_names, keyword->str.addr, keyword->str.len)) < 0)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZSOCKETATTR, 2, keyword->str.len, keyword->str.addr);
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
		case zsocket_currindex:
			numret = (int)dsocketptr->current_socket;
			break;
		case zsocket_delimiter:
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
					s2pool(&dst->str);
					zsocket_type = MV_STR;
				}
			}
			break;
		case zsocket_descriptor:
			numret = socketptr->sd;
			break;
		case zsocket_howcreated:
			assert(creator_passed >= socketptr->howcreated);
			dst->str.addr = (char *)zsocket_howcreated_names[socketptr->howcreated];
			dst->str.len = STRLEN(dst->str.addr);
			UNICODE_ONLY(dst->str.char_len = 0);
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
			if (socketptr->ioerror)
				*dst = literal_one;
			else
				*dst = literal_zero;
			break;
		case zsocket_localaddress:
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
			UNICODE_ONLY(dst->str.char_len = 0);
			s2pool(&dst->str);
			break;
		case zsocket_localport:
			if ((NULL != socketptr->local.sa) || socketptr->passive)
				numret = (int)socketptr->local.port;
			else
			{
				*dst = literal_null;
				zsocket_type = MV_STR;
			}
			break;
		case zsocket_morereadtime:
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
		case zsocket_parent:
			if (NULL != socketptr->parenthandle)
			{
				dst->str.addr = socketptr->parenthandle;
				dst->str.len = STRLEN(socketptr->parenthandle);
				UNICODE_ONLY(dst->str.char_len = 0);
				s2pool(&dst->str);
			} else
				*dst = literal_null;
			break;
		case zsocket_protocol:
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
			if (NULL != socketptr->remote.saddr_ip)
			{
				dst->str.addr = socketptr->remote.saddr_ip;
				dst->str.len = STRLEN(socketptr->remote.saddr_ip);
#			ifndef VMS
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
#			endif
			} else
				*dst = literal_null;
			UNICODE_ONLY(dst->str.char_len = 0);
			s2pool(&dst->str);
			break;
		case zsocket_remoteport:
			if (NULL != socketptr->remote.sa)
				numret = (int)socketptr->remote.port;
			else
			{
				*dst = literal_null;
				zsocket_type = MV_STR;
			}
			break;
		case zsocket_sockethandle:
			dst->str.addr = socketptr->handle;
			dst->str.len = socketptr->handle_len;
			UNICODE_ONLY(dst->str.char_len = 0);
			s2pool(&dst->str);
			break;
		case zsocket_state:
			assert(socket_connect_inprogress >= socketptr->state);
			dst->str.addr = (char *)zsocket_state_names[socketptr->state];
			dst->str.len = STRLEN(dst->str.addr);
			UNICODE_ONLY(dst->str.char_len = 0);
			s2pool(&dst->str);
			break;
		case zsocket_tls:
#			ifdef	GTM_TLS
			if (socketptr->tlsenabled)
			{
				tls_sock = (gtm_tls_socket_t *)socketptr->tlssocket;
				if (NULL == tls_sock)
				{
					*dst = literal_null;	/* something is wrong */
					break;
				}
				len = SIZEOF(ONE_COMMA) - 1 + SIZEOF(TLSCLIENTSTR) - 1 + 1; /* remove nulls, add trailing comma */
				len += STRLEN(tls_sock->tlsid);		/* trailing comma above not needed if no tlsid but OK */
				if ((NULL != arg2) && (0 < arg2->str.len))
				{
					len2 = MIN((MAX_TRANS_NAME_LEN - 1), arg2->str.len);
					lower_to_upper((uchar_ptr_t)buf1, (uchar_ptr_t)arg2->str.addr, len2);
					buf1[len2] = '\0';
					tls_options_mask = 0;
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
					len2 = 0;	/* flag no extras */
				ENSURE_STP_FREE_SPACE(len);
				charptr = (char *)stringpool.free;
				len = SIZEOF(ONE_COMMA) - 1;
				memcpy(charptr, ONE_COMMA, len);
				charptr += len;
				len = SIZEOF(TLSCLIENTSTR) - 1;
				STRNCPY_STR(charptr, (GTMTLS_OP_CLIENT_MODE & tls_sock->flags) ? TLSCLIENTSTR : TLSSERVERSTR, len);
				charptr += len;
				len = STRLEN(tls_sock->tlsid);
				if (0 < len)
				{
					*charptr++ = ',';
					STRNCPY_STR(charptr, tls_sock->tlsid, len);
					charptr += len;
				}
				if (0 < len2)
				{
					if (TLS_OPTIONS_CIPHER & tls_options_mask)
					{
						STRCPY(charptr, "|P:");
						charptr += OPTIONPREFIXLEN;
						len2 = STRLEN(conn_info.protocol);
						STRNCPY_STR(charptr, conn_info.protocol, len2);
						charptr += len2;
						STRCPY(charptr, "|C:");
						charptr += OPTIONPREFIXLEN;
						len2 = STRLEN(conn_info.session_algo);
						STRNCPY_STR(charptr, conn_info.session_algo, len2);
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
							STRNCPY_STR(charptr, conn_info.session_id, len2);
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
			numret = socketptr->buffer_size;
			break;
		case zsocket_zdelay:
			if (socketptr->nodelay)
				*dst = literal_zero;
			else
				*dst = literal_one;
			break;
		case zsocket_zff:
			if (0 < socketptr->zff.len)
			{
				dst->str = socketptr->zff;
				s2pool(&dst->str);
			} else
				*dst = literal_null;
			break;
		case zsocket_zibfsize:
			numret = socketptr->bufsiz;
			break;
		default:
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZSOCKETATTR, 2, keyword->str.len, keyword->str.addr);
	}
	dst->mvtype = zsocket_type;
	if (MV_NM == dst->mvtype)
		MV_FORCE_MVAL(dst, numret);
	return;
}
