/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __BG_UPDATE_H__
#define __BG_UPDATE_H__

enum cdb_sc bg_update(cw_set_element *cs, trans_num ctn, trans_num effective_tn, sgm_info *si);

enum cdb_sc bg_update_phase1(cw_set_element *cs, trans_num ctn, sgm_info *si);
enum cdb_sc bg_update_phase2(cw_set_element *cs, trans_num ctn, trans_num effective_tn, sgm_info *si);

#endif
