/****************************************************************
 *								*
 * Copyright (c) 2018-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef DM_AUDIT_LOG_INCLUDED
#define DM_AUDIT_LOG_INCLUDED

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#ifdef GTM_TLS
#include "gtm_tls.h"
#endif

#define	MAX_SOCKADDR_UN_PATH            SIZEOF(((struct sockaddr_un *)0)->sun_path)
#define	MAX_AUDIT_PROC_INFO_LEN		64		/* Safe max length of "src=%d; uid=%d; euid=%d; pid=%d;" */
/* For every caller that calls dm_audit_log(), an integer value needs be defined to identify that caller.
 * Otherwise, caller should use AUDIT_SRC_UNKNOWN (0) to identify itself when calling dm_audit_log().
 * If new value defined, the caller must use the defined value as the second argument when calling dm_audit_log().
 */
#define AUDIT_SRC_UNKNOWN		0		/* Caller is unknown */
#define AUDIT_SRC_DMREAD		1		/* Caller is dm_read */
#define AUDIT_SRC_OPREAD		2		/* Caller is op_read */
/* Connection type when sending log info to logger */
#define AUDIT_CONN_INVALID		0
#define AUDIT_CONN_UN			1		/* UNIX domain (socket) */
#define AUDIT_CONN_TCP			2		/* TCP */
#define AUDIT_CONN_TLS			3		/* TLS */

/* This structure is used to store information
 * about the direct mode auditing logger.
 */
typedef struct
{
	int			conn_type;	/* Connection type (UNIX-domain/TCP/TLS) */
	struct sockaddr_un	un_addr;	/* UNIX domain socket address */
	struct addrinfo         *tcp_addr;	/* TCP/IP address */
	int			sock_fd;	/* Socket identifier */
#	ifdef GTM_TLS
	char			*tls_id;	/* Label of section in the GTM TLS configuration
						 * file containing the TLS options and/or certificate
						 * that GT.M uses to connect to the logger.
						 */
	gtm_tls_socket_t	*tls_sock;	/* TLS socket */
#	endif
	boolean_t		initialized;	/* TRUE if this struct is initialized */
} dm_audit_info;

void free_dm_audit_info(void);
int dm_audit_connect(void);
int dm_audit_init(char *host_info, boolean_t is_tls);
int dm_audit_log(mval *v, int src);

#endif /* DM_AUDIT_LOG_INCLUDED */
