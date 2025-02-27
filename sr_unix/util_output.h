/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

//kt added (created) this header file to achieve compilation

#ifndef UTIL_OUTPUT_H
#define UTIL_OUTPUT_H

#include <stdarg.h>  // For va_list
#include <sys/types.h>  // For caddr_t
#include "gtm_common_defs.h"  // For boolean_t

// Function declarations
caddr_t util_format(caddr_t message, va_list fao, caddr_t buff, ssize_t size, int faocnt);
void util_out_close(void);
void util_out_send_oper(char *addr, unsigned int len);
void util_out_print_vaparm(caddr_t message, int flush, va_list var, int faocnt);
void util_out_print(caddr_t message, int flush, ...);
void util_out_print_args(caddr_t message, int faocnt, int flush, ...);
boolean_t util_out_save(char *dst, int *dstlen_ptr);
void util_cond_flush(void);

#ifdef DEBUG
void util_out_syslog_dump(void);
#endif

#endif // UTIL_OUTPUT_H