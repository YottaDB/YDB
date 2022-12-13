/****************************************************************
 *                                                              *
 * Copyright (c) 2018-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved. *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"
#include "gtmxc_types.h"
#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include "gtm_fcntl.h"
#include "gtm_limits.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_un.h"
#include "gtm_syslog.h"
#include "gtm_unistd.h"
#include "gtm_socket.h"
#include "gtm_string.h"
#include "gtm_strings.h"
#include "gtm_ipv6.h"
#include "gtm_inet.h"
#ifdef GTM_TLS
#include "gtm_tls.h"
#endif
#include "have_crit.h"
#include "io.h"
#include "iottdef.h"
#include "util.h"
#include "restrict.h"
#include "dm_audit_log.h"
#include "error.h"
#include "send_msg.h"
#include "gtmmsg.h"
#include "min_max.h"
#include "fgncal.h"		/* Needed for MAX_ERRSTR_LEN */
#include "eintr_wrappers.h"
#include "compiler.h"		/* Needed for MAX_SRCLINE */

#define	ON			1		/* Used to enable socket options */
#define STRINGIFY(S)		#S
#define BOUNDED_FMT(LIMIT,TYPE)	"%" STRINGIFY(LIMIT) TYPE
#define TLSID_FMTSTR		BOUNDED_FMT(MAX_TLSID_LEN, "s")
#define PORTNUM_FMTSTR		BOUNDED_FMT(NI_MAXSERV, "[^:]")
#define IPV4_FMTSTR		BOUNDED_FMT(SA_MAXLEN, "[^:]")
#define IPV6_FMTSTR		"[" BOUNDED_FMT(SA_MAXLEN, "[^]]") "]"

GBLREF	io_pair			io_curr_device;
GBLREF	io_pair			io_std_device;
GBLREF	char			dl_err[MAX_ERRSTR_LEN];	/* This is maintained by gtm_tls_loadlibrary() */
GBLREF	dm_audit_info		audit_conn[];
GBLREF	void 			(*primary_exit_handler)(void);
GBLREF	boolean_t 		restrict_initialized;
GBLREF	enum gtmImageTypes	image_type;
LITREF	gtmImageName		gtmImageNames[];
GBLREF	mstr			sys_input;

error_def(ERR_TLSDLLNOOPEN);
error_def(ERR_TEXT);
error_def(ERR_SOCKINIT);
error_def(ERR_TLSINIT);
error_def(ERR_SETSOCKOPTERR);
error_def(ERR_INVADDRSPEC);
error_def(ERR_GETADDRINFO);
error_def(ERR_SYSCALL);
error_def(ERR_TLSHANDSHAKE);
error_def(ERR_TEXT);
error_def(ERR_TLSIOERROR);
error_def(ERR_TLSCONVSOCK);
error_def(ERR_OPENCONN);
error_def(ERR_APDINITFAIL);
error_def(ERR_APDCONNFAIL);
error_def(ERR_APDLOGFAIL);
error_def(ERR_AUDINITFAIL);
error_def(ERR_AUDCONNFAIL);
error_def(ERR_AUDLOGFAIL);
error_def(ERR_SOCKCLOSE);
error_def(ERR_RESTRICTEDOP);

/* Frees all pointers allocated by audit_conn[save_is_zauditlog] */
void	free_dm_audit_info_ptrs(void)
{
	intrpt_state_t		prev_intrpt_state;
	gtm_tls_ctx_t		*dm_audit_tls_ctx;
	boolean_t		save_is_zauditlog;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	save_is_zauditlog = TREF(is_zauditlog);
#	ifdef GTM_TLS
	if (NULL != audit_conn[save_is_zauditlog].tls_sock)
	{
		dm_audit_tls_ctx = audit_conn[save_is_zauditlog].tls_sock->gtm_ctx;
		gtm_tls_session_close((gtm_tls_socket_t **)&audit_conn[save_is_zauditlog].tls_sock);
		if (NULL != dm_audit_tls_ctx)
			gtm_tls_fini((gtm_tls_ctx_t **)&dm_audit_tls_ctx);
	}
#	endif
	if (AUDIT_CONN_TCP == audit_conn[save_is_zauditlog].conn_type)
	{
		DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
		freeaddrinfo(audit_conn[save_is_zauditlog].netaddr.tcp_addr);
		ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
	}
}

