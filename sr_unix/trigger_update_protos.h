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

#ifndef TRIGGER_UPDATE_PROTOS_H_INCLUDED
#define TRIGGER_UPDATE_PROTOS_H_INCLUDED

STATICFNDCL boolean_t check_unique_trigger_name(char *trigvn, int trigvn_len, char *trigger_name, unsigned short trigger_name_len);
STATICFNDCL boolean_t trigger_already_exists(char *trigvn, int trigvn_len, char **values, unsigned short *value_len, int *trig_indx,
					     boolean_t *big_cmp_result, boolean_t *little_cmp_result, boolean_t *full_match);
STATICFNDCL int4 modify_record(char *trigvn, int trigvn_len, char p_m_d, int trigger_index, char **values,
			       unsigned short *value_len, mval trigger_count, boolean_t big_compare, boolean_t little_compare);
STATICFNDCL int4 gen_trigname_sequence(char *trigvn, int trigvn_len, mval *trigger_count, char *trigname_seq_str,
				       unsigned short seq_len);
STATICFNDCL int4 add_trigger_hash_entry(char *trigvn, int trigvn_len, char **values, unsigned short *value_len, int trigindx,
					boolean_t add_little_hash, uint4 *little_hash, uint4 *big_hash);
STATICFNDCL int4 add_trigger_cmd_attributes(char *trigvn, int trigvn_len,  int trigger_index, char *trig_cmds, char **values,
					    unsigned short *value_len, boolean_t big_compare, boolean_t little_compare);
STATICFNDCL int4 add_trigger_options_attributes(char *trigvn, int trigvn_len, int trigger_index, char *trig_options, char **values,
						unsigned short *value_len);
STATICFNDCL boolean_t subtract_trigger_cmd_attributes(char *trigvn, int trigvn_len, char *trig_cmds, char **values,
						      unsigned short *value_len, boolean_t big_cmp);
STATICFNDCL boolean_t subtract_trigger_options_attributes(char *trigvn, int trigvn_len, char *trig_options, char *option_value);
STATICFNDCL void build_little_cmp_str(char *trigvn, int trigvn_len, char **values, unsigned short *value_len, char *little_str,
				      uint4 *little_len);
STATICFNDCL void build_big_cmp_str(char *trigvn, int trigvn_len, char **values, unsigned short *value_len, char *big_str,
				   uint4 *big_len);
STATICFNDCL boolean_t compare_vals(char *trigger_val, uint4 trigger_val_len, char *key_val, uint4 key_val_len);
STATICFNDCL boolean_t validate_label(char *trigvn, int trigvn_len);
STATICFNDCL int4 update_commands(char *trigvn, int trigvn_len, int trigger_index, char *new_trig_cmds, char *orig_db_cmds);
STATICFNDCL int4 update_options(char *trigvn, int trigvn_len, int trigger_index, char *trig_options, char *option_value);
STATICFNDCL int4 update_trigger_name(char *trigvn, int trigvn_len, int trigger_index, char *db_trig_name, char *tf_trig_name,
				     unsigned short tf_trig_name_len);
STATICFNDCL boolean_t trigger_update_rec_helper(char *trigger_rec, uint4 len, boolean_t noprompt, uint4 *trig_stats);

boolean_t trigger_update_rec(char *trigger_rec, uint4 len, boolean_t noprompt, uint4 *trig_stats);
boolean_t trigger_update(char *trigger_rec, uint4 len);
#endif
