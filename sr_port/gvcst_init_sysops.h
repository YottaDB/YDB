/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __GVCST_INIT_SYSOPS_H__
#define __GVCST_INIT_SYSOPS_H__

gd_region *dbfilopn (gd_region *reg);
void dbsecspc(gd_region *reg, sgmnt_data_ptr_t csd);
void db_init(gd_region *reg, sgmnt_data_ptr_t tsd);
void db_auto_upgrade(gd_region *reg);

#endif
