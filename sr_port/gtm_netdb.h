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

#define SA_MAXLEN	NI_MAXHOST	/* NI_MAXHOST is 1025, large enough to hold any IPV6 address format
					 * e.g.(123:567:901:345:215:0:0:0)
					 */
#define SA_MAXLITLEN	NI_MAXHOST	/* large enough to hold any host name, e.g.
					 * host name google: dfw06s16-in-x12.1e100.net
					 */
#define USR_SA_MAXLITLEN	128	/* maximum size of host GTM user can specify
					 * the reason why the number is so small is because the host name size
					 * is stored as one byte in socket parameter list (refer to iosocket_use)
					 */

#ifdef VMS
#define VMS_MAX_TCP_IO_SIZE	(64 * 1024 - 512)	/* Hard limit for TCP send or recv size. On some implementations, the limit
							 * is 64K - 1, on others it is 64K - 512. We take the conservative approach
							 * and choose the lower limit
						   	 */
#endif

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
/* Get either string for either system or gai error */
#define TEXT_ADDRINFO(TEXT, ERRCODE, SAVEERRNO)			\
{								\
	if (EAI_SYSTEM == ERRCODE)				\
		TEXT = (char *)STRERROR(SAVEERRNO);		\
	else							\
		TEXT = (char *)gai_strerror(ERRCODE);			\
}

#endif
