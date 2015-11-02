/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef   	CREATE_FATAL_ERROR_ZSHOW_DMP_H_
# define   	CREATE_FATAL_ERROR_ZSHOW_DMP_H_

#define GTMFATAL_ERROR_DUMP_FILENAME	"GTM_FATAL_ERROR"

void create_fatal_error_zshow_dmp(int4 signal, boolean_t repeat_error);

#endif
