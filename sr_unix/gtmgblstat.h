/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Functions to support %YGBLSTAT  */
#ifndef GTMGBLSTAT_H
#define GTMGBLSTAT_H
gtm_status_t accumulate(int argc, gtm_string_t *acc, gtm_string_t *incr);
gtm_status_t is_big_endian(int argc, gtm_uint_t *endian);
gtm_status_t to_ulong(int argc, gtm_ulong_t *value, gtm_string_t *bytestr);
#endif /* GTMGBLSTAT_H */
