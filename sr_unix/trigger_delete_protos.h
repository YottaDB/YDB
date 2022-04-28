/****************************************************************
 *								*
 * Copyright (c) 2010-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MU_TRIG_DELETE_PROTOS_H_INCLUDED
#define MU_TRIG_DELETE_PROTOS_H_INCLUDED

void cleanup_trigger_hash(char *trigvn, int trigvn_len, char **values, uint4 *value_len, stringkey *set_hash,
		stringkey *kill_hash, boolean_t del_kill_hash, int match_index);
void cleanup_trigger_name(char *trigvn, int trigvn_len, char *trigger_name, int trigger_name_len);

boolean_t trigger_delete_name(mval *trigger_rec, uint4 *trig_stats);
int4 trigger_delete(char *trigvn, int trigvn_len, mval *trigger_count, int index);
void trigger_delete_all(mval *trigger_rec, uint4 *trig_stats);

#endif
