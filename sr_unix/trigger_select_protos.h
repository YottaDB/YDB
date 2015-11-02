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

STATICFNDEF void write_subscripts(char **out_ptr, char **sub_ptr, int *sub_len);
STATICFNDEF void write_out_trigger(char *gbl_name, unsigned short gbl_name_len, unsigned short file_name_len, mval *op_val,
				   int nam_indx);
STATICFNDEF void write_gbls_or_names(char *gbl_name, unsigned short gbl_name_len, unsigned short file_name_len, mval *op_val,
				     boolean_t trig_name);
STATICFNDEF void dump_all_triggers(unsigned short file_name_len, mval *op_val);

boolean_t trigger_select(char *select_list, uint4 select_list_len, char *file_name, uint4 file_name_len);

#endif /* TRIGGER_SELECT_PROTOS_INCLUDED */
