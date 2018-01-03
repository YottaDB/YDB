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

#define GDE_LABEL_SIZE 13
#define GDE_LABEL_NUM 1
/* Note, GDE_LABEL_LITERAL must be maintained in gdeinit.m if changes are made here */
/* The reference file for 64bittn/vermismatch expects this value so must be kept in sync */
/* Also change at least the assert in create_dummy_gbldir.c */
#define GDE_LABEL_LITERAL GTM64_ONLY("GTCGBDUNX112") NON_GTM64_ONLY("GTCGBDUNX012")
#define DEF_GDR_EXT "*.gld"
