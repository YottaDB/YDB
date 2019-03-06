/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC and/or its subsidiaries. *
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

<<<<<<< HEAD
#define	YDB_OMAGIC	0411
#define	GTM_OMAGIC	0407	/* old impure format */
=======
/* There is a system define for OMAGIC on some platforms that conficts with ours. Old
   use is deprecated in favor of GTM_OMAGIC flavor.
*/
#define	GTM_OMAGIC	0407		/* old impure format */
#ifndef USHBIN_SUPPORTED
#define OMAGIC	GTM_OMAGIC		/* non-native doesn't have to worry here */
#endif

/* The Object file label used to be a composite of a platform-generic part and platform-specific part, which is no longer the case
 *
 * 	OBJ_LABEL = (OBJ_UNIX_LABEL << n) + OBJ_PLATFORM_LABEL
 *      (n = 8 previously now n = 4 to allow more binary versions)
 *
 * For every object format change, we increment OBJ_UNIX_LABEL.
 * Note that OBJ_UNIX_LABEL should not exceed 4095 on 64 bit platforms and 255 on 32 bit platforms.
 * If the 32bit platform limit would be exceeded, start bumping the OBJ_PLATFORM_LABEL value - but only for 32 bit Linux.
 */

#define OBJ_UNIX_LABEL	37
#define	OBJ_PLATFORM_LABEL	0
>>>>>>> 74ea4a3c... GT.M V6.3-006

#define	OBJ_UNIX_LABEL	5	/* Increment each binary version change */
#ifdef USHBIN_SUPPORTED
#  define MAGIC_COOKIE_V5	((GTM_OMAGIC << 16) + (8 << 4) + 0) /* A version stake for V5. Should never change. */
#  define OBJ_LABEL	OBJ_UNIX_LABEL
#else
#  define YDB_OBJ_LABEL	0xF0	/* Flag that this is a YDB object (Linux32 does not use *_OMAGIC values) */
#  define OBJ_LABEL	((YDB_OBJ_LABEL << 8) + OBJ_UNIX_LABEL)
#endif
#define	MAGIC_COOKIE ((YDB_OMAGIC << 16) + OBJ_LABEL)

#endif /* OBJLABEL_DEFINED */