/* Establishes a connection to the logger by using the
 * information contained in the audit_conn global struct.
 *
 * returns:
 * 	-1 if something went wrong when trying to connect
 * 	0 if connection successfull
 */
int	dm_audit_connect(void)
{
	struct addrinfo		*head_ai_ptr = NULL, *remote_ai_ptr = NULL;
	boolean_t		need_socket = TRUE, need_connect = TRUE, save_is_zauditlog;
	struct linger		enable_linger = {1, 3};
	char			*errptr;
	int			on = ON, save_errno, status, errlen;
#	ifdef GTM_TLS
	gtm_tls_ctx_t		*dm_audit_tls_ctx;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	save_is_zauditlog = TREF(is_zauditlog);
	assert((AUDIT_CONN_INVALID != audit_conn[save_is_zauditlog].conn_type) && audit_conn[save_is_zauditlog].initialized);
	/* If connected previously, close old tls and/or tcp sockets so they can be reused */
	if (FD_INVALID != audit_conn[save_is_zauditlog].sock_fd)
	{
		if ((AUDIT_CONN_TLS == audit_conn[save_is_zauditlog].conn_type)
				&& (NULL != audit_conn[save_is_zauditlog].tls_sock))
			gtm_tls_socket_close(audit_conn[save_is_zauditlog].tls_sock);
		else
			assert((AUDIT_CONN_TCP == audit_conn[save_is_zauditlog].conn_type)
				|| (AUDIT_CONN_UN == audit_conn[save_is_zauditlog].conn_type));
		close(audit_conn[save_is_zauditlog].sock_fd);
		audit_conn[save_is_zauditlog].sock_fd = FD_INVALID;
	}
	if (AUDIT_CONN_UN != audit_conn[save_is_zauditlog].conn_type)
	{	/* ======================= Do the TCP connection ======================== */
		assert(NULL != audit_conn[save_is_zauditlog].netaddr.tcp_addr);
		head_ai_ptr = audit_conn[save_is_zauditlog].netaddr.tcp_addr;
		do
		{	/* Test all address families until connected */
			if (need_socket && (FD_INVALID != audit_conn[save_is_zauditlog].sock_fd))
			{
				close(audit_conn[save_is_zauditlog].sock_fd);
				audit_conn[save_is_zauditlog].sock_fd = FD_INVALID;
			}
			if (need_socket)
			{
				assert(FD_INVALID == audit_conn[save_is_zauditlog].sock_fd);
				for (remote_ai_ptr = head_ai_ptr; NULL != remote_ai_ptr; remote_ai_ptr = remote_ai_ptr->ai_next)
				{	/* Go through each addrinfo until socket creation with one of them is successful */
					audit_conn[save_is_zauditlog].sock_fd = socket(remote_ai_ptr->ai_family,
							remote_ai_ptr->ai_socktype, remote_ai_ptr->ai_protocol);
					if (FD_INVALID != audit_conn[save_is_zauditlog].sock_fd)
						break;
				}
				if (FD_INVALID == audit_conn[save_is_zauditlog].sock_fd)
				{
					save_errno = errno;
					errptr = (char *)STRERROR(save_errno);
					if ((IS_MUPIP_IMAGE) || (IS_DSE_IMAGE) || (IS_LKE_IMAGE) || (save_is_zauditlog))
					{
						send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_AUDCONNFAIL, 1,
								gtmImageNames[image_type].imageName,
								ERR_SOCKINIT, 3, save_errno, LEN_AND_STR(errptr));
					} else if (IS_MUMPS_IMAGE)
						send_msg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_APDCONNFAIL, 0,
								ERR_SOCKINIT, 3, save_errno, LEN_AND_STR(errptr));
					return -1;
				}
				head_ai_ptr = remote_ai_ptr->ai_next;	/* Get ready to try next address if connect fails */
				need_socket = FALSE;
			}
			save_errno = status = 0;
			if (need_connect)
			{	/* Attempt a connect to logger */
				assert((FD_INVALID != audit_conn[save_is_zauditlog].sock_fd) && (NULL != remote_ai_ptr));
				if (0 > setsockopt(audit_conn[save_is_zauditlog].sock_fd, SOL_SOCKET, SO_LINGER,
							(const void *)&enable_linger, SIZEOF(enable_linger)))
				{
					save_errno = errno;
					errptr = (char *)STRERROR(save_errno);
					errlen = STRLEN(errptr);
					close(audit_conn[save_is_zauditlog].sock_fd);
					audit_conn[save_is_zauditlog].sock_fd = FD_INVALID;
					send_msg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
							LEN_AND_LIT("SO_LINGER"), save_errno, errlen, errptr);
				}
#                       	ifdef TCP_NODELAY
				if (0 > setsockopt(audit_conn[save_is_zauditlog].sock_fd, IPPROTO_TCP, TCP_NODELAY,
						&on, SIZEOF(on)))
				{
					save_errno = errno;
					errptr = (char *)STRERROR(save_errno);
					errlen = STRLEN(errptr);
					close(audit_conn[save_is_zauditlog].sock_fd);
					audit_conn[save_is_zauditlog].sock_fd = FD_INVALID;
					send_msg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
							LEN_AND_LIT("TCP_NODELAY"), save_errno, errlen, errptr);
				}
