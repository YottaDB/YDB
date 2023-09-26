/****************************************************************
 *								*
 * Copyright (c) 2023 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef GTM_TLS_EXTERNALCALLS
#define GTM_TLS_EXTERNALCALLS

#ifdef _AIX
#define PLUGIN_LIBRARY_OSNAME        "AIX"
#else
#define PLUGIN_LIBRARY_OSNAME        "Linux"
#endif
#ifdef DEBUG
#define PLUGIN_LIBRARY_RELEASE_TYPE        "(debug)"
#else
#define PLUGIN_LIBRARY_RELEASE_TYPE        ""
#endif
long gtm_tls_get_version(int count, char *version);
long gtm_tls_get_TLS_lib_version(int count, char *version, char *comptime, char *errstr);
long gtm_tls_get_defaultciphers(int count, char *tlsciphers, char *tlsver, char *errstr);
long gtm_tls_get_ciphers(int count, char *tlsciphers, char *tlsver, char *mode, char *tlsid, char *ciphersuite, char *errstr);
#endif
