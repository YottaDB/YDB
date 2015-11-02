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

#ifndef TRIGGER_SELECT_PROTOS_INCLUDED
#define TRIGGER_SELECT_PROTOS_INCLUDED

STATICFNDCL void write_subscripts(char *out_rec, char **out_ptr, char **sub_ptr, int *sub_len);
STATICFNDCL void write_out_trigger(char *gbl_name, uint4 gbl_name_len, uint4 file_name_len, mval *op_val, int nam_indx);
STATICFNDCL void write_gbls_or_names(char *gbl_name, uint4 gbl_name_len, uint4 file_name_len, mval *op_val, boolean_t trig_name);
STATICFNDCL void dump_all_triggers(uint4 file_name_len, mval *op_val);

boolean_t trigger_select(char *select_list, uint4 select_list_len, char *file_name, uint4 file_name_len);

#endif /* TRIGGER_SELECT_PROTOS_INCLUDED */