#				endif
				CONNECT_SOCKET(audit_conn[save_is_zauditlog].sock_fd, remote_ai_ptr->ai_addr,
						remote_ai_ptr->ai_addrlen, status);
				save_errno = errno;
				/* CONNECT_SOCKET should have handled EINTR. Assert that */
				assert((0 <= status) || (EINTR != save_errno));
				if (0 <= status)
				{
					need_connect = FALSE;
					save_errno = 0;	/* Connection successful - need to reset save_errno */
				} else
				{
					switch (save_errno)
					{
						case ETIMEDOUT: /* The other side bound but not listening - fall through */
						case ECONNREFUSED: /* Connection refused */
							if (NULL != head_ai_ptr)
							{	/* Do loop again - try next address */
								need_connect = need_socket = TRUE;
								save_errno = 0;
								status = -1;
							} else
							{	/* We've run out of addresses to try */
								need_connect = need_socket = FALSE;
								status = 0;	/* Break out of loop */
							}
							break;
						default: /* Connect failed */
							need_connect = FALSE;
							status = 0;	/* Break out of loop */
							break;
					}
				}
			}
		} while (0 > status);
		if (save_errno)
		{	/* Failed to connect, throw error */
			if (FD_INVALID != audit_conn[save_is_zauditlog].sock_fd)
			{
				close(audit_conn[save_is_zauditlog].sock_fd);
				audit_conn[save_is_zauditlog].sock_fd = FD_INVALID;
			}
			if ((IS_MUPIP_IMAGE) || (IS_DSE_IMAGE) || (IS_LKE_IMAGE) || (save_is_zauditlog))
			{
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_AUDCONNFAIL, 1, gtmImageNames[image_type].imageName,
						ERR_OPENCONN, 0, save_errno);
			} else if (IS_MUMPS_IMAGE)
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_APDCONNFAIL, 0, ERR_OPENCONN, 0, save_errno);
			return -1;
		}
		assert(FD_INVALID != audit_conn[save_is_zauditlog].sock_fd);
		if (AUDIT_CONN_TLS == audit_conn[save_is_zauditlog].conn_type)
		{	/* ================== Do the TLS connection/handshake =================== */
			/* If tls socket was not initialized, we need to create one */
			if (NULL == audit_conn[save_is_zauditlog].tls_sock)
			{	/* gtm_tls_loadlibrary() must be called before gtm_tls_init() */
				if(SS_NORMAL != (status = gtm_tls_loadlibrary()))
				{
					close(audit_conn[save_is_zauditlog].sock_fd);
					audit_conn[save_is_zauditlog].sock_fd = FD_INVALID;
					/* dl_err is initialized and set in gtm_tls_loadlibrary() when an error occurs */
					if ((IS_MUPIP_IMAGE) || (IS_DSE_IMAGE) || (IS_LKE_IMAGE))
					{
						send_msg_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_AUDCONNFAIL, 1,
								gtmImageNames[image_type].imageName,
								ERR_TLSDLLNOOPEN, 0, ERR_TEXT, 2, LEN_AND_STR(dl_err));
					} else if (IS_MUMPS_IMAGE)
						send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_APDCONNFAIL, 0,
								ERR_TLSDLLNOOPEN, 0, ERR_TEXT, 2, LEN_AND_STR(dl_err));
					return -1;
				}
				/* Create the SSL/TLS context */
				if (NULL == (dm_audit_tls_ctx = gtm_tls_init(GTM_TLS_API_VERSION, GTMTLS_OP_INTERACTIVE_MODE)))
				{
					close(audit_conn[save_is_zauditlog].sock_fd);
					audit_conn[save_is_zauditlog].sock_fd = FD_INVALID;
					errptr = (char *)gtm_tls_get_error(NULL);
					if ((IS_MUPIP_IMAGE) || (IS_DSE_IMAGE) || (IS_LKE_IMAGE) || (save_is_zauditlog))
					{
						send_msg_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_AUDCONNFAIL, 1,
								gtmImageNames[image_type].imageName,
								ERR_TLSINIT, 0, ERR_TEXT, 2, LEN_AND_STR(errptr));
					} else if (IS_MUMPS_IMAGE)
						send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_APDCONNFAIL, 0,
								ERR_TLSINIT, 0, ERR_TEXT, 2, LEN_AND_STR(errptr));
					return -1;
				}
				if (primary_exit_handler)
					atexit(primary_exit_handler);
			} else
			{	/* TLS socket already exists, so just reuse it */
				assert(NULL != audit_conn[save_is_zauditlog].tls_sock->gtm_ctx);
				dm_audit_tls_ctx = audit_conn[save_is_zauditlog].tls_sock->gtm_ctx;
			}
			assert(NULL != dm_audit_tls_ctx);
			/* Create tls socket */
			audit_conn[save_is_zauditlog].tls_sock = gtm_tls_socket(dm_audit_tls_ctx,
									audit_conn[save_is_zauditlog].tls_sock,
									audit_conn[save_is_zauditlog].sock_fd,
									audit_conn[save_is_zauditlog].tls_id,
									GTMTLS_OP_DM_AUDIT | GTMTLS_OP_CLIENT_MODE);
			if (NULL == audit_conn[save_is_zauditlog].tls_sock)
			{
				errptr = (char *)gtm_tls_get_error(NULL);
				close(audit_conn[save_is_zauditlog].sock_fd);
				audit_conn[save_is_zauditlog].sock_fd = FD_INVALID;
				restrict_initialized = TRUE;
				if ((IS_MUPIP_IMAGE) || (IS_DSE_IMAGE) || (IS_LKE_IMAGE) || (save_is_zauditlog))
				{
					send_msg_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_AUDCONNFAIL, 1,
							gtmImageNames[image_type].imageName,
							ERR_TLSCONVSOCK, 0, ERR_TEXT, 2, LEN_AND_STR(errptr));
				} else if (IS_MUMPS_IMAGE)
					send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_APDCONNFAIL, 0,
							ERR_TLSCONVSOCK, 0, ERR_TEXT, 2, LEN_AND_STR(errptr));
				return -1;
			}
			/* Attempt TLS handshake with logger */
			if (0 > gtm_tls_connect(audit_conn[save_is_zauditlog].tls_sock))
			{
				errptr = (char *)gtm_tls_get_error(audit_conn[save_is_zauditlog].tls_sock);
				gtm_tls_socket_close(audit_conn[save_is_zauditlog].tls_sock);
				close(audit_conn[save_is_zauditlog].sock_fd);
				audit_conn[save_is_zauditlog].sock_fd = FD_INVALID;
				restrict_initialized = TRUE;
				if ((IS_MUPIP_IMAGE) || (IS_DSE_IMAGE) || (IS_LKE_IMAGE) || (save_is_zauditlog))
				{
					send_msg_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_AUDCONNFAIL, 1,
							gtmImageNames[image_type].imageName,
							ERR_TLSHANDSHAKE, 0, ERR_TEXT, 2, LEN_AND_STR(errptr));
				} else if (IS_MUMPS_IMAGE)
					send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_APDCONNFAIL, 0,
							ERR_TLSHANDSHAKE, 0, ERR_TEXT, 2, LEN_AND_STR(errptr));
				return -1;
			}
		}
	} else
	{	/* =============== Do the unix domain socket connection ================ */
		assert('\0' != audit_conn[save_is_zauditlog].netaddr.un_addr.sun_path[0]);
		audit_conn[save_is_zauditlog].sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
		if (FD_INVALID == audit_conn[save_is_zauditlog].sock_fd)
		{
			save_errno = errno;
			errptr = (char *)STRERROR(save_errno);
			restrict_initialized = TRUE;
			if ((IS_MUPIP_IMAGE) || (IS_DSE_IMAGE) || (IS_LKE_IMAGE) || (save_is_zauditlog))
			{
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_AUDCONNFAIL, 1,
						gtmImageNames[image_type].imageName,
						ERR_SOCKINIT, 3, save_errno, LEN_AND_STR(errptr));
			} else if (IS_MUMPS_IMAGE)
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_APDCONNFAIL, 0,
						ERR_SOCKINIT, 3, save_errno, LEN_AND_STR(errptr));
			return -1;
		}
		CONNECT_SOCKET(audit_conn[save_is_zauditlog].sock_fd,
					(struct sockaddr *)&(audit_conn[save_is_zauditlog].netaddr.un_addr),
					SIZEOF(struct sockaddr_un), status);
		if (0 > status)
		{
			save_errno = errno;
			close(audit_conn[save_is_zauditlog].sock_fd);
			audit_conn[save_is_zauditlog].sock_fd = FD_INVALID;
			restrict_initialized = TRUE;
			if ((IS_MUPIP_IMAGE) || (IS_DSE_IMAGE) || (IS_LKE_IMAGE) || (save_is_zauditlog))
			{
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_AUDCONNFAIL, 1,
						gtmImageNames[image_type].imageName,
						ERR_OPENCONN, 0, save_errno);
			} else if (IS_MUMPS_IMAGE)
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_APDCONNFAIL, 0, ERR_OPENCONN, 0, save_errno);
			return -1;
		}
	}
	return 0;
}

