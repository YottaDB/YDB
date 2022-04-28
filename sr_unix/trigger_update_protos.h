/****************************************************************
 *								*
 * Copyright (c) 2010-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2021-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef TRIGGER_UPDATE_PROTOS_H_INCLUDED
#define TRIGGER_UPDATE_PROTOS_H_INCLUDED

boolean_t trigger_name_search(char *trigger_name, uint4 trigger_name_len, mval *val, gd_region **found_reg);
boolean_t check_unique_trigger_name_full(char **values, uint4 *value_len, mval *val, boolean_t *new_match,
				char *trigvn, int trigvn_len, stringkey *kill_trigger_hash, stringkey *set_trigger_hash);
boolean_t trigger_update_rec(mval *trigger_rec, boolean_t noprompt, uint4 *trig_stats, io_pair *trigfile_device,
			     int4 *record_num);

boolean_t trigger_update(mval *trigger_rec);
#endif
