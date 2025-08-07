/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef BG_UPDATE_H_INCLUDED
#define BG_UPDATE_H_INCLUDED

enum cdb_sc bg_update_phase1(cw_set_element *cs, trans_num ctn);
enum cdb_sc bg_update_phase2(cw_set_element *cs, trans_num ctn, trans_num effective_tn);

#endif
