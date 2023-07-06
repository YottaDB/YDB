/****************************************************************
 *								*
 * Copyright (c) 2022-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include "gtm_string.h"

#include "error.h"
#include "nametabtyp.h"
#include "namelook.h"
#include "io.h"
#include "iosp.h"
#include "io_params.h"
#include "gtm_netdb.h"
#include "gtm_socket.h"
#include "gtm_un.h"
#include "gtm_inet.h"
#include "gtm_ipv6.h"
#include "iosocketdef.h"
#include "op.h"
#include "mmemory.h"
#include "mvalconv.h"
#include "gtm_caseconv.h"
#include "min_max.h"
#include "gtm_time.h"
#include "gtm_stdlib.h"

error_def(ERR_DEVICEOPTION);

#define	OPTIONEND		','
#define	OPTIONENDSTR		","
#define OPTIONVALUE		'='
#define OPTIONVALUESTR		"="

#define DEVOPTIONITEM(A,B,C,D,E,F)	{(SIZEOF(A) - 1), A}
const nametabent devoption_names[] =
{
#include "devoptionstab.h"         /* BYPASSOK */
};
#undef DEVOPTIONITEM
const unsigned char devoption_indextab[] =
{ /*    A  B  C  D  E  F  G  H  I  J  K  L  M  N */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 4,
  /*    O  P  Q  R  S  T  U  V  W  X  Y  Z  end */
        4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5
};
#define DEVOPTIONITEM(A,B,C,D,E,F) B
enum devoption_code {		/* case labels */
#include "devoptionstab.h"         /* BYPASSOK */
};
#undef DEVOPTIONITEM
#define DEVOPTIONITEM(A,B,C,D,E,F) C
static const enum io_dev_type devoption_device[] =
{
#include "devoptionstab.h"         /* BYPASSOK */
};
#undef DEVOPTIONITEM
#define DEVOPTIONITEM(A,B,C,D,E,F) D
static const int devoption_command[] =
{
#include "devoptionstab.h"         /* BYPASSOK */
};
#undef DEVOPTIONITEM
#define DEVOPTIONITEM(A,B,C,D,E,F) E
static const boolean_t devoption_defer[] =
{
#include "devoptionstab.h"         /* BYPASSOK */
};
#undef DEVOPTIONITEM
#define DEVOPTIONITEM(A,B,C,D,E,F) F
static const int devoption_argtype[] =	/* IOP_SRC_ */
{
#include "devoptionstab.h"         /* BYPASSOK */
};
#undef DEVOPTIONITEM


#define ONEVALUEALLOWED	"only one value allowed"
#define UNKNOWNOPTION	"unrecognized option"
#define INVALIDNUMBER	"invalid number"
#define VALUEREQUIRED	"value required"


#define GET_OPTION_VALUE(OPTION, OPTIONLEN, RET, STATUS)		\
MBSTART { /* OPTIONLEN not used since strtol stops on non digit */	\
	STATUS = errno = 0;						\
	RET = STRTOL(OPTION, NULL, 0);					\
	if (0 != errno)							\
	{								\
		STATUS = errno;						\
	}								\
} MBEND

/*	Parse OPTIONS device parameter arguments 		*
 *	Note: while current options are all for socket devices,	*
 *		options for other device types may be added in	*
 *		the future.					*
 *	iod		future: device descriptor if not socket	*
 *	socketptr	socket descriptor or NULL if not socket	*
 *	optionstr	OPTIONS device parameter		*
 *	caller		where called from for error messages	*
 *	command		future: IOP_command_OK from io_params.h	*
 */
