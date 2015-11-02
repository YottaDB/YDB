/****************************************************************
 *								*
 *	Copyright 2010, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef TRIGGER_PARSE_PROTOS_INCLUDED
#define TRIGGER_PARSE_PROTOS_INCLUDED

STATICFNDCL char *scan_to_end_quote(char *ptr, int len, int max_len);
STATICFNDCL boolean_t process_dollar_char(char **src_ptr, int *src_len, boolean_t have_star, char **d_ptr, int *dst_len);
STATICFNDCL boolean_t process_delim(char *delim_str, uint4 *delim_len);
STATICFNDCL boolean_t process_options(char *option_str, uint4 option_len, boolean_t *isolation, boolean_t *noisolation,
				    boolean_t *consistency, boolean_t *noconsistency);
STATICFNDCL boolean_t process_subscripts(char *subscr_str, uint4 *subscr_len, char **next_str, char *out_str, int4 *out_max);
STATICFNDCL boolean_t process_pieces(char *piece_str, uint4 *piece_len);
STATICFNDCL boolean_t process_xecute(char *xecute_str, uint4 *xecute_len, boolean_t multi_line);

boolean_t check_trigger_name(char *name_str, uint4 *name_len);
boolean_t trigger_parse(char *input, uint4 input_len, char *trigvn, char **values, uint4 *value_len, int4 *max_len,
			boolean_t *multi_line_xecute);
#endif /* TRIGGER_PARSE_PROTOS_INCLUDED */
