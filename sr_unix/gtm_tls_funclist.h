/****************************************************************
 *								*
 * Copyright (c) 2013-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Any time a new function gets added to the GT.M SSL/TLS interface, add the corresponding entry here. This file is included in
 * `gtm_tls_loadlibrary', by appropriately, defining TLS_DEF, to generate necessary string literals and function pointers which
 * is used to `dlsym' symbols from the SSL/TLS shared library.
 */
TLS_DEF(gtm_tls_get_error)
TLS_DEF(gtm_tls_errno)
TLS_DEF(gtm_tls_init)
TLS_DEF(gtm_tls_prefetch_passwd)
TLS_DEF(gtm_tls_socket)
TLS_DEF(gtm_tls_connect)
TLS_DEF(gtm_tls_accept)
TLS_DEF(gtm_tls_renegotiate)
TLS_DEF(gtm_tls_get_conn_info)
TLS_DEF(gtm_tls_send)
TLS_DEF(gtm_tls_recv)
TLS_DEF(gtm_tls_cachedbytes)
TLS_DEF(gtm_tls_socket_close)
TLS_DEF(gtm_tls_session_close)
TLS_DEF(gtm_tls_fini)
TLS_DEF(gtm_tls_store_passwd)
TLS_DEF(gtm_tls_add_config)
TLS_DEF(gtm_tls_renegotiate_options)
