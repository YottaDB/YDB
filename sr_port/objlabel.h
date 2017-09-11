/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* objlabel.h -- define *_OMAGIC, OBJ_LABEL and MAGIC_COOKIE here  */
#ifndef OBJLABEL_DEFINED
#define OBJLABEL_DEFINED

#define	YDB_OMAGIC	0411
#define	GTM_OMAGIC	0407	/* old impure format */

#define	OBJ_UNIX_LABEL	2	/* Increment each binary version change */
#ifdef USHBIN_SUPPORTED
#  define MAGIC_COOKIE_V5	((GTM_OMAGIC << 16) + (8 << 4) + 0) /* A version stake for V5. Should never change. */
#  define OBJ_LABEL	OBJ_UNIX_LABEL
#else
#  define YDB_OBJ_LABEL	0xF0	/* Flag that this is a YDB object (Linux32 does not use *_OMAGIC values) */
#  define OBJ_LABEL	((YDB_OBJ_LABEL << 8) + OBJ_UNIX_LABEL)
#endif
#define	MAGIC_COOKIE ((YDB_OMAGIC << 16) + OBJ_LABEL)

#endif /* OBJLABEL_DEFINED */