/* Initializes the global variable/struct audit_conn[aud_indx] with the logger's (server)
 * connection information (obtained from restriction file).
 *
 * params:
 * 	@host_info contains either path to domain socket file or IP+port
 * 		Format:
 * 			<ip>:<portno>:<tlsid>
 * 		Args:
 * 			<ip>     = Logger's IP address (if enclosed in '[' and ']',
 * 					then we assume it's IPv6, otherwise IPv4)
 * 			<portno> = Port number that the logger listens on
 * 			<tlsid>  = (Optional) Label of section in the configuration
 * 					file containing the TLS options and/or certificate
 * 					that GT.M uses when connecting to the logger.
 * 		NOTE: If "host_info" does not match the above format, then we
 * 			assume it contains path to domain socket file.
 * 	@is_tls determines whether we are initializing a tls connection
 * returns:
 * 	-1 if something went wrong when initializing and connecting
 * 	0 if initialization and connection successfull
 */
int	dm_audit_init(char *host_info, boolean_t is_tls)
{
	char			host[SA_MAXLEN + 1];
	struct addrinfo		*remote_ai_head, hints;
	intrpt_state_t		prev_intrpt_state;
	char			port_buffer[NI_MAXSERV + 1];
	int			host_len, port_len;
	unsigned int		host_info_len;
	int			errcode, real_errno;
	int			fields, port;
	char			tlsid[MAX_TLSID_LEN + 1];
	char 			*errptr;
	boolean_t		save_is_zauditlog;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	save_is_zauditlog = TREF(is_zauditlog);
	free_dm_audit_info_ptrs();
        if ((NULL == host_info) || ('\0' == host_info[0]))
	{	/* No logger (server) information provided */
		restrict_initialized = TRUE; // needed for gtm_putmsg_list.c
		if ((IS_MUPIP_IMAGE) || (IS_DSE_IMAGE) || (IS_LKE_IMAGE) || (save_is_zauditlog))
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_AUDINITFAIL, 1,
					gtmImageNames[image_type].imageName, ERR_INVADDRSPEC);
		else if (IS_MUMPS_IMAGE)
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_APDINITFAIL, 0, ERR_INVADDRSPEC);
		return -1;
	}
	audit_conn[save_is_zauditlog].sock_fd = FD_INVALID;
	if ('[' == host_info[0])
		/* If first character is '[', then we assume that the IP address is enclosed in brackets and is IPV6 */
		fields = SSCANF(host_info, IPV6_FMTSTR " : " PORTNUM_FMTSTR " : " TLSID_FMTSTR, host, port_buffer, tlsid);
	else
		/* Otherwise, we assume "host_info" contains an IPV4 address or is a path to file domain socket file */
		fields = SSCANF(host_info, IPV4_FMTSTR " : " PORTNUM_FMTSTR " : " TLSID_FMTSTR, host, port_buffer, tlsid);
	host_len = STRLEN(host);
	if ((0 == fields) || (0 == host_len))
	{
		restrict_initialized = TRUE;
		if ((IS_MUPIP_IMAGE) || (IS_DSE_IMAGE) || (IS_LKE_IMAGE))
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_AUDINITFAIL, 1,
					gtmImageNames[image_type].imageName, ERR_INVADDRSPEC);
		else if (IS_MUMPS_IMAGE)
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_APDINITFAIL, 0, ERR_INVADDRSPEC);
		return -1;
	}
	if ((3 == fields) || (2 == fields))
	{	/* If we get  2 or 3 fields, then we assume host_info contains IP and port information */
		port_len = STRLEN(port_buffer);
		port_buffer[port_len]='\0';
		CLIENT_HINTS(hints);
		DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
		if (0  != (errcode = getaddrinfo(host, port_buffer, &hints, &remote_ai_head)))
		{
			real_errno = errno;
			ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
			TEXT_ADDRINFO(errptr, errcode, real_errno);
			restrict_initialized = TRUE;
			if ((IS_MUPIP_IMAGE) || (IS_DSE_IMAGE) || (IS_LKE_IMAGE) || (save_is_zauditlog))
			{
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_AUDCONNFAIL, 1,
						gtmImageNames[image_type].imageName,
						ERR_GETADDRINFO, 0, ERR_TEXT, 2, LEN_AND_STR(errptr));
			} else if (IS_MUMPS_IMAGE)
			{
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_APDCONNFAIL, 0,
						ERR_GETADDRINFO, 0, ERR_TEXT, 2, LEN_AND_STR(errptr));
			}
			return -1;
		}
		ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
		audit_conn[save_is_zauditlog].netaddr.tcp_addr = remote_ai_head;
		if (!is_tls)
			audit_conn[save_is_zauditlog].conn_type = AUDIT_CONN_TCP;
		else
		{
			audit_conn[save_is_zauditlog].conn_type = AUDIT_CONN_TLS;
			audit_conn[save_is_zauditlog].tls_id[0] = '\0';
			if ((3 == fields) && ('\0' != tlsid[0]))
				memcpy(audit_conn[save_is_zauditlog].tls_id, tlsid, MAX_TLSID_LEN + 1);
		}
	} else
	{	/* Assume host_info contains path to unix domain socket
		 * if number of fields is 1 or is more than 3.
		 */
		host_info_len = strlen(host_info);
		if (is_tls || (MAX_SOCKADDR_UN_PATH <= host_info_len))
		{	/* If the "TLS" option is specified but no IP/port info provided or path to socket file is too long */
			restrict_initialized = TRUE;
			if ((IS_MUPIP_IMAGE) || (IS_DSE_IMAGE) || (IS_LKE_IMAGE))
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_AUDINITFAIL, 1,
					gtmImageNames[image_type].imageName, ERR_INVADDRSPEC);
			else if (IS_MUMPS_IMAGE)
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_APDINITFAIL, 0, ERR_INVADDRSPEC);
			return -1;
		}
		audit_conn[save_is_zauditlog].conn_type = AUDIT_CONN_UN;
		(audit_conn[save_is_zauditlog].netaddr.un_addr).sun_family = AF_UNIX;
		memcpy((audit_conn[save_is_zauditlog].netaddr.un_addr).sun_path, host_info, host_info_len);
		(audit_conn[save_is_zauditlog].netaddr.un_addr).sun_path[host_info_len] = '\0';
	}
	audit_conn[save_is_zauditlog].initialized = TRUE;
	return 0;
}