void	devoptions(io_desc *iod, void *socketptrarg, mstr *optionstr, char *caller, int command)
{
	int			devopt_item, tmpnum, numret, index, index2, keywordend;
	int4			stat, len, len2;
	mstr			keyword, options;
	boolean_t		valuepresent;
	int			optionvalue, valuelen, valuestart = -1, local_errno;
	char			*errortext;
	io_desc			*socket_iod;
	d_socket_struct		*dsocketptr;
	socket_struct		*socketptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* future options may be for non socket devices */
	if (socketptrarg)
	{
		socketptr = (socket_struct *)socketptrarg;
		dsocketptr = socketptr->dev;
		socket_iod = dsocketptr->iod;
	} else
	{
		if (NULL == iod)
		{	/* nothing to do */
			assert(socketptrarg && (NULL != iod));
			return;
		}
		socketptr = NULL;
		dsocketptr = NULL;
		socket_iod = NULL;
	}
	options = *optionstr;	/* option[=value],... */
	for (index = 0; 0 < options.len ; options.len -= (index + 1), options.addr += (index + 1))
	{	/* comma separated options - need quotes if ever allow string values */
		valuepresent = FALSE;
		keyword = options;
		for (index = 0; (index < options.len) && (OPTIONEND != options.addr[index]) ; index++)
		{
			if (OPTIONVALUE == keyword.addr[index])
			{
				if (valuepresent)
				{	/* only one value per option */
					assert(0 <= valuestart);
					keywordend = valuestart - 1;	/* remove = */
					errortext = ONEVALUEALLOWED;
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_DEVICEOPTION, 4,
							keywordend, keyword.addr, caller, errortext);
				}
				valuepresent = TRUE;
				valuestart = index + 1;
				keyword.len = index;	/* continue scan for OPTIONEND */
			}
		}
		if (valuepresent)
		{
			assert(0 <= valuestart);
			valuelen = index - valuestart;
		} else
			keyword.len = index;
		if ((devopt_item = namelook(devoption_indextab, devoption_names, keyword.addr, keyword.len)) < 0)
		{
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_DEVICEOPTION, 4, keyword.len, keyword.addr, caller, UNKNOWNOPTION);
		}
		assert(!valuepresent || (0 <= valuestart));
		switch (devopt_item)
		{
			case devopt_keepalive:
				if (!socketptr)
					break;		/* ignore if not socket */
				if (!valuepresent)
				{
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_DEVICEOPTION,
						4, keyword.len, keyword.addr, caller, VALUEREQUIRED);
				}
				GET_OPTION_VALUE(&keyword.addr[valuestart], valuelen, optionvalue, local_errno);
				if (0 != local_errno)
				{
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_DEVICEOPTION,
						 4, keyword.len, keyword.addr, caller, INVALIDNUMBER);
				}
				socketptr->options_state.alive = SOCKOPTIONS_USER | SOCKOPTIONS_PENDING;
				socketptr->keepalive = optionvalue;
				break;
			case devopt_keepcnt:
				if (!socketptr)
					break;		/* ignore if not socket */
				if (!valuepresent)
				{
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_DEVICEOPTION,
						4, keyword.len, keyword.addr, caller, VALUEREQUIRED);
				}
				GET_OPTION_VALUE(&keyword.addr[valuestart], valuelen, optionvalue, local_errno);
				if (0 != local_errno)
				{
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_DEVICEOPTION,
						4, keyword.len, keyword.addr, caller, INVALIDNUMBER);
				}
				socketptr->options_state.cnt = SOCKOPTIONS_USER | SOCKOPTIONS_PENDING;
				socketptr->keepcnt = optionvalue;
				break;
			case devopt_keepidle:
				if (!socketptr)
					break;		/* ignore if not socket */
				if (!valuepresent)
				{
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_DEVICEOPTION,
						4, keyword.len, keyword.addr, caller, VALUEREQUIRED);
				}
				GET_OPTION_VALUE(&keyword.addr[valuestart], valuelen, optionvalue, local_errno);
				if (0 != local_errno)
				{
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_DEVICEOPTION,
						4, keyword.len, keyword.addr, caller, INVALIDNUMBER);
				}
				socketptr->options_state.idle = SOCKOPTIONS_USER | SOCKOPTIONS_PENDING;
				socketptr->keepidle = optionvalue;
				break;
			case devopt_keepintvl:
				if (!socketptr)
					break;		/* ignore if not socket */
				if (!valuepresent)
				{
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_DEVICEOPTION,
						4, keyword.len, keyword.addr, caller, VALUEREQUIRED);
				}
				GET_OPTION_VALUE(&keyword.addr[valuestart], valuelen, optionvalue, local_errno);
				if (0 != local_errno)
				{
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_DEVICEOPTION,
						4, keyword.len, keyword.addr, caller, INVALIDNUMBER);
				}
				socketptr->options_state.intvl = SOCKOPTIONS_USER | SOCKOPTIONS_PENDING;
				socketptr->keepintvl = optionvalue;
				break;
			case devopt_sndbuf:
				if (!socketptr)
					break;		/* ignore if not socket */
				if (!valuepresent)
				{
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_DEVICEOPTION,
						4, keyword.len, keyword.addr, caller, VALUEREQUIRED);
				}
				GET_OPTION_VALUE(&keyword.addr[valuestart], valuelen, optionvalue, local_errno);
				if (0 != local_errno)
				{
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_DEVICEOPTION,
						4, keyword.len, keyword.addr, caller, INVALIDNUMBER);
				}
				socketptr->options_state.sndbuf = SOCKOPTIONS_USER | SOCKOPTIONS_PENDING;
				socketptr->iobfsize = optionvalue;
				break;
			default:
				assert(TRUE || devopt_item);
		}
	}
	return;
}
