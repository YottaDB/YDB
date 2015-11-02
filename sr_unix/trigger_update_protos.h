/****************************************************************
 *								*
 *	Copyright 2010, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef TRIGGER_UPDATE_PROTOS_H_INCLUDED
#define TRIGGER_UPDATE_PROTOS_H_INCLUDED

STATICFNDCL boolean_t check_unique_trigger_name(char *trigvn, int trigvn_len, char *trigger_name, uint4 trigger_name_len);
STATICFNDEF boolean_t trigger_already_exists(char *trigvn, int trigvn_len, char **values, uint4 *value_len, int *set_index,
					     int *kill_index, boolean_t *set_cmp_result, boolean_t *kill_cmp_result,
					     boolean_t *full_match, stringkey *set_trigger_hash, stringkey *kill_trigger_hash,
					     mval *setname, mval *killname);
STATICFNDCL int4 modify_record(char *trigvn, int trigvn_len, char add_delete, int trigger_index, char **values, uint4 *value_len,
			       mval *trigger_count, boolean_t set_compare, boolean_t kill_compare, stringkey *kill_hash,
			       stringkey *set_hash);
STATICFNDCL int4 gen_trigname_sequence(char *trigvn, int trigvn_len, mval *trigger_count, char *trigname_seq_str,
				       uint4 seq_len);
STATICFNDCL int4 add_trigger_hash_entry(char *trigvn, int trigvn_len, char *cmd_value, int trigindx, boolean_t add_kill_hash,
					stringkey *kill_hash, stringkey *set_hash);
STATICFNDCL int4 add_trigger_cmd_attributes(char *trigvn, int trigvn_len,  int trigger_index, char *trig_cmds, char **values,
					    uint4 *value_len, boolean_t set_compare, boolean_t kill_compare, stringkey *kill_hash,
					    stringkey *set_hash);
STATICFNDCL int4 add_trigger_options_attributes(char *trigvn, int trigvn_len, int trigger_index, char *trig_options, char **values,
						uint4 *value_len);
STATICFNDCL boolean_t subtract_trigger_cmd_attributes(char *trigvn, int trigvn_len, char *trig_cmds, char **values,
						      uint4 *value_len, boolean_t set_cmp, stringkey *kill_hash,
						      stringkey *set_hash);
STATICFNDCL boolean_t subtract_trigger_options_attributes(char *trigvn, int trigvn_len, char *trig_options, char *option_value);
STATICFNDCL boolean_t validate_label(char *trigvn, int trigvn_len);
STATICFNDCL int4 update_commands(char *trigvn, int trigvn_len, int trigger_index, char *new_trig_cmds, char *orig_db_cmds);
STATICFNDCL int4 update_options(char *trigvn, int trigvn_len, int trigger_index, char *trig_options, char *option_value);
STATICFNDCL int4 update_trigger_name(char *trigvn, int trigvn_len, int trigger_index, char *db_trig_name, char *tf_trig_name,
				     uint4 tf_trig_name_len);
STATICFNDCL boolean_t trigger_update_rec_helper(char *trigger_rec, uint4 len, boolean_t noprompt, uint4 *trig_stats);
boolean_t trigger_update_rec(char *trigger_rec, uint4 len, boolean_t noprompt, uint4 *trig_stats, io_pair *trigfile_device,
			     int4 *record_num);
boolean_t trigger_update(char *trigger_rec, uint4 len);
#endif
