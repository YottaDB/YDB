/****************************************************************
 *								*
 * Copyright (c) 2005-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GDSDBVER_H_INCLUDED
#define GDSDBVER_H_INCLUDED
/* Database version related definitions */

/* Values for bytes 10 and 11 of the GDS Label (first field in the database file header) */
#ifdef VMS
#  define GDS_V20	"02"
#  define GDS_X21	"03"
#  define GDS_V23	"04"
#  define GDS_V24	"05"
#  define GDS_V25	"06"
#  define GDS_V254	"07"
#  define GDS_V255	"08"
#  define GDS_V30	"09"
#  define GDS_V40	"10"
#  define GDS_V50	"11"
#else
#  define GDS_V40	"02"
#  define GDS_V50	"03"
#endif
#define GDS_CURR	GDS_V50
#define MAX_DB_VER_LEN		2
#define GDSVCURR ((enum db_ver)(GDSVLAST - 1))

/* Database major version as an enum quantity. Used to index the dbversion table in mtables.c */
enum db_ver
{
	GDSNOVER = -1,
	GDSV4 = 0,
	GDSV5 = 1,
	GDSV6 = 1, /*GDSV5 and GDSV6 have same value because block format is same for these two version*/
	GDSVLAST
};

#define GDSMVCURR ((enum mdb_ver)(GDSMVLAST - 1))

/* Database minor version as an enum quantity. This is an ever increasing number that may skip actual
 * releases as it is only added to when a file-header field is added or changed or if there is a
 * significant API difference in the version. This number can be incremented only by adding an entry
 * at the end, just before GDSMVLAST. Note these entries need corresponding updates in
 * db_auto_upgrade.c.
 */
#define ENUM_ENTRY(ENUMNAME)	ENUMNAME
enum mdb_ver
{
#include "gdsdbver_sp.h"
};
#undef ENUM_ENTRY

#endif
