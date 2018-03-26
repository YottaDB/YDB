/****************************************************************
 *								*
 * Copyright (c) 2010-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef CREATE_FATAL_ERROR_ZSHOW_DMP_H_
#define	CREATE_FATAL_ERROR_ZSHOW_DMP_H_

#define YDBFATAL_ERROR_DUMP_FILENAME	"YDB_FATAL_ERROR.ZSHOW_DMP_"
#define YDBFATAL_ERROR_DUMP_FILETYPE 	".txt"

void create_fatal_error_zshow_dmp(int4 signal);

#endif