/* Responsible for audit logging. It essentially sends the to-be-logged
 * command or activity to the listener (logger) for logging.
 *
 * params:
 * 	@v contains command to be logged
 * 	@src integer that identifies the source caller
 * returns:
 * 	FALSE if Direct Mode / Utilities Auditing enabled and logging failed
 * 	TRUE if Direct Mode / Utilities Auditing disabled, or if enabled and logging successfull
 */
int	dm_audit_log(mval *v, int src)
{
	unsigned char	log_msg_pre[GTM_PATH_MAX + MAX_AUDIT_PROC_INFO_LEN + MAX_SRCLINE + 1], *log_msg;
	char 		cmd_pre[MAX_SRCLINE + 1], *cmd, *errptr, ttybuf[TTY_NAME_MAX + 1];
	int		status, save_errno, log_msg_len, max_log_msg_len;
	boolean_t	need_free = FALSE, save_is_zauditlog;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	save_is_zauditlog = TREF(is_zauditlog);
	if ((AUDIT_SRC_MUPIP != src) && (AUDIT_SRC_DSE != src) && (AUDIT_SRC_LKE != src) && (AUDIT_SRC_GTM != src))
	{
		if (AUDIT_DISABLE == RESTRICTED(dm_audit_enable))
			return TRUE;
		assert(TREF(dollar_zaudit));
	}
	/* Never log empty commands (useless entries) */
	if (v->str.len == 0)
		return TRUE;
	/* Skip logging for DMREAD and OPREAD when $IO != $PRINCIPAL */
	if (((AUDIT_SRC_DMREAD == src) || (AUDIT_SRC_OPREAD == src)) && (io_curr_device.in != io_std_device.in))
		return TRUE;
	/* Make sure the caller is classified, otherwise set to unknown */
	if ((AUDIT_SRC_DMREAD != src) && (AUDIT_SRC_OPREAD != src) && (AUDIT_SRC_MUPIP != src) && (AUDIT_SRC_DSE != src)
			&& (AUDIT_SRC_LKE != src) && (AUDIT_SRC_GTM != src))
		src = AUDIT_SRC_UNKNOWN;	/* Unknown source caller */
	/* Check if audit information was initialized */
	if (!audit_conn[save_is_zauditlog].initialized)
	{
		restrict_initialized = TRUE;
		if ((IS_MUPIP_IMAGE)|| (IS_DSE_IMAGE) || (IS_LKE_IMAGE) || (save_is_zauditlog))
		{
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_AUDLOGFAIL, 1, gtmImageNames[image_type].imageName,
					ERR_TEXT, 2, LEN_AND_LIT("Audit information is not initialized"));
		} else if (IS_MUMPS_IMAGE)
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_APDLOGFAIL, 0,
					ERR_TEXT, 2, LEN_AND_LIT("Audit information is not initialized"));
		return FALSE;
	}
	/* Check if an initial connect is needed */
	if ((FD_INVALID == audit_conn[save_is_zauditlog].sock_fd) && (0 > dm_audit_connect()))
	{
		restrict_initialized = TRUE;
		if ((IS_MUPIP_IMAGE) || (IS_DSE_IMAGE) || (IS_LKE_IMAGE) || (save_is_zauditlog))
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_AUDLOGFAIL, 1, gtmImageNames[image_type].imageName);
		else if (IS_MUMPS_IMAGE)
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_APDLOGFAIL, 0, ERR_APDCONNFAIL);
		return FALSE;
	}
	assert(FD_INVALID != audit_conn[save_is_zauditlog].sock_fd);
	if (MAX_SRCLINE < v->str.len)
	{
		max_log_msg_len = GTM_PATH_MAX + MAX_AUDIT_PROC_INFO_LEN + v->str.len;
		log_msg = (unsigned char *)malloc(max_log_msg_len + 1);
		cmd = (char *)malloc(v->str.len + 1);
		need_free = TRUE;
	} else
	{
		max_log_msg_len = GTM_PATH_MAX + MAX_AUDIT_PROC_INFO_LEN + MAX_SRCLINE;
		log_msg = log_msg_pre;
		cmd = cmd_pre;
	}
	STRNCPY_STR(cmd, v->str.addr, v->str.len);
	cmd[v->str.len] = '\0';
	memset(ttybuf, '\0', TTY_NAME_MAX  + 1);
	memcpy(ttybuf, sys_input.addr, sys_input.len);

	/* Attach process information to command as one message*/
	log_msg_len = SNPRINTF((char *)log_msg, max_log_msg_len + 1, "dist=%s; src=%d; uid=%d; euid=%d; pid=%d; tty=%s; command=%s",
					getenv("gtm_dist"), src, getuid(), geteuid(), getpid(), ttybuf, cmd);

	assert((max_log_msg_len + 1) > log_msg_len);
	log_msg[log_msg_len] = '\0';
	if (need_free)
		free(cmd);
	if (AUDIT_CONN_TLS == audit_conn[save_is_zauditlog].conn_type)
	{	/* Connection type is TLS */
		assert(NULL != audit_conn[save_is_zauditlog].tls_sock);
		if (0 >= gtm_tls_send(audit_conn[save_is_zauditlog].tls_sock, (char *)log_msg, log_msg_len))
		{	/* Either invalid socket or sending failed */
			if (need_free)
				free(log_msg);
			errptr = (char *)gtm_tls_get_error(audit_conn[save_is_zauditlog].tls_sock);
			gtm_tls_socket_close(audit_conn[save_is_zauditlog].tls_sock);
			close(audit_conn[save_is_zauditlog].sock_fd);
			audit_conn[save_is_zauditlog].sock_fd = FD_INVALID;
			restrict_initialized = TRUE;
			if ((IS_MUPIP_IMAGE) || (IS_DSE_IMAGE) || (IS_LKE_IMAGE) || (save_is_zauditlog))
			{
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(11) ERR_AUDLOGFAIL, 1, gtmImageNames[image_type].imageName,
						ERR_TLSIOERROR, 2, LEN_AND_LIT("send"), ERR_TEXT, 2, LEN_AND_STR(errptr));
			} else if (IS_MUMPS_IMAGE)
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(10) ERR_APDLOGFAIL, 0,
						ERR_TLSIOERROR, 2, LEN_AND_LIT("send"), ERR_TEXT, 2, LEN_AND_STR(errptr));
			return FALSE;
		}
	} else
	{	/* Connection type is either TCP or unix domain socket */
		SEND(audit_conn[save_is_zauditlog].sock_fd, (char *)log_msg, log_msg_len, 0, status);
		if (0 >= status)
		{	/* Sending failed */
			save_errno = errno;
			if (need_free)
				free(log_msg);
			close(audit_conn[save_is_zauditlog].sock_fd);
			audit_conn[save_is_zauditlog].sock_fd = FD_INVALID;
			restrict_initialized = TRUE;
			if ((IS_MUPIP_IMAGE) || (IS_DSE_IMAGE) || (IS_LKE_IMAGE) || (save_is_zauditlog))
			{
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(11) ERR_AUDLOGFAIL, 1, gtmImageNames[image_type].imageName,
						ERR_SYSCALL, 5, LEN_AND_LIT("SEND"), CALLFROM, save_errno);
			} else if (IS_MUMPS_IMAGE)
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(10) ERR_APDLOGFAIL, 0,
						ERR_SYSCALL, 5, LEN_AND_LIT("SEND"), CALLFROM, save_errno);
			return FALSE;
		}
	}
	if (need_free)
		free(log_msg);
	errno = 0;
	if (AUDIT_CONN_TLS == audit_conn[save_is_zauditlog].conn_type)
		gtm_tls_socket_close(audit_conn[save_is_zauditlog].tls_sock);
	if (AUDIT_CONN_TCP == audit_conn[save_is_zauditlog].conn_type)
	{
		if (-1 == shutdown(audit_conn[save_is_zauditlog].sock_fd, SHUT_WR))
		{
			assert(FALSE);
			audit_conn[save_is_zauditlog].sock_fd = FD_INVALID;
			return FALSE;
		}
	}
	/* Check for error on normal socket close */
	if (-1 == close(audit_conn[save_is_zauditlog].sock_fd))
	{
		save_errno = errno;
		errptr = (char *)STRERROR(save_errno);
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_AUDCONNFAIL, 1,gtmImageNames[image_type].imageName,
				ERR_SOCKCLOSE, 3, save_errno, LEN_AND_STR(errptr));
		audit_conn[save_is_zauditlog].sock_fd = FD_INVALID;
		return FALSE;
	}
	audit_conn[save_is_zauditlog].sock_fd = FD_INVALID;
	return TRUE;
}

/* Check for image_type ensures that the code executes only for the specific image and commands log properly. */
int log_cmd_if_needed(char *logcmd)
{
	boolean_t	audit_on;
	mval		input_line;
	int		aud_type;

	switch (image_type)
	{
		case DSE_IMAGE:
			audit_on = RESTRICTED(dse_audit_enable);
			aud_type = AUDIT_SRC_DSE;
			break;
		case LKE_IMAGE:
			audit_on = RESTRICTED(lke_audit_enable);
			aud_type = AUDIT_SRC_LKE;
			break;
		case MUPIP_IMAGE:
			audit_on = RESTRICTED(mupip_audit_enable);
			aud_type = AUDIT_SRC_MUPIP;
			break;
		default:
			return -1;
	}
	if (audit_on)
	{
		input_line.mvtype = MV_STR;
		input_line.str.addr = logcmd;
		input_line.str.len = STRLEN(input_line.str.addr);
		if (!dm_audit_log(&input_line, aud_type))
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) MAKE_MSG_SEVERE(ERR_RESTRICTEDOP), 1,
					gtmImageNames[image_type].imageName);
			return 1;
		}
	}
	return 0;
}
