/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* gtm_netdb.h - interlude to <netdb.h> system header file.  */
#ifndef GTM_NETDBH
#define GTM_NETDBH

#include <netdb.h>

#define MAX_GETHOST_TRIES	8

/* Macro to issue an rts_error_csa() with the results of getaddrinfo() or getnameinfo().
 * Takes ERR_GETADDRINFO or ERR_GETNAMEINFO as the mnemonic.
 */
#define RTS_ERROR_ADDRINFO(CSA, MNEMONIC, ERRCODE)						\
{												\
	if (EAI_SYSTEM == ERRCODE)								\
		rts_error_csa(CSA_ARG(CSA) VARLSTCNT(4) MNEMONIC, 0, errno, 0);			\
	else											\
		rts_error_csa(CSA_ARG(CSA) VARLSTCNT(6) MNEMONIC, 0,				\
				ERR_TEXT, 2, RTS_ERROR_STRING(gai_strerror(ERRCODE)));		\
}
/* Same as above, but with a literal string context description. */
#define RTS_ERROR_ADDRINFO_CTX(CSA, MNEMONIC, ERRCODE, CONTEXT)					\
{												\
	if (EAI_SYSTEM == ERRCODE)								\
		rts_error_csa(CSA_ARG(CSA) VARLSTCNT(8) MNEMONIC, 0,				\
				ERR_TEXT, 2, RTS_ERROR_LITERAL(CONTEXT), errno, 0);		\
	else											\
		rts_error_csa(CSA_ARG(CSA) VARLSTCNT(10) MNEMONIC, 0,				\
				ERR_TEXT, 2, RTS_ERROR_LITERAL(CONTEXT),			\
				ERR_TEXT, 2, RTS_ERROR_STRING(gai_strerror(ERRCODE)));		\
}

#endif
