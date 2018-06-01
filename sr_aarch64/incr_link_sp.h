/****************************************************************
 *								*
 * Copyright 2007, 2009 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 * Copyright (c) 2017 Stephen L Johnson. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef INCR_LINK_SP_INCLUDED
#define INCR_LINK_SP_INCLUDED

#include <rtnhdr.h>
#include <obj_file.h>
#include <elf.h>

#define NATIVE_HDR_LEN		(ROUND_UP2(SIZEOF(Elf32_Ehdr), SECTION_ALIGN_BOUNDARY))

#define COFFHDRLEN		(SIZEOF(int4) * 8)		/* Size of old masscomp-coff header before routine hdr */

#define GET_RTNHDR_ADDR(sym)	(unsigned char *)(sym)
#define DOREADRC_OBJFILE(FDESC, FBUFF, FBUFF_LEN, RC) DOREADRC(FDESC, FBUFF, FBUFF_LEN, RC)

#endif
