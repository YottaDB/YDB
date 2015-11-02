/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MU_TRIG_DELETE_PROTOS_H_INCLUDED
#define MU_TRIG_DELETE_PROTOS_H_INCLUDED

STATICFNDCL void cleanup_trigger_hash(char *trigvn, int trigvn_len, char **values, uint4 *value_len, stringkey *set_hash,
		stringkey *kill_hash, boolean_t del_kill_hash, int match_index);
STATICFNDCL void cleanup_trigger_name(char *trigvn, int trigvn_len, char *trigger_name, int trigger_name_len);
STATICFNDCL int4 update_trigger_hash_value(char *trigvn, int trigvn_len, char **values, uint4 *value_len, stringkey *set_hash,
					   stringkey *kill_hash, int old_trig_index, int new_trig_index);
STATICFNDCL int4 update_trigger_name_value(int trigvn_len, char *trig_name, int trig_name_len, int new_trig_index);

boolean_t trigger_delete_name(char *trigger_name, uint4 trigger_name_len, uint4 *trig_stats);
int4 trigger_delete(char *trigvn, int trigvn_len, mval *trigger_count, int index);
void trigger_delete_all(void);

#endif
