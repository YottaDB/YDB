/****************************************************************
 *								*
 *	Copyright 2014 Fidelity Information Services, Inc	*
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
#include "mvalconv.h"
#include "trans_log_name.h"
#include "zsocket.h"

GBLREF spdesc		stringpool;
GBLREF io_pair		io_curr_device;
GBLREF io_log_name	*io_root_log_name;
GBLREF d_socket_struct	*socket_pool;

error_def(ERR_ZSOCKETATTR);
error_def(ERR_ZSOCKETNOTSOCK);
error_def(ERR_IONOTOPEN);

LITREF	mval		literal_zero;
LITREF	mval		literal_one;
LITREF	mval		literal_null;
LITREF	mval		skiparg;

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
	10, 10, 12, 12, 14, 16, 16, 16, 16, 16, 16, 16, 20
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
	int4		stat;
	mval		*arg1, *arg2, tmpmval;
	mval		*keyword;
	mval		*devicename;
	mstr		tn;			/* translated name */
	io_desc		*iod;
	io_log_name	*nl, *tl;
	char		buf1[MAX_TRANS_NAME_LEN];	/* buffer to hold translated name */
	d_socket_struct	*dsocketptr;
	socket_struct	*socketptr;
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
	{	/* get information from provided device name */
		nl = get_log_name(&devicename->str, NO_INSERT);
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
}
