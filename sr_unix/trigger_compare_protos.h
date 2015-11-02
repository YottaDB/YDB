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

#ifndef TRIGGER_COMPARE_PROTOS_H_INCLUDED
#define TRIGGER_COMPARE_PROTOS_H_INCLUDED

STATICFNDCL boolean_t compare_vals(char *trigger_val, uint4 trigger_val_len, char *key_val, uint4 key_val_len);

void build_kill_cmp_str(char *trigvn, int trigvn_len, char **values, uint4 *value_len, mstr *kill_key, boolean_t multi_line);
void build_set_cmp_str(char *trigvn, int trigvn_len, char **values, uint4 *value_len, mstr *set_key, boolean_t multi_line);

boolean_t search_trigger_hash(char *trigvn, int trigvn_len, stringkey *trigger_hash, int trig_indx, int *hash_indx);
boolean_t search_triggers(char *trigvn, int trigvn_len, char **values, uint4 *value_len, stringkey *trigger_hash, int *hash_indx,
	int *trig_indx, int match_index, boolean_t doing_set);
#endif
