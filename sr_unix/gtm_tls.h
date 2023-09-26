/****************************************************************
 *								*
 * Copyright (c) 2013-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_TLS_H
#define GTM_TLS_H

#define gtm_tls_get_error		(*gtm_tls_get_error_fptr)
#define gtm_tls_errno			(*gtm_tls_errno_fptr)
#define gtm_tls_init			(*gtm_tls_init_fptr)
#define gtm_tls_prefetch_passwd		(*gtm_tls_prefetch_passwd_fptr)
#define gtm_tls_socket			(*gtm_tls_socket_fptr)
#define gtm_tls_connect			(*gtm_tls_connect_fptr)
#define gtm_tls_accept			(*gtm_tls_accept_fptr)
#define gtm_tls_do_post_hand_shake	(*gtm_tls_do_post_hand_shake_fptr)
#define gtm_tls_repeat_hand_shake	(*gtm_tls_repeat_hand_shake_fptr)
#define gtm_tls_renegotiate		(*gtm_tls_renegotiate_fptr)
#define gtm_tls_get_conn_info		(*gtm_tls_get_conn_info_fptr)
#define gtm_tls_send			(*gtm_tls_send_fptr)
#define gtm_tls_recv			(*gtm_tls_recv_fptr)
#define gtm_tls_cachedbytes		(*gtm_tls_cachedbytes_fptr)
#define gtm_tls_socket_close		(*gtm_tls_socket_close_fptr)
#define gtm_tls_session_close		(*gtm_tls_session_close_fptr)
#define gtm_tls_fini			(*gtm_tls_fini_fptr)
#define gtm_tls_store_passwd		(*gtm_tls_store_passwd_fptr)
#define gtm_tls_add_config		(*gtm_tls_add_config_fptr)
#define gtm_tls_renegotiate_options	(*gtm_tls_renegotiate_options_fptr)
#define gtm_tls_version			(*gtm_tls_version_fptr)

/* It's important that the "ydb_tls_interface.h" include should be *after* the above macro definitions. This way, the function
 * prototypes defined in the header file will automatically be expanded to function pointers saving us the trouble of explicitly
 * defining them once again.
 */
#include "ydb_tls_interface.h"

#undef gtm_tls_get_error
#undef gtm_tls_errno
#undef gtm_tls_init
#undef gtm_tls_prefetch_passwd
#undef gtm_tls_socket
#undef gtm_tls_connect
#undef gtm_tls_accept
#undef gtm_tls_do_post_hand_shake
#undef gtm_tls_repeat_hand_shake
#undef gtm_tls_renegotiate
#undef gtm_tls_get_conn_info
#undef gtm_tls_send
#undef gtm_tls_recv
#undef gtm_tls_cachedbytes
#undef gtm_tls_socket_close
#undef gtm_tls_session_close
#undef gtm_tls_fini
#undef gtm_tls_store_passwd
#undef gtm_tls_add_config
#undef gtm_tls_renegotiate_options
#undef gtm_tls_version

/* Now, we need to define prototypes for wrapper functions that will be defined in GT.M to defer interrupts before invoking the
 * corresponding TLS function. But, to avoid redefining the prototypes, include the ydb_tls_interface.h once again to automatically
 * generate the prototypes.
 */
#define gtm_tls_get_error		intrsafe_gtm_tls_get_error
#define gtm_tls_errno			intrsafe_gtm_tls_errno
#define gtm_tls_init			intrsafe_gtm_tls_init
#define gtm_tls_prefetch_passwd		intrsafe_gtm_tls_prefetch_passwd
#define gtm_tls_socket			intrsafe_gtm_tls_socket
#define gtm_tls_connect			intrsafe_gtm_tls_connect
#define gtm_tls_accept			intrsafe_gtm_tls_accept
#define gtm_tls_do_post_hand_shake	intrsafe_gtm_tls_do_post_hand_shake
#define gtm_tls_repeat_hand_shake	intrsafe_gtm_tls_repeat_hand_shake
#define gtm_tls_renegotiate		intrsafe_gtm_tls_renegotiate
#define gtm_tls_get_conn_info		intrsafe_gtm_tls_get_conn_info
#define gtm_tls_send			intrsafe_gtm_tls_send
#define gtm_tls_recv			intrsafe_gtm_tls_recv
#define gtm_tls_cachedbytes		intrsafe_gtm_tls_cachedbytes
#define gtm_tls_socket_close		intrsafe_gtm_tls_socket_close
#define gtm_tls_session_close		intrsafe_gtm_tls_session_close
#define gtm_tls_fini			intrsafe_gtm_tls_fini
#define gtm_tls_store_passwd		intrsafe_gtm_tls_store_passwd
#define gtm_tls_add_config		intrsafe_gtm_tls_add_config
#define gtm_tls_renegotiate_options	intrsafe_gtm_tls_renegotiate_options
#define gtm_tls_version			intrsafe_gtm_tls_version

#undef GTM_TLS_INTERFACE_H	/* Allows us to include ydb_tls_interface.h twice. */
#include "ydb_tls_interface.h"	/* BYPASSOK : intentional duplicate include. */

GBLREF	gtm_tls_ctx_t			*tls_ctx;

int	gtm_tls_loadlibrary(void);

#endif
